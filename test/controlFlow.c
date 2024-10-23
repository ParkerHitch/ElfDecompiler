#include <unistd.h>

int main(int argc, char** argv) {

    for (int i=1; i<argc; i++) {
        int val = 0;
        int digits = 0;
        int j = 0;
        char c = argv[i][0];

        // while(c && c >= '0' && c <= '9') {
        while(c) {
            val *= 10;
            val += c - '0';
            digits++;
            c = argv[i][++j];
        }

        val *= 2;

        do {
            int div = 1;
            for (int k=0; k<digits; k++) {
                div *= 10;
            }
            digits--;

            int temp = val / div;
            temp = temp % 10;

            temp += '0';

            write(STDOUT_FILENO, &temp, 1);

        } while (digits >= 0);
        write(STDOUT_FILENO, "\n", 1);
    }
    return 0;
}
