#include <unistd.h>

int main(int argc, char** argv) {

    if (argc == 1) {
        write(STDOUT_FILENO, "1\n", 2);
    } else if (argc == 2) {
        write(STDOUT_FILENO, "2\n", 2);
    } else {
        write(STDOUT_FILENO, "3+\n", 3);
    }

}
