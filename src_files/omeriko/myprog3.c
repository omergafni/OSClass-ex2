#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv){

    char* line = NULL;
    size_t len;
    
    getline(&line, &len, stdin);
    printf("%s\n", line);
    fflush(stdout);
    return 0;
    
}
