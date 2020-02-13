#include <unistd.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    int i = 0;
    int count = 10000;
    if (argc == 2) {
        count = atoi(argv[1]);
    }

    if (count == -1) {
        while (1);
    }

    while (1) {
        i++;
        if (i >= count) {
            i = 0;
            usleep(400);
        }
    }
}
