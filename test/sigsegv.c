#include <signal.h>

int main() {
    char buf[1024];
    read(0, buf, sizeof(buf));
    printf("%s", buf);
    raise(SIGSEGV);
}