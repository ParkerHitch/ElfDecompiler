#include <unistd.h>

int main(void) {

    int val1 = 20;
    int val2 = 2 * val1;

    val2 += 30;
    val2 -= val1 * 2;
    val1 += val1 / 2;

    if (val1 == val2) {
        char msg[] = "Hello!";
        write(STDOUT_FILENO, msg, sizeof(msg)-1);
    } else {
        char msg[] = "Goodbye!";
        write(STDOUT_FILENO, msg, sizeof(msg)-1);
    }

    for (int i=0; i<10; i++) {
        char out = i + 'a';
        write(STDOUT_FILENO, &out, 1);
    }
}
