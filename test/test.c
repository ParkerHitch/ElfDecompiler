#include <unistd.h>

int main(int argc, char** argv) {
    int var_36 = argc;
    char** var_48 = argv;
    int var_4 = 1;
    while((var_4 < var_36)) {
        int var_8 = 0;
        int var_12 = 0;
        int var_16 = 0;
        char var_17 = argv[var_4][0];
        while((var_17 != 0)) {
            var_8 = ((((var_8 << 2) + var_8) + ((var_8 << 2) + var_8)) + (var_17 - 48));
            var_12 = (var_12 + 1);
            var_16 = (var_16 + 1);
            var_17 = argv[var_4][var_16];
        }
        var_8 = (var_8 << 1);
        do {
            int var_24 = 1;
            int var_28 = 0;
            while((var_28 < var_12)) {
                var_24 = (((var_24 << 2) + var_24) + ((var_24 << 2) + var_24));
                var_28 = (var_28 + 1);
            }
            var_12 = (var_12 - 1);
            int var_32 = (((var_8 / var_24) - ((((((((var_8 / var_24) * 1717986919) >> 32) >> 2) - ((var_8 / var_24) >> 31)) << 2) + (((((var_8 / var_24) * 1717986919) >> 32) >> 2) - ((var_8 / var_24) >> 31))) + (((((((var_8 / var_24) * 1717986919) >> 32) >> 2) - ((var_8 / var_24) >> 31)) << 2) + (((((var_8 / var_24) * 1717986919) >> 32) >> 2) - ((var_8 / var_24) >> 31))))) + 48);
            write(1, &var_32, 1);
        } while(((var_12 - 1) > 0));
        write(1, "\n", 1);
        var_4 = (var_4 + 1);
    }
    return 0;
}
