#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main() {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return 1;
    }

    if (pid == 0) {
        // ребенок
        printf("Ребенок: PID = %d. выход\n", getpid());
        exit(0);
    } else {
        //  родитель
        printf("Родитель: PID = %d \n", getpid());
        printf("Родитель: задержка без wait()\n");
        printf(" ps aux | grep Z\n");
        
        sleep(30); 

        printf("Родитель: вызов wait()\n");
        wait(NULL); 
        printf("Родитель: выход\n");
    }

    return 0;
}