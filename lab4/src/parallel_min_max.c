#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

// Глобальные переменные для таймаута
int timeout_sec = -1;
pid_t *global_child_pids = NULL;
int global_pnum = 0;

// Обработчик сигнала SIGALRM
void handle_alarm(int sig) {
  if (global_child_pids != NULL) {
    for (int i = 0; i < global_pnum; i++) {
      if (global_child_pids[i] > 0) {
        kill(global_child_pids[i], SIGKILL);
      }
    }
  }
}

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  bool with_files = false;

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {
        {"seed", required_argument, 0, 0},
        {"array_size", required_argument, 0, 0},
        {"pnum", required_argument, 0, 0},
        {"by_files", no_argument, 0, 'f'},
        {"timeout", required_argument, 0, 't'}, // Добавили опцию таймаута
        {0, 0, 0, 0}};

    int option_index = 0;
    // В optstring добавили "t:"
    int c = getopt_long(argc, argv, "ft:", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            seed = atoi(optarg);
            if (seed <= 0) {
              printf("seed must be a positive number\n");
              return 1;
            }
            break;
          case 1:
            array_size = atoi(optarg);
            if (array_size <= 0) {
              printf("array_size must be a positive number\n");
              return 1;
            }
            break;
          case 2:
            pnum = atoi(optarg);
            if (pnum <= 0) {
              printf("pnum must be a positive number\n");
              return 1;
            }
            break;
          case 3:
            with_files = true;
            break;
          case 4:
            timeout_sec = atoi(optarg);
            if (timeout_sec <= 0) {
              printf("timeout must be a positive number\n");
              return 1;
            }
            break;

          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;
      case 'f':
        with_files = true;
        break;
      case 't':
        timeout_sec = atoi(optarg);
        if (timeout_sec <= 0) {
          printf("timeout must be a positive number\n");
          return 1;
        }
        break;

      case '?':
        break;

      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (optind < argc) {
    printf("Has at least one no option argument\n");
    return 1;
  }

  if (seed == -1 || array_size == -1 || pnum == -1) {
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--by_files] [--timeout \"num\"]\n",
           argv[0]);
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  GenerateArray(array, array_size, seed);

  // Создаём pipe'ы ДО fork, только если не используем файлы
  int pipes[pnum][2];
  if (!with_files) {
    for (int i = 0; i < pnum; i++) {
      if (pipe(pipes[i]) == -1) {
        printf("pipe failed\n");
        return 1;
      }
    }
  }

  // Массив для хранения PID'ов детей
  pid_t child_pids[pnum];
  global_child_pids = child_pids;
  global_pnum = pnum;

  // Настраиваем таймер, если он задан
  if (timeout_sec > 0) {
    signal(SIGALRM, handle_alarm);//на сигал вызываем хэндл
    alarm(timeout_sec);//включаем таймер
  }

  int active_child_processes = 0;

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  for (int i = 0; i < pnum; i++) {
    pid_t child_pid = fork();
    if (child_pid >= 0) {
      child_pids[i] = child_pid; // сохраняем PID
      active_child_processes += 1;
      
      if (child_pid == 0) {
        // дочерний процесс
        unsigned int begin = i * (array_size / pnum);
        unsigned int end = (i + 1) * (array_size / pnum);
        if (i == pnum - 1) end = array_size;

        struct MinMax local = GetMinMax(array, begin, end);

        if (with_files) {
          char filename[64];
          sprintf(filename, "result_%d.txt", i);
          FILE *f = fopen(filename, "w");
          if (f) {
            fprintf(f, "%d %d\n", local.min, local.max);
            fclose(f);
          }
        } else {
          for (int j = 0; j < pnum; j++) {
            close(pipes[j][0]);
            if (j != i) close(pipes[j][1]);
          }
          write(pipes[i][1], &local, sizeof(struct MinMax));
          close(pipes[i][1]);
        }
        return 0;
      }
    } else {
      printf("Fork failed!\n");
      return 1;
    }
  }

  // Родитель: закрыть все пишущие концы pipe'ов
  if (!with_files) {
    for (int i = 0; i < pnum; i++) {
      close(pipes[i][1]);
    }
  }

  // Неблокирующий wait с WNOHANG
  while (active_child_processes > 0) {
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);//ждем любого ребенка, не спим
    
    if (pid > 0) {
      // Один ребенок завершился
      active_child_processes -= 1;
      // Помечаем, что этот ребенок больше не активен
      for (int i = 0; i < pnum; i++) {
        if (child_pids[i] == pid) {
          child_pids[i] = -1;
          break;
        }
      }
    } else if (pid == 0) {
      // Дети еще работают, ждем немного
      usleep(10000); // 10 миллисекунд
    } else {
      // Ошибка (например, все дети убиты)
      break;
    }
  }

  // Отключаем будильник, если родитель успел до таймаута
  if (timeout_sec > 0) {
    alarm(0);
  }

  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;
    bool success = false;

    // Читаем только если процесс не был убит (child_pids[i] != -1)
    if (with_files) {
      char filename[64];
      sprintf(filename, "result_%d.txt", i);
      FILE *f = fopen(filename, "r");
      if (f) {
        if (fscanf(f, "%d %d", &min, &max) == 2) success = true;
        fclose(f);
        remove(filename);
      }
    } else {
      struct MinMax local;
      // read вернет сколько данных в пайпе
      ssize_t bytes = read(pipes[i][0], &local, sizeof(struct MinMax));
      if (bytes == sizeof(struct MinMax)) {
        min = local.min;
        max = local.max;
        success = true;
      }
      close(pipes[i][0]);
    }

    if (success) {
      if (min < min_max.min) min_max.min = min;
      if (max > min_max.max) min_max.max = max;
    }
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);

  // Если таймаут сработал, min_max может остаться дефолтным
  if (min_max.min == INT_MAX && min_max.max == INT_MIN) {
    printf("Timeout reached or no data collected!\n");
  } else {
    printf("Min: %d\n", min_max.min);
    printf("Max: %d\n", min_max.max);
  }
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}