#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "s2e.h"

// ./symfile file_path offset length target target_args... 
int main(int argc, char *argv[])
{
    if(argc < 5) {
        perror("E: Args too few.\n");
        return -1;
    }

    int fd;
    int offset = atoi(argv[2]);
    int length = atoi(argv[3]);

    fd = open(argv[1], O_RDWR);
    if(fd < 0) {
        perror("E: File open error.\n");
        return -1;
    }

    struct stat file_stat;
    fstat(fd, &file_stat);

    char *p;
    p = mmap(NULL, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(p == MAP_FAILED) {
        perror("E: Map create error.\n");
        return -1;
    }

    s2e_disable_forking();
    if(offset + length > file_stat.st_size || length <= 0)
        length = file_stat.st_size - offset;
    s2e_make_concolic(p + offset, length, "scrax");

    int wstatus;
    pid_t fid = fork();
    if(fid == 0) {
        setenv("LD_PRELOAD", "./tlib.so", 1);
        execv(argv[4], argv + 4);
    } else if(fid == -1) {
        perror("E: Fork failed.\n");
        return -1;
    } else {
        close(fd);
        wait(&wstatus);
        s2e_kill_state(0, "Program terminated");
    }

    return 0;
}
