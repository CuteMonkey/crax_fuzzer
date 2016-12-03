#include <stdio.h>
#include <string.h>
#include <unistd.h>

// ./symarg argv_index args ...
int main(int argc, char **argv)
{

  if(argc < 3)
  {
    printf("args error\n");
    return 0;
  }
  int index = atoi(argv[1])+2;

  int pid = fork();
  if(pid == 0 )
  {
    setenv("LD_PRELOAD", "./tlib.so", 1);
    execvp(argv[2], argv+2);
  }
  else
  {
    wait();
  }

  return 0;
}
