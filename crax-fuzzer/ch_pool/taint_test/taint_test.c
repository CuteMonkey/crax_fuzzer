#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//test function: strcpy, memset
int main(int argc, char *argv[])
{
    if(argc != 2) return -1;
    
    char input[10] = {'0'};
    int a = 0;
    
    strcpy(input, argv[1]);
    a = atoi(input);
    
    if(0 < a && a <= 9) memset(input, 'x', a);
    
    printf("%s\n", input);

    return 0;
}
