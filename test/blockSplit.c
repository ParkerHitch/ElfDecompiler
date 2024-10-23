#include <unistd.h>

int main(void) {

    int counter = 5;
    int specialVar = 20;
    char out[] = "a\n";

    specialVar*=10;

    do {
        out[0] = 'a' + specialVar;
        specialVar += counter;

        write(STDOUT_FILENO, out, 2);

        counter--;
    } while(counter >= 0);

    return 0;
}
