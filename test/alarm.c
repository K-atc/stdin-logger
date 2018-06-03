#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

void timeup() {
    puts("Time Up!");
    exit(1);
}

int main() {
    signal(SIGALRM, timeup);
    alarm(1);

    char buf[1024];
    read(0, buf, sizeof(buf));
    printf("%s", buf);
}