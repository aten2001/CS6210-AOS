#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#define PAGE_SIZE 4096
#define TIME_SLEEP 500
#define QUICK_TEM 65536 /* 512MB */ 

int main(int argc, char **argv)
{
        if (argc != 2) {
            exit(-1);
        }
        else if (strcmp(argv[1], "A") == 0){
            
        }
        void *ptr = NULL;
        void *nptr = NULL;
        size_t i = 1;
        while (1) {
                if (i >= QUICK_TEM && strcmp(argv[1], "A") == 0) {
                    break;
                }
                nptr = realloc(ptr, PAGE_SIZE * i);
                if (nptr) {
                        ptr = nptr;
                        ((char *)ptr)[PAGE_SIZE * (i-1)] = 'a';
                        i++;
                }
                else {
                        break;
                }
                usleep(TIME_SLEEP);
        }
        printf("REACH MAX\n");
        free(ptr);
        return 0;
}
