#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <getopt.h>
#include <sys/time.h>

#include "sum_lib.h"
#include "utils.h" 

// Обертка для потока
void *ThreadSum(void *args) {
  struct SumArgs *sum_args = (struct SumArgs *)args;
  // Приводим int к void*, чтобы вернуть из потока
  return (void *)(size_t)Sum(sum_args);
}

int main(int argc, char **argv) {
  uint32_t threads_num = 0;
  uint32_t array_size = 0;
  uint32_t seed = 0;

  while (true) {
    static struct option options[] = {
        {"threads_num", required_argument, 0, 't'},
        {"seed", required_argument, 0, 's'},
        {"array_size", required_argument, 0, 'a'},
        {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "t:s:a:", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 't':
        threads_num = atoi(optarg);
        break;
      case 's':
        seed = atoi(optarg);
        break;
      case 'a':
        array_size = atoi(optarg);
        break;
      default:
        printf("Usage: %s --threads_num \"num\" --seed \"num\" --array_size \"num\"\n", argv[0]);
        return 1;
    }
  }

  if (threads_num <= 0 || array_size <= 0 || seed <= 0) {
    printf("Usage: %s --threads_num \"num\" --seed \"num\" --array_size \"num\"\n", argv[0]);
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  GenerateArray(array, array_size, seed);

  pthread_t threads[threads_num];
  struct SumArgs args[threads_num];

  for (uint32_t i = 0; i < threads_num; i++) {
    args[i].array = array;
    args[i].begin = i * (array_size / threads_num);
    args[i].end = (i + 1) * (array_size / threads_num);
    if (i == threads_num - 1) {
      args[i].end = array_size; //последний поток забирает остаток
    }
  }

  struct timeval start_time, finish_time;
  gettimeofday(&start_time, NULL);

  for (uint32_t i = 0; i < threads_num; i++) {
    if (pthread_create(&threads[i], NULL, ThreadSum, (void *)&args[i])) {
      printf("Error: pthread_create failed!\n");
      return 1;
    }
  }

  int total_sum = 0;
  for (uint32_t i = 0; i < threads_num; i++) {
    int sum = 0;
    pthread_join(threads[i], (void **)&sum);
    total_sum += sum;
  }

  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);
  
  printf("Total: %d\n", total_sum);
  printf("Elapsed time: %fms\n", elapsed_time);
  
  return 0;
}