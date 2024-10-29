#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#define SCHEDULER_PIPE "/tmp/scheduler_pipe" // The same pipe name
#define MAX_SIZE 250

int front = 0,rear = 0;
int num_CPU,TSLICE;

pid_t ready_queue[MAX_SIZE];

void enqueue(pid_t pid) {
    if ((rear + 1) % MAX_SIZE == front) {
        printf("Queue is full\n");

        if(kill(pid,SIGKILL)==-1){
            printf("Couldn't kill Process with PID:%d\n",pid);
            exit(EXIT_FAILURE);
        }

        printf("Process with PID:%d\n Killed",pid);

        return;
    }
    ready_queue[rear] = pid;
    rear = (rear + 1) % MAX_SIZE;
}

pid_t dequeue() {
    if (front == rear) {
        printf("Queue is empty\n");
        return -1;
    }
    pid_t pid = ready_queue[front];
    front = (front + 1) % MAX_SIZE;
    return pid;
}


void start_scheduler() {
    mkfifo(SCHEDULER_PIPE, 0666); // Create named pipe if it doesn’t exist

    while (1) {
        int fd = open(SCHEDULER_PIPE, O_RDONLY);
        if (fd < 0) {
            printf("Error: Could not open scheduler pipe\n Closing Scheduler\n");
            exit(EXIT_FAILURE);
        }
        
        int pid;
        while (read(fd, &pid, sizeof(pid)) > 0) { // Read the PID from the pipe
            printf("Process %d is ready to start\n", pid);
            // Signal to continue execution as per your scheduling strategy
            kill(pid, SIGCONT);
            // Add logic to send SIGSTOP after a time slice or other scheduler requirements
        }
        close(fd);
    }
}

int main(int argc,char** argv) {

    num_CPU = atoi(argv[1]);
    TSLICE = atoi(argv[2]);

    start_scheduler(num_CPU,TSLICE);
    return 0;
}
