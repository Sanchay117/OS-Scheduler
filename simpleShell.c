#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

#define MAX_ARGS 64
#define MAX_COMMANDS 20000
#define MAX_PROCESS 250

#define SCHEDULER_PIPE "/tmp/scheduler_pipe" // FOR IPC
#define TEMP_FILE "/tmp/pid_temp.txt" // Define the path for the temporary file
#define TEMP_PID_FILE "/tmp/pid_temp.txt"
#define TEMP_RESPONSE_FILE "/tmp/response_temp.txt"

char** history[MAX_COMMANDS];
int history_ptr = 0;

pid_t scheduler_PID;

struct CommandDetails {
    char* command;
    pid_t pid;
    struct timeval start_time;
    struct timeval end_time;
    double duration;  // Duration in seconds
    int status;
};

struct Process{
    pid_t pid;
    char* name;
    int completion_time;
    int waiting_time;
};

struct Process processes[MAX_COMMANDS];
int process_pointer = 0;
struct CommandDetails commandDetails[MAX_COMMANDS];
int process_ptr = 0;

int num_CPU,TSLICE;

int submit(char** inp,char* command);

void add_to_history(char* command, pid_t pid, struct timeval start, struct timeval end,int status) {
    double duration = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

    commandDetails[process_ptr].command = strdup(command);
    commandDetails[process_ptr].pid = pid;
    commandDetails[process_ptr].start_time = start;
    commandDetails[process_ptr].end_time = end;
    commandDetails[process_ptr].duration = duration;
    commandDetails[process_ptr].status = status;
    process_ptr++;
}

void sig_child_handler(int sig){
    exit(0);
}

char* read_user_input(){
    char* cmnd;
    size_t len = 0;
    size_t read;
    read = getline(&cmnd, &len, stdin);
    
    if (read == -1) {
        printf("getline failed\n");
        free(cmnd);
        return "echo Some Error Occurred";
    }

    return cmnd;
}

// work on from here

void remove_trailing_spaces(char *str) {
    int len = strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t')) {
        len--;
    }
    while(&str==' ')str++;
    str[len] = '\0';
}

void remove_leading_spaces(char *str) {
    char *start = str; // Pointer to the beginning of the string

    // Move the pointer forward until a non-space character is found
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    if (start != str) {
        memmove(str, start, strlen(start) + 1); // Include the null terminator
    }
}

char** split_command_space(char* command){
    // Determine command type length
    int len_cmnd_type = 0;
    for (int i = 0; (command[i] != ' ' && command[i] != '\0'); i++) {
        len_cmnd_type++;
    }

    int spaces = 0;
    for (int i = 0; command[i] != '\0'; i++) { 
        if (command[i] == ' ') spaces++;
        if (i>0 && command[i-1]==' ' && command[i]==' ') spaces--;
        else if(i==0 && command[i]==' ')spaces--;
    }

    char *command_type = malloc(len_cmnd_type + 1);
    if (command_type == NULL) {
        printf("Malloc failed\n");
        exit(1);
    }

    // Copy command type
    for (int i = 0; i < len_cmnd_type; i++) {
        command_type[i] = command[i];
    }
    command_type[len_cmnd_type] = '\0';

    // Extract arguments
    int num_args = spaces + 1;
    char** args = malloc((num_args+1)*sizeof(char*));
    if (args == NULL) {
        printf("Malloc failed\n");
        exit(1);
    }
    
    int ptr = 0;
    for (int i = 0; i < num_args; i++) {
        int ptr_ptr = ptr;
        int len_word = 0;

        // Count the length of the current word
        while (command[ptr_ptr] != '\0') {
            if (command[ptr_ptr] == ' ' && len_word!=0) {
                break;
            }else if(command[ptr_ptr]==' ' && len_word==0){
                ptr_ptr++;
                continue;
            }
            len_word++;
            ptr_ptr++;
        }

        char* word = malloc(len_word + 1); // Allocate memory for the word + null terminator
        if (word == NULL) {
            printf("Malloc failed\n");
            return 0;
        }

        // Fill the word array
        for (int j = 0; j < len_word; j++) {
            while(command[ptr]==' '){
                ptr++;
            }
            word[j] = command[ptr++];
        }
        word[len_word] = '\0'; // Null-terminate the string
        args[i] = word; // Store the word in args

        // Skip spaces for the next word
        while (command[ptr] == ' ') ptr++;
        if (command[ptr] == '\n') ptr++; // Skip newline if present
    }
    args[num_args] = NULL; // Null-terminate the args array

    return args;
}

char** split_command(char *command) {
    // split by |

    int pipes = 0;

    for(int i = 0;command[i]!='\0';i++){
        if(command[i]=='|') pipes++;
        if (i>0 && command[i-1]=='|' && command[i]=='|') pipes--;
        else if(i==0 && command[i]=='|')pipes--;
    }

    char** args=malloc((pipes+2)*(sizeof(char*)));
    if(args==NULL){
        printf("Malloc Failed\n");
        return 0;
    }
    int num_args = pipes+1;
    
    int ptr = 0;
    for (int i = 0; i < num_args; i++) {
        int ptr_ptr = ptr;
        int len_cmnd = 0;

        // Count the length of the current word
        while (command[ptr_ptr] != '\0') {
            if (command[ptr_ptr] == '|') {
                break;
            }
            len_cmnd++;
            ptr_ptr++;
        }

        char* word = malloc(len_cmnd + 1); // Allocate memory for the word + null terminator
        if (word == NULL) {
            printf("Malloc failed\n");
            return 0;
        }

        // Fill the word array
        for (int j = 0; j < len_cmnd; j++) {
            word[j] = command[ptr++];
        }
        word[len_cmnd] = '\0'; // Null-terminate the string
        args[i] = word; // Store the word in args

        // Skip spaces for the next word
        while (command[ptr] == '|') ptr++;
        if (command[ptr] == '\n') ptr++; // Skip newline if present
    }

    args[pipes+1] = NULL;

    return args;


}

bool hang(char** args){
    char* x = strdup(args[0]);
    if((strcmp(x,"cat")==0 || strcmp(x,"sort")==0 || strcmp(x,"uniq")==0 || strcmp(x,"wc")==0) && args[1]==NULL){
        return true;
    }

    if(strcmp(x,"wc")==0 && args[2]==NULL) return false;

    return false;
}

int piped_process(char* command){
    // Assume Input Is Sanitized
    
    int status;
    char** args = split_command(command);

    // for(int i = 0;args[i]!=NULL;i++){
    //     printf("%s\n",args[i]);
    // }
    
    int num_commands = 0;
    
    // Count the number of commands
    while (args[num_commands] != NULL) {
        num_commands++;
    }

    // printf("%d",num_commands);

    int pipe_fd[2];
    int pid;
    int fds = 0; // Variable to keep track of the previous command's output

    struct timeval start_time, end_time;

    gettimeofday(&start_time, NULL);  // Record start time

    for (int i = 0; i < num_commands; i++) {
        char* comand = strdup(args[i]);
        remove_leading_spaces(comand);
        remove_trailing_spaces(comand);
        char** exec = split_command_space(comand);

        if (i < num_commands - 1) {
            // Create a pipe for all but the last command
            if (pipe(pipe_fd) == -1) {
                perror("Pipe Error");
                free(comand); // Free dynamically allocated memory
                return 0;
            }
        }

        pid = fork();
        if(pid==-1){
            printf("Fork Failure\n");
            free(comand);
            return 0;
        }else if(pid==0){

            signal(SIGINT,sig_child_handler); // ignore sig_int
            // Child
            if (fds != 0) {
                dup2(fds, STDIN_FILENO);  // Get input from the previous command's output
                close(fds); // Close the old input
            }

            if (i < num_commands - 1) {
                dup2(pipe_fd[1], STDOUT_FILENO); // Output to the pipe
                close(pipe_fd[0]); // Close unused read end
            }

            if (i==0 && hang(exec)) {
                // No arguments are passed to cat, so we close stdin
                close(STDIN_FILENO);  // Close stdin to avoid hanging
                printf("Invalid Command\n");
                exit(EXIT_FAILURE);
            }

            if (execvp(exec[0], exec) == -1) {
                printf("Invalid Command\n");
                exit(EXIT_FAILURE);
            }

        }else{
            // Parent
            if (fds != 0) {
                close(fds); // Close previous input pipe if it exists
            }

            if (i < num_commands - 1) {
                close(pipe_fd[1]); // Close write end of the current pipe
            }

            fds = pipe_fd[0]; // Save read end for the next command
            waitpid(pid, &status, 0); // Wait for the child process
            gettimeofday(&end_time, NULL);  // Record end time

            // Check if the command executed successfully
            int success = WIFEXITED(status) && WEXITSTATUS(status) == 0;

            // Store details in history
            add_to_history(command, pid, start_time, end_time,success);
        }
        free(comand); // Free dynamically allocated memory for the command
        free(exec); // Free the exec array if dynamically allocated
    }

    return 0;

}

int create_process_and_run(char* command){
    remove_leading_spaces(command);
    remove_trailing_spaces(command);

    bool pipe = false;

    for(int i = 0;command[i]!='\0';i++){
        if(command[i]=='|') {
            pipe = true;
            // printf("KAKA");
        }
    }

    if(pipe){
        printf("Sorry Piping Not Allowed\n");
        // printf("%d",status);
        return 0;
    }

    char** inp = split_command_space(command);

    if(strcmp(inp[0],"submit") == 0){
        int result = submit(inp,command);
        return result;
    }

    history[history_ptr] = strdup(command);
    history_ptr++;

    char* str = "history";
    if(strcmp(str,command)==0) {
        history_ptr--;
        int i = 0;
        while(i<history_ptr){
            printf("%s\n",history[i]);
            i++;
        }
        return 0;
    }

    int pid;
    int status;

    pipe = false;

    for(int i = 0;command[i]!='\0';i++){
        if(command[i]=='|') pipe = true;
    }

    if(pipe){
        status = piped_process(command);
        // printf("%d",status);
        return status;
    }

    // Determine command type length
    int len_cmnd_type = 0;
    for (int i = 0; (command[i] != ' ' && command[i] != '\0'); i++) {
        len_cmnd_type++;
    }

    int spaces = 0;
    for (int i = 0; command[i] != '\0'; i++) { 
        if (command[i] == ' ') spaces++;
        if (i>0 && command[i-1]==' ' && command[i]==' ') spaces--;
        else if(i==0 && command[i]==' ')spaces--;
    }

    char *command_type = malloc(len_cmnd_type + 1);
    if (command_type == NULL) {
        printf("malloc failed\n");
        return 0;
    }

    // Copy command type
    for (int i = 0; i < len_cmnd_type; i++) {
        command_type[i] = command[i];
    }
    command_type[len_cmnd_type] = '\0';

    // Extract arguments
    int num_args = spaces + 1;
    char** args = malloc((num_args+1)*sizeof(char*));
    if (args == NULL) {
        printf("Malloc failed\n");
        return 0;
    }
    
    int ptr = 0;
    for (int i = 0; i < num_args; i++) {
        int ptr_ptr = ptr;
        int len_word = 0;

        // Count the length of the current word
        while (command[ptr_ptr] != '\0') {
            if (command[ptr_ptr] == ' ' && len_word!=0) {
                break;
            }else if(command[ptr_ptr]==' ' && len_word==0){
                ptr_ptr++;
                continue;
            }
            len_word++;
            ptr_ptr++;
        }

        char* word = malloc(len_word + 1); // Allocate memory for the word + null terminator
        if (word == NULL) {
            printf("Malloc failed\n");
            return 0;
        }

        // Fill the word array
        for (int j = 0; j < len_word; j++) {
            while(command[ptr]==' '){
                ptr++;
            }
            word[j] = command[ptr++];
        }
        word[len_word] = '\0'; // Null-terminate the string
        args[i] = word; // Store the word in args

        // Skip spaces for the next word
        while (command[ptr] == ' ') ptr++;
        if (command[ptr] == '\n') ptr++; // Skip newline if present
    }
    args[num_args] = NULL; // Null-terminate the args array

    struct timeval start_time, end_time;

    gettimeofday(&start_time, NULL);  // Record start time

    bool amp = false;
    if(command_type[0]=='&') amp = true;
    if(amp){
        for(int i = 0;command_type[i]!='\0';i++){
           command_type[i]=command_type[i+1];
        }
        int len = strlen(command_type);
        command_type[len] = '\0';
    }

    // Fork and execute the command
    pid = fork();
    if (pid == 0) {
        // Child Process
        signal(SIGINT,sig_child_handler); // ignore sig_int

        if (hang(args)) {
            printf("Invalid Command\n");
            return 0;
        }

        if (execvp(command_type, args) == -1) {
            printf("Invalid Command\n");
            return 1;
        }
    } else if (pid < 0) {
        printf("Fork Failed\n");
        return 0;
    } else {
        if(!amp) waitpid(pid, &status, 0);
        else{
            printf("Executing Process in Background With PID:%d\n",pid);
            status = 0;
        }
        gettimeofday(&end_time, NULL);
            
        // Check if the command executed successfully
        int success = WIFEXITED(status) && WEXITSTATUS(status) == 0;
        if(success == 1) success=0;
            
        add_to_history(command, pid, start_time, end_time,success);
    }

    // Free allocated memory
    free(command_type);
    for (int i = 0; i < num_args; i++) {
        free(args[i]);
    }
    free(args);

    return status;
}

// code all good below this line

int launch(char* command){
    int status;
    status = create_process_and_run(command);
    // printf("STATUS %d\n",status);
    return status;
}

void shell_loop(){
    int status;
    do {
        printf("@User:~$: ");
        char* command = read_user_input();

        if(command[0]=='\n'){
            status = 0;
            continue; // user just pressed enter
        }
        
        // Note : command also has a new line character at the end
        // so we remove it
        command[strlen(command)-1] = '\0';
        
        // printf("%s",command);
        // exit(1);

        status = launch(command);
        if(status==1 && process_ptr>0){
            commandDetails[process_ptr-1].status = 1;
            status = 0;
        }
        free(command);
    }while(status==0);
}

void exit_shell(){
    printf("\nExiting!\n");
    printf("-----------------------------------\n");
    printf("Process History:\n");
    printf("-----------------------------------\n");
    // for (int i = 0; i < process_ptr; i++) {
    //     struct CommandDetails cmd = commandDetails[i];

    //     // Convert start time to a readable format
    //     struct tm* start_tm = localtime(&cmd.start_time.tv_sec);
    //     char start_str[30];
    //     strftime(start_str, 30, "%Y-%m-%d %H:%M:%S", start_tm);

    //     printf("Command: %s\n", cmd.command);
    //     printf("PID: %d\n", cmd.pid);
    //     printf("Start Time: %s\n", start_str);
    //     printf("Duration: %.8f seconds\n", cmd.duration);
    //     char* stat = "SUCCESS";
    //     if(cmd.status==1) stat = "FAIL";
    //     // printf("Status: %s\n",stat);
    //     printf("-----------------------------------\n");
    // }

    for(int i  = 0;i<process_pointer;i++){
        printf("Name: %s\n",processes[i].name);
        printf("PID: %d\n",processes[i].pid);
        printf("Completion Time [ms] : %d\n",processes[i].completion_time);
        printf("Waiting Time [ms] : %d\n",processes[i].waiting_time);
        printf("-----------------------------------\n");
    }

    // int status;
    // int pid = waitpid(scheduler_PID,&status,0);

    // if(WIFEXITED(status)) {
    //     printf("%d Exit =%d\n",pid,WEXITSTATUS(status));
    // } else if (WIFSIGNALED(status)) {
    //     printf("Child was terminated by signal: %d\n", WTERMSIG(status));
    // } else {
    //     printf("Abnormal termination of %d\n", scheduler_PID);
    // }

    exit(0);
}

void sigHandler_usr(int sig){
    // printf("SHELL SIGHANDLER\n");
    // Read PID from temp file
    FILE *pid_file = fopen(TEMP_PID_FILE, "r");
    if (pid_file == NULL) {
        perror("Error opening PID temp file");
        return;
    }

    int pid,turns,arrivalTurn,bursts;
    fscanf(pid_file, "%d", &pid);
    fscanf(pid_file,"%d",&turns);
    fscanf(pid_file,"%d",&arrivalTurn);
    fscanf(pid_file,"%d",&bursts);

    printf("TURNS [shell]:%d\n",turns);

    // printf("READ PID:%d\n",pid);

    int ptr = 0;
    for(int i = 0;i<process_pointer;i++){
        if(processes[i].pid==pid){
            ptr = i;
            break;
        }
    }

    fclose(pid_file);

    // Check if process has finished
    int status;
    pid_t result = waitpid(pid, &status, WNOHANG); // Non-blocking check

    FILE *response_file = fopen(TEMP_RESPONSE_FILE, "w");
    
    if (result == 0) {
        fprintf(response_file, "0\n");
    } else if (result == -1) {
        fprintf(response_file, "-1\n");
    } else {
        processes[ptr].completion_time = TSLICE*turns;
        processes[ptr].waiting_time = (turns-arrivalTurn)*TSLICE - bursts*TSLICE;
        fprintf(response_file, "1\n");
    }
    
    fclose(response_file);
}

int submit(char** inp,char* command){

    int length = 0;
    while (inp[length] != NULL) {
        length++;
    }

    if(length<2){
        printf("Invalid Number Of Args\n");
        return 0;
    }

    char** args = (char**)malloc(sizeof(char*)*(length));
    
    for (int i = 1; i < length; i++) {
        args[i - 1] = strdup(inp[i]);
    }
    args[length - 1] = NULL; // Add NULL as the last element for execvp

    char* path = inp[1];

    if (access(path, F_OK) == -1) {
        printf("File does not exist.\n");
        return 0;
    }

    // Check if the file is executable
    if (access(path, X_OK) == -1) {
        printf("File is not an executable.\n");
        return 0;
    }

    pid_t pid = fork();
    if(pid<0){
        printf("ERROR! FORK FAILED\n");
        exit(EXIT_FAILURE);
    }

    // printf("PID:SCHEDULER %d\n",scheduler_PID);

    if(pid==0){
        // child
        // printf("------SUBMITTED NEW PROC WITH ID:%d-------\n",getpid());
        kill(getpid(), SIGSTOP);
        execvp(path, args);
        printf("Error: exec failed\n");
        exit(EXIT_FAILURE);
    }else{

        // printf("Submitted new process with PID: %d\n", pid);

        processes[process_pointer].name = strdup(command);
        processes[process_pointer].pid = pid;
        processes[process_pointer].completion_time = 0;
        process_pointer++;

        // Write the PID to a temporary file
        FILE *file = fopen(TEMP_FILE, "w");
        if (file == NULL) {
            perror("Error opening temp file");
            exit(EXIT_FAILURE);
        }
        
        fprintf(file, "%d\n", pid); // Write the PID to the file
        fclose(file); // Close the file

        kill(scheduler_PID,SIGUSR1);

    }

    // Free the allocated memory
    for (int i = 0; i < length - 1; i++) {
        free(args[i]);
    }
    free(args);

    return 0;
}

int main(int argc,char** argv){
    signal(SIGUSR1,sigHandler_usr);
    signal(SIGINT, exit_shell);

    if(argc!=3){
        printf("Invalid Number of arguments!\n");
        exit(1);
    }

    num_CPU = atoi(argv[1]);
    TSLICE = atoi(argv[2]);

    int available_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (num_CPU > available_cores) {
        printf("This machine doesn't have that many available cores\n");
        exit(1);
    }

    if(TSLICE>500 || TSLICE<=0){
        printf("Provided Time Quantum Either way too large or way too small\n");
        exit(1);
    }

    scheduler_PID = fork();

    if(scheduler_PID<0){
        printf("Fork failed\n");
        exit(EXIT_FAILURE);
    }else if(scheduler_PID==0){
        // child

        char** scheduler_ARGS = (char**)malloc(sizeof(char*)*(argc+1));
        scheduler_ARGS[0] = strdup("./scheduler");
        int i = 1;
        while(i<argc){
            scheduler_ARGS[i] = strdup(argv[i]);
            i++;
        }

        scheduler_ARGS[argc] = NULL;

        execvp(scheduler_ARGS[0],scheduler_ARGS);

        printf("Couldn't launch Scheduler\n");
        exit(EXIT_FAILURE);

    }else{

        printf("SimpleScheduler Started with PID:%d\n\n",scheduler_PID);
        printf("Scheduler initialized with %d CPU cores and time slice of %d ms\n\n", num_CPU, TSLICE);

        // printf("Welcome To Our Shell!\n");
        shell_loop();

    }
    
    
    return 0;
}