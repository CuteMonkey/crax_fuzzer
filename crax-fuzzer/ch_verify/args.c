#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

// ./stdin target target_args...
int main(int argc, char *argv[])
{
    if(argc < 2) {
        perror("E: Args too few.\n");
        return -1;
    }

    int wstatus;
    pid_t fid = fork();
    if(fid == 0) {
        setenv("LD_PRELOAD", "/home/chengte/ch_verify/tlib.so", 1);
        execv(argv[1], argv + 1);
    } else if(fid == -1) {
        perror("E: Fork failed.\n");
        return -1;
    } else {
        wait(&wstatus);
    }

    return 0;
}
