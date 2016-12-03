#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "s2e.h"

// ./symarg argv_index args ...
int main(int argc, char **argv)
{

  if(argc < 3)
  {
    printf("args error\n");
    return 0;
  }
  s2e_disable_forking();
  int index = atoi(argv[1])+2;
  s2e_make_concolic(argv[index], strlen(argv[index]), "crax");

  int pid = fork();
  if(pid == 0 )
  {
    setenv("LD_PRELOAD", "./tlib.so", 1);
    execvp(argv[2], argv+2);
  }
  else
  {
    wait();
    s2e_kill_state(0, "Program terminated");
  }

  return 0;
}
