#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#define SCHEDULER_PIPE "/tmp/scheduler_pipe" // The same pipe name

void start_scheduler(int n,int slice) {
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

    int num_CPU = atoi(argv[1]);
    int TSLICE = atoi(argv[2]);

    start_scheduler(num_CPU,TSLICE);
    return 0;
}
