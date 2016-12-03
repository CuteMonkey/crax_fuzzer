#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "s2e.h"

// ./symargs symbolic_index target target_args...
int main(int argc, char *argv[])
{
    if(argc < 3) {
        perror("E: Args too few.\n");
        return -1;
    }

    int index = atoi(argv[1]);

    s2e_disable_forking();
    s2e_make_concolic(argv[index], strlen(argv[index]), "scrax");

    int wstatus;
    pid_t fid = fork();
    if(fid == 0) {
        setenv("LD_PRELOAD", "./tlib.so", 1);
        execv(argv[2], argv + 2);
    } else if(fid == -1) {
        perror("E: Fork failed.\n");
        return -1;
    } else {
        wait(&wstatus);
        s2e_kill_state(0, "Program terminated");
    }

    return 0;
}
