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
#define TEMP_FILE2 "/tmp/pid_temp2.txt" // Same path as in shell code
#define TEMP_PID_FILE "/tmp/pid_temp.txt"
#define TEMP_RESPONSE_FILE "/tmp/response_temp.txt"
#define MAX_SIZE 250

int front = 0,rear = 0;
int num_CPU,TSLICE;

int turns = 0;

struct Process {
    pid_t pid;
    int arrivalTurn;
    int bursts;
};

struct Process procs[MAX_SIZE];
int ptr = 0;

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
void handle_sigint(int sig) {
    printf("Received SIGSTOP, terminating all managed processes...\n");
    fflush(stdout);
    for (int i = 0; i < MAX_SIZE; i++) {
        if (ready_queue[i] > 0) {
            kill(ready_queue[i], SIGKILL); // Terminate the process
        }
    }
    exit(EXIT_SUCCESS); // Exit the scheduler
}

void handle_sigusr(int signo){

    FILE *file = fopen(TEMP_FILE, "r");
    if (file != NULL) {
        int pid;
        fscanf(file, "%d", &pid); // Read the PID from the file
        // printf("Received PID: %d\n", pid);
        enqueue(pid);
        fclose(file);

        // Here you can add logic to manage/process the received PID
        procs[ptr].pid = pid;
        procs[ptr].arrivalTurn = turns;
        ptr++;

        // Optionally remove the temp file after reading
        remove(TEMP_FILE);
    } else {
        printf("No new PIDs found.\n");
    }

}

// Function to check if a process has finished
int check_process_status(pid_t pid) {

    // Write PID to temp file
    FILE *pid_file = fopen(TEMP_PID_FILE, "w");
    if (pid_file == NULL) {
        perror("Error opening PID temp file");
        return 1;
    }
    // printf("WRITTEN PID:%d\n",pid);
    fprintf(pid_file, "%d\n", pid);
    fprintf(pid_file,"%d\n",turns);
    printf("TURNS [scheduler]:%d\n",turns);
    
    int arrivalTurn,bursts;
    for(int  i = 0;i<ptr;i++){
        if(procs[i].pid==pid){
            arrivalTurn = procs[i].arrivalTurn;
            bursts = procs[i].bursts;
            break;
        }
    }
    fprintf(pid_file,"%d\n",arrivalTurn);
    fprintf(pid_file,"%d\n",bursts);
    fclose(pid_file);

    kill(getppid(),SIGUSR1);

    // Wait for response from shell
    while (access(TEMP_RESPONSE_FILE, F_OK) == -1) {
        // printf("Waiting for shell to process PID %d...\n", pid);
        // sleep(0.5); // Sleep for a while before checking again
    }

    // Read response from temp response file
    FILE *response_file = fopen(TEMP_RESPONSE_FILE, "r");
    if (response_file == NULL) {
        perror("Error opening response temp file");
        return 1;
    }
    if (response_file != NULL) {
        int status;
        fscanf(response_file, "%d", &status); // Read the status from the file
        // printf("Received status: %d\n", status);
        fclose(response_file);

        remove(TEMP_RESPONSE_FILE);
        remove(TEMP_PID_FILE);

        if(status==-1){
            // printf("An error occured\n");
            // exit(EXIT_FAILURE);
            status = 1;
        }

        return status;
        
    } else {
        printf("No new statu found.\n");
        return 0;
    }

    unlink(TEMP_PID_FILE);
    unlink(TEMP_RESPONSE_FILE);
    
}

void start_scheduler() {

    struct timespec req = { .tv_sec = 0, .tv_nsec = TSLICE * 1000000L };

    while (1) {
        pid_t active_processes[num_CPU];
        int count = 0;

        // Fetch NCPU processes from the ready queue
        for (int i = 0; i < num_CPU && front != rear; i++) {
            

            pid_t pid = dequeue();
            if (pid > 0) {

                for(int i =0;i<ptr;i++){
                    if(procs[i].pid==pid){
                        procs[i].bursts++;
                        break;
                    }
                }

                kill(pid, SIGCONT); // Start the process
                printf("--------\nPID:%d  STARTED\n--------\n",pid);
                active_processes[count++] = pid;
            }
        }

        // Wait for the timeslice to expire
        nanosleep(&req, NULL);
        turns++;

        // Stop and re-queue each active process
        for (int i = 0; i < count; i++) {

            if(check_process_status(active_processes[i])==0){    
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
    signal(SIGINT, handle_sigint);

    for(int i =0;i<MAX_SIZE;i++){
        ready_queue[i]=-1;
    }

    for(int i = 0;i<MAX_SIZE;i++ ){
        procs[i].bursts = 0;
        procs[i].pid = -1;
    }

    num_CPU = atoi(argv[1]);
    TSLICE = atoi(argv[2]);

    start_scheduler(num_CPU,TSLICE);
    return 0;
}