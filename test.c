#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main()
{
    char text[15];
    int x = 50;

    sprintf(text,"%d",x);

    printf("%s\n",text);

    int aux = atoi(text);

    printf("%d\n",aux);

    return 0;
}