#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "s2e.h"

int main(int argc, char **argv)
{
    if(argc < 4)
    {
        printf("args error\n");
        return 0;
    }
    
    int pipe_fd[2];
    pid_t pid;
    
    s2e_disable_forking();
    s2e_make_concolic(argv[1], strlen(argv[1]), "crax");

    if(pipe(pipe_fd)<0)
    {
        printf("pipe error \n");
        exit(1);
    }

    write(pipe_fd[1] , argv[1] , strlen(argv[1]));

    if((pid = fork())==0)
    {
        dup2(pipe_fd[0] , 0);
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        setenv("LD_PRELOAD", "./tlib.so", 1);
        execl(argv[2], argv[2], argv[3], NULL);
    }
    else
    {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        wait();
        s2e_kill_state(0, "program terminated");
    }
    return 0;
}

