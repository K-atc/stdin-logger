#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    char buf[1024];
    if (argc == 2) {
        read(0, buf, atoi(argv[1]) + 1);
        printf("%s", buf);
    }
    return 0;
}