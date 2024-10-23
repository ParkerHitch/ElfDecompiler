#include <unistd.h>

int main(int argc, char** argv) {

    int i=0;
    int j=i;

    if (i>0) {
        write(STDOUT_FILENO, "Larger!\n", 8);
    } else {
        write(STDOUT_FILENO, "Smaller\n", 8);
        j++;
    }

    if (j > i) {
        write(STDOUT_FILENO, "Yep Small\n", 10);
    }
    

    return 0;
}
