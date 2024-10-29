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
#define TEMP_FILE "/tmp/pid_temp.txt" // Same path as in shell code
#define MAX_SIZE 250

int front = 0,rear = 0;
int num_CPU,TSLICE;

volatile sig_atomic_t read_pipe = 1;

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

// Function to handle SIGSTOP signal
void handle_sigstop(int sig) {
    printf("Received SIGSTOP, terminating all managed processes...\n");
    for (int i = 0; i < MAX_SIZE; i++) {
        if (ready_queue[i] > 0) {
            kill(ready_queue[i], SIGKILL); // Terminate the process
        }
    }
    exit(EXIT_SUCCESS); // Exit the scheduler
}

void handle_sigusr(int signo){
    write(STDOUT_FILENO, "SIGUSR\n", 7);  // Avoids printf buffering issues

    // Handle PIDs sent from the shell
    // int fd = open(SCHEDULER_PIPE, O_RDONLY);

    // if(fd<0){
    //     printf("ERROR OPENING PIPE [SCHEDULER]\n");
    //     exit(EXIT_FAILURE);
    // }

    // pid_t pid;
    // while (read(fd, &pid, sizeof(pid)) > 0) { // Read the PID from the pipe
    //     // printf("\nProcess %d is ready to start\n", pid);
    //     enqueue(pid); // Add to ready queue
    // }
    // close(fd);

    FILE *file = fopen(TEMP_FILE, "r");
    if (file != NULL) {
        int pid;
        fscanf(file, "%d", &pid); // Read the PID from the file
        printf("Received PID: %d\n", pid);
        enqueue(pid);
        fclose(file);

        // Here you can add logic to manage/process the received PID

        // Optionally remove the temp file after reading
        remove(TEMP_FILE);
    } else {
        printf("No new PIDs found.\n");
    }
}

// Function to check if a process has finished
int check_process_status(pid_t pid) {
    int status;
    pid_t result = waitpid(pid, &status, WNOHANG); // Non-blocking check

    if (result == -1) {
        return 1;
    } else if (result == 0) {
        // Process is still running
        return 0; // Process is still running
    } else {
        // Process has finished
        if (WIFEXITED(status)) {
            printf("Process %d finished with exit status %d\n", pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Process %d was terminated by signal %d\n", pid, WTERMSIG(status));
        }
        return 1; // Process has finished
    }
}

void start_scheduler() {
    signal(SIGTERM, handle_sigstop);

    struct timespec req = { .tv_sec = 0, .tv_nsec = TSLICE * 1000000L };

    // Remove the FIFO if it already exists
    unlink(SCHEDULER_PIPE);

    // Create the FIFO pipe once
    if (mkfifo(SCHEDULER_PIPE, 0666) == -1) {
        printf("Failed to create FIFO\n");
        exit(EXIT_FAILURE);
    }

    while (1) {
        pid_t active_processes[num_CPU];
        int count = 0;

        // Fetch NCPU processes from the ready queue
        for (int i = 0; i < num_CPU && front != rear; i++) {
            

            pid_t pid = dequeue();
            if (pid > 0) {
                kill(pid, SIGCONT); // Start the process
                printf("--------\nPID:%d  STARTED\n--------\n",pid);
                active_processes[count++] = pid;
            }
        }

        // Wait for the timeslice to expire
        nanosleep(&req, NULL);

        // Stop and re-queue each active process
        for (int i = 0; i < count; i++) {

            if(kill(active_processes[i],0)==0){    
                kill(active_processes[i], SIGSTOP); // Pause the process
                printf("----------\nPID:%d  STOPPED\n--------\n",active_processes[i]);
                enqueue(active_processes[i]);       // Re-add to the rear of the queue
            }else{
                printf("----------\nProcess with PID:%d finished execution\n------------\n",active_processes[i]);
            }

        }
    }
}

int main(int argc,char** argv) {

    signal(SIGUSR1,handle_sigusr);

    for(int i =0;i<MAX_SIZE;i++){
        ready_queue[i]=-1;
    }

    num_CPU = atoi(argv[1]);
    TSLICE = atoi(argv[2]);

    start_scheduler(num_CPU,TSLICE);
    return 0;
}