#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {

    if (argc != 2){
        printf("Please provide exactly 1 arg: the .elf you'd like to decompile.\n");
        return -1;
    }

    char* elfName = argv[1];

    if (!memcmp(".elf", &elfName[strlen(elfName)-4], 4)){
        printf("File not .elf\n");
        return -1;
    }

    printf("Hello, Decomp!\n");
    return 0;
}
