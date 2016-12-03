#include <stdio.h>
#include <stdlib.h>
#include "s2e.h"

int main()
{
    int a = 2;

    s2e_disable_forking();
    s2e_make_concolic(&a, sizeof(a), "scraxf");

    if(a > 0)
        printf("'a' > 0\n");
    else
        printf("'a' < 0\n");

    s2e_kill_state(0, "testing program end");

    return 0;
}
