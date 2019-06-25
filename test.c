#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>//Para poder usar a função isdigit()

int main () {
    char str[80] = "D 0 2540 -22.2218 -54.8064";
    const char s[2] = " ";
    char *parte;
    int x=0;

    /* get the first parte */
    parte = strtok(str, s);

    if(parte[0] != 'D' && parte[0] != 'P'){
        exit(0);
    }
    printf("%s\n",parte);
    parte = strtok(NULL, s);
    if(parte[0] != '0' && parte[0] != '1' && parte[0] != '2'){
        exit(0);
    }
    printf("%s\n",parte);
    parte = strtok(NULL,s);
    while(parte[x] != '\0'){
        if(!isdigit(parte[x])){
            exit(0);
        }
        x++;
    }
    x = 0;
    printf("%s\n",parte);
    parte = strtok(NULL,s);
    while(parte[x] != '\0'){
        if(parte[x] != '-' && parte[x] != '.'){
            if(!isdigit(parte[x])){
                exit(0);
            }
        }
        x++;
    }
    x = 0;
    printf("%s\n",parte);
    parte = strtok(NULL,s);
    while(parte[x] != '\0'){
        if(parte[x] != '-' && parte[x] != '.'){
            if(!isdigit(parte[x])){
                exit(0);
            }
        }
        x++;
    }
    printf("%s\n",parte);
    puts("Final");

    return 0;
}