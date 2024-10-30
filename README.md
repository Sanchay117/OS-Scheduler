# OS Simple Scheduler


## Overview

This project implements a simple operating system scheduler that manages processes using a round-robin scheduling algorithm. The scheduler uses signal handling and inter-process communication (IPC) to coordinate with simple shell.

## Features

- **Process Management**: Enqueues and dequeues processes in a ready queue.
- **Signal Handling**: Responds to user-defined signals for managing processes.
- **Error Handling**: Manages errors related to process creation and file operations.
- **History Tracking**: Records process history for executed processes.

## Functions

### 1. `enqueue(pid_t pid)`
- **Purpose**: Adds a process to the end of the `ready_queue`.
- **Parameters**: 
  - `pid`: Process ID to enqueue.
- **Behavior**: If the queue is full, the function kills the process and exits.

### 2. `dequeue()`
- **Purpose**: Removes a process from the front of the `ready_queue`.
- **Return**: Returns `pid` if a process is dequeued, or `-1` if the queue is empty.

### 3. `handle_sigint(int sig)`
- **Purpose**: Handles `SIGINT` signal for terminating the scheduler.
- **Operation**: Iterates through `ready_queue` and terminates each active process using `SIGKILL`.

### 4. `handle_sigusr(int signo)`
- **Purpose**: Handles user-defined signal to enqueue a new process from a temporary file.
- **Behavior**: Reads the PID from `TEMP_FILE`, enqueues it, and updates the Process structure.

### 5. `check_process_status(pid_t pid)`
- **Purpose**: Checks if a process has finished by communicating with SimpleShell.
- **Return**: Returns `0` if the process needs to be re-queued or `1` if it has finished execution.
- **Process**:
  1. Writes the process ID (PID) and turns to `TEMP_PID_FILE`.
  2. Sends a `SIGUSR1` to SimpleShell.
  3. Waits for SimpleShell to write a status response in `TEMP_RESPONSE_FILE`.

### 6. `start_scheduler()`
- **Purpose**: Core scheduler loop that manages processes according to the round-robin policy.
- **Operation**:
  - Fetches up to NCPU processes from the ready queue.
  - Starts each process using `SIGCONT` and waits for TSLICE.
  - Stops each active process using `SIGSTOP` and re-enqueues if unfinished.
  - Increments the turns counter after each TSLICE.

## Signal Handling

- **SIGINT**: Caught by `handle_sigint`, which kills all managed processes and exits.
- **SIGUSR1**: Caught by `handle_sigusr`, which allows new processes to be enqueued from TEMP_FILE.

## Error Handling

- **Queue Overflow**: If the ready queue is full, the enqueued process is killed.
- **File Operations**: Handles potential errors when accessing or removing temporary files like TEMP_FILE, TEMP_RESPONSE_FILE, etc.
- **Invalid Input**: NCPU and TSLICE are validated at runtime.

## Compilation

To compile the project, navigate to the project directory and run:

```bash
gcc simpleShell.c -o simpleShell
gcc scheduler.c -o scheduler
./simpleShell <N_CPU> <TSLICE>
```

## Bonus
We habe also implemented priority scheduling with 4 priorities(1-4) 4 being prioritized first

## Credits
**Sanchay Singh**: Modified shell and wrote the scheduler <br/>
**Arshad Khan**: Wrote the documentation and took care of the error handling