#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

long long fib(int n){
    if (n == 0) return 0;
    else if (n == 1) return 1;
    else return fib(n - 1) + fib(n - 2);
}

void handle_sigint(int sig){
    
}

int main(int argc,char** argv){
    signal(SIGINT, handle_sigint);
    int n, t1 = 0, t2 = 1, nextTerm;

    if(argc!=2){
        printf("Invalid Arguments\n");
        exit(0);
    }

    n = atoi(argv[1]);

    printf("FIB.C STARTED EXECUTING WITH N:%d\n",n);

    long long ans = fib(n);

    printf("FIB.C N:%d FINISHED ANS:%lld\n",n,ans);

    return 0;
}