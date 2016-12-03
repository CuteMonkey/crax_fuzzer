#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>

// ./symstdin symbolic_content target target_other_args... 
int main(int argc, char* argv[])
{
    if(argc < 3) {
        printf("Error: agruments too few.\n");
        exit(EXIT_FAILURE);
    }
    
    int fd[2];
    pid_t pid;
   
    if(pipe(fd) != 0) {
        printf("Error: pipe creation fail.\n");
        exit(EXIT_FAILURE);
    }

    write(fd[1], argv[1], strlen(argv[1]));

    if((pid = fork()) < 0) {
        printf("Error: fork fail.\n");
        exit(EXIT_FAILURE);
    } else if(pid == 0) {
        //child part
        close(fd[1]);
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);

        setenv("LD_PRELOAD", "/home/chengte/ch_verify/tlib.so", 1);
        execl(argv[2], argv[2], argv[3], (char*)NULL);
    } else {
        //parent part
        close(fd[0]);
        close(fd[1]);
        wait();
    }

    return 0;
}
