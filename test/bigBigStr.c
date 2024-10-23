#include <unistd.h>

char* globalLong = "This is a super duper crazy long string which has a length way more than 64 bytes and is also a global variable and is so so cool and fun and I hope they don't do this to me fr\n";

int main(int argc, char** argv) {
    
    write(STDOUT_FILENO, globalLong, 177);

    for (int i=1; i<argc; i++) {
        write(STDOUT_FILENO, argv[i], 1);
    }

    return 0;
}
