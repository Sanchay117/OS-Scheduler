#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#define TEMP_FILE "/tmp/pid_temp.txt" // Same path as in shell code
#define TEMP_PID_FILE "/tmp/tmp.txt"
#define TEMP_RESPONSE_FILE "/tmp/response_temp.txt"

#define MAX_SIZE 250

int terminate = 0;
int proc_count = 0;

int front = 0,rear = 0;
int num_CPU,TSLICE;

int turns = 0;

struct Process {
    pid_t pid;
    int arrivalTurn;
    int bursts;
    int priority;
};

struct Process procs[MAX_SIZE];
int ptr = 0;

volatile sig_atomic_t read_pipe = 1;

pid_t ready_queue[MAX_SIZE];

int getPriority(pid_t pid){
    for(int i = 0;i<ptr;i++){
        if(procs[i].pid==pid){
            return procs[i].priority;
        }
    }
}

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

    // higher priority means first in queue (4 before 1 etc)

    int ind = rear;
    int givenPriority = getPriority(pid);

    // Find the correct position to insert based on priority
    for (int i = front; i != rear; i = (i + 1) % MAX_SIZE) {
        if (getPriority(ready_queue[i]) < givenPriority) {
            ind = i;
            break;
        }
    }

    if(ind==rear){
        ready_queue[rear] = pid;
        rear = (rear + 1) % MAX_SIZE;
        return;
    }

    // Shift processes down to make space for the new process
    for (int i = rear; i != ind; i = (i - 1 + MAX_SIZE) % MAX_SIZE) {
        ready_queue[i] = ready_queue[(i - 1 + MAX_SIZE) % MAX_SIZE];
    }

    // Insert the new process at its correct position
    ready_queue[ind] = pid;
    rear = (rear + 1) % MAX_SIZE; // Update rear pointer
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

void handle_sigint(int sig) {
    terminate=1;
}

void handle_sigusr(int signo){

    FILE *file = fopen(TEMP_FILE, "r");
    if (file != NULL) {
        int pid,priority;
        fscanf(file, "%d", &pid); // Read the PID from the file
        fscanf(file,"%d",&priority);
        // printf("Received Prioir: %d\n", priority);
        enqueue(pid);
        fclose(file);

        // Here you can add logic to manage/process the received PID
        procs[ptr].pid = pid;
        procs[ptr].arrivalTurn = turns;
        procs[ptr].priority = priority;
        ptr++;

        proc_count++;

        // Optionally remove the temp file after reading
        remove(TEMP_FILE);
    } else {
        printf("No new PIDs found.\n");
    }

}

// Function to check if a process has finished
int check_process_status(pid_t pid) {
    unlink(TEMP_PID_FILE);
    unlink(TEMP_RESPONSE_FILE);

    // Write PID to temp file
    FILE *pid_file = fopen(TEMP_PID_FILE, "w");
    if (pid_file == NULL) {
        perror("Error opening PID temp file");
        return 1;
    }
    
    fprintf(pid_file, "%d\n", pid);
    fprintf(pid_file,"%d\n",turns);
    
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

    struct timespec req = { .tv_sec = 0, .tv_nsec = 5 * 1000000L };

    // Wait for response from shell
    while (access(TEMP_RESPONSE_FILE, F_OK) == -1) {
        // printf("Waiting for shell to process PID %d...\n", pid);
        nanosleep(&req,NULL); // IDK WHY BUT WITHOUT THIS LINE CODE NOT WORKING!
    }

    // Read response from temp response file
    FILE *response_file = fopen(TEMP_RESPONSE_FILE, "r");
    if (response_file == NULL) {
        perror("Error opening response temp file");
        return 1;
    }
    if (response_file != NULL) {
        int status=-2;
        fscanf(response_file, "%d", &status); // Read the status from the file
        // printf("Received status: %d\n", status);
        fclose(response_file);

        remove(TEMP_RESPONSE_FILE);
        remove(TEMP_PID_FILE);

        if(status==-1){
            printf("An error occured\n");
            // exit(EXIT_FAILURE);
            status = 0;
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
                // printf("--------\nPID:%d  STARTED\n--------\n",pid);
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
                // printf("----------\nPID:%d  STOPPED\n--------\n",active_processes[i]);
                enqueue(active_processes[i]);       // Re-add to the rear of the queue
            }else{
                proc_count--;
                // printf("----------\nProcess with PID:%d finished execution\n------------\n",active_processes[i]);
            }

        }

        if(terminate == 1 && proc_count==0){
            // printf("SCHEDULER EXITING\n");
            exit(EXIT_SUCCESS);
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