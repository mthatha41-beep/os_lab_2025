#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char **argv) {
  if (argc != 3) {
    printf("Usage: %s seed array_size\n", argv[0]);
    return 1;
  }

  pid_t child_pid = fork();

  if (child_pid < 0) {
    printf("Fork failed!\n");
    return 1;
  }

  if (child_pid == 0) {
    // дочерний процесс: запускаем sequential_min_max
    execl("./sequential_min_max", "sequential_min_max", argv[1], argv[2], NULL);

    // сюда попадаем, только если execl не сработал
    perror("execl failed");
    return 1;
  }

  // родитель: ждёт завершения ребёнка
  int status;
  wait(&status);

  if (WIFEXITED(status)) {
    printf("Child exited with code %d\n", WEXITSTATUS(status));
  }

  return 0;
}