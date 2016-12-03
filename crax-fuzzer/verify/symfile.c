#include <sys/mman.h>
#include<stdio.h>
#include<unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include<malloc.h>
#define LEN 128

// ./symfile filename offset length args ...
int main(int argc, char **argv)
{
   if(argc < 5)
   {
      printf("args error\n");
      return 1;
   }

   int fd;
   char *p;
   int offset = atoi(argv[2]);
   int length = atoi(argv[3]);

   fd = open(argv[1], O_RDWR);
   if (fd<0)
   {
      printf("File open error \n");
      return 1;
   }

   if(fork()==0){
      setenv("LD_PRELOAD", "./tlib.so", 1);
      execvp(argv[4], argv+4);
   }else{
      close(fd);
      wait();
   }
   return 0;
}

