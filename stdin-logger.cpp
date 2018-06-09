#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/reg.h>
#include <sys/syscall.h>
#include <libgen.h>
#include <signal.h>
#include <errno.h>

#include <string>
#include <iostream>
#include <fstream>
#include <time.h>

#if _WIN64 || __amd64__ // 64 bits CPU
#define ARCH_BYTES 8
#else // 32 bits CPU
#define M32 1
#define ARCH_BYTES 4
#endif

#define ALLOC_BUF(size) (malloc((count / sizeof(unsigned long)) * sizeof(unsigned long) + 1))

class Logger {
private:
    std::string basename;
    std::string buf;

public:
    Logger(char *b);
    void append(char *x, long int count);
    void save();
};

Logger::Logger(char *b) {
    basename.append(b);
}

void Logger::append(char *x, long int count) {
    if (count > 0) {
        buf.append(x, count);
    }
}

void Logger::save() {

    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);

    char time_stamp[32];
    strftime (time_stamp, sizeof(time_stamp), "%F-%H%M%S", timeinfo);
    char file_name[64];
    sprintf(file_name, "%s.%s.stdin.log", basename.c_str(), time_stamp);

    std::ofstream logfile(file_name);
    logfile << buf;
    logfile.close();
}

void read_remote_process_memory(pid_t pid, unsigned long int addr, unsigned long int count, void* result) {
    for (unsigned long int i = 0; i < count; i += sizeof(unsigned long)) {
        long data = ptrace(PTRACE_PEEKDATA, pid, addr + i, NULL);
        // printf("data = %x\n", data);
        if (data == -1) {
            fprintf(stdout, "[!] failed to peek data\n");
            return;
        }
        *((unsigned long *)((char *) result + i)) = (unsigned long) data;
    }
}

void *logger = NULL;

void crash_handler(int signo) {
    ((Logger *)logger)->save();
    exit(1);
}

void usage(char *argv[]) {
    printf("usage: %s TRACEE [TRACEE args...]\n", argv[0]);
    // puts("env[\"ENABLE_ALARM\"]\tSet some value to disable blocking SIGALRM");
    exit(0);
}

int main(int argc, char *argv[]) {

    if (argc == 1) {
        usage(argv);
    }

    pid_t child;
    unsigned long int orig_rax, rax;
    unsigned long int params[6];
    int status;
    int insyscall = 0;

    bool DEBUG = false;

    if (std::getenv("DEBUG")) {
        DEBUG = true;
    }

    auto l = new Logger(basename(argv[1]));
    logger = (void *) l;

    // Handle SIGINT (Ctrl+C) (and child process signals?)
    signal(SIGINT, crash_handler);
    signal(SIGABRT, crash_handler);
    signal(SIGBUS, crash_handler);
    signal(SIGTERM, crash_handler);

    child = fork();
    if(child == 0) {
        // unblock some signals
        // if (std::getenv("ENABLE_ALARM")) {
        //     puts("[*] Enabling SIGALARM");
        //     sigset_t sig_mask;
        //     sigemptyset(&sig_mask); /* initialize mask set */
        //     sigaddset(&sig_mask, SIGALRM);
        //     sigprocmask(SIG_UNBLOCK, &sig_mask, NULL);
        // }

        // start tracee
        #ifdef M32
        ptrace(PTRACE_TRACEME, 0, ARCH_BYTES * ORIG_EAX, NULL);
        #else
        ptrace(PTRACE_TRACEME, 0, ARCH_BYTES * ORIG_RAX, NULL);
        #endif
        auto res = execvp(argv[1], &argv[1]);
        if (res) {
            perror("execvp");
        }
    }
    else {
        if (DEBUG) {
            printf("[*] DEBUG mode enabled\n");
        }
        while(1) {
            wait(&status);

            if(WIFEXITED(status))
                break;

            // Handle SIGALRM
            // siginfo_t siginfo;
            // ptrace(PTRACE_GETSIGINFO, child, NULL, &siginfo);
            // if (siginfo.si_signo != SIGTRAP) {
            //     printf("sig number = %d\n", siginfo.si_signo);
            // }
            // if (siginfo.si_signo == SIGALRM) {
            //     ptrace(PTRACE_SETSIGINFO, child, NULL, &siginfo); // BUG: SIGALRM is not sent correctly
            //     // ptrace(PTRACE_CONT, child, NULL, &siginfo);
            //     continue;
            // }

            // %rax System call %rdi    %rsi    %rdx    %r10    %r8 %r9
            #ifdef M32
            orig_rax = ptrace(PTRACE_PEEKUSER, child, ARCH_BYTES * ORIG_EAX, NULL);
            rax = ptrace(PTRACE_PEEKUSER, child, ARCH_BYTES * EAX, NULL);
            params[0] = ptrace(PTRACE_PEEKUSER, child, ARCH_BYTES * EBX, NULL);
            params[1] = ptrace(PTRACE_PEEKUSER, child, ARCH_BYTES * ECX, NULL);
            params[2] = ptrace(PTRACE_PEEKUSER, child, ARCH_BYTES * EDX, NULL);
            #else
            orig_rax = ptrace(PTRACE_PEEKUSER, child, ARCH_BYTES * ORIG_RAX, NULL);
            rax = ptrace(PTRACE_PEEKUSER, child, ARCH_BYTES * RAX, NULL);
            params[0] = ptrace(PTRACE_PEEKUSER, child, ARCH_BYTES * RDI, NULL);
            params[1] = ptrace(PTRACE_PEEKUSER, child, ARCH_BYTES * RSI, NULL);
            params[2] = ptrace(PTRACE_PEEKUSER, child, ARCH_BYTES * RDX, NULL);
            #endif

            if(orig_rax == SYS_read) {
                if(insyscall == 0) { /* Syscall entry */
                    insyscall = 1;
                    // printf("enter: sys_read(%ld, %p, %ld)\n",
                    //        params[0], params[1], params[2]);
                }
                else { /* Syscall exit */
                    unsigned long int count = params[2];
                    char* buf = (char *) ALLOC_BUF(count);
                    read_remote_process_memory(child, params[1], count, (void *) buf);
                    buf[count] = '\0';
                    if (DEBUG) printf("sys_read(%ld, %p '%s', %ld) = %ld\n",
                           params[0], params[1], buf, params[2], rax);
                    if (params[0] == 0 && (long int) rax > 0) {
                        l->append(buf, rax);
                    }
                    free(buf);
                    insyscall = 0;
                }
            }

            else if(orig_rax == SYS_write) {
                unsigned long int count = params[2];
                char* buf = (char *) ALLOC_BUF(count);
                read_remote_process_memory(child, params[1], count, (void *) buf);
                buf[count] = '\0';
                if(insyscall == 0) { /* Syscall entry */
                    insyscall = 1;
                    // printf("sys_write(%ld, %p '%s', %ld)\n",
                    //        params[0], params[1], buf, params[2]);
                }
                else { /* Syscall exit */
                    if (DEBUG) printf("sys_write(%ld, %p '%s', %ld) = %ld\n",
                        params[0], params[1], buf, params[2], rax);
                    insyscall = 0;
                }
                free(buf);
            }

            // else if(orig_rax == SYS_stat) {
            //     if(insyscall == 0) { /* Syscall entry */
            //         insyscall = 1;
            //         unsigned long int count = 32;
            //         char* buf = (char *) ALLOC_BUF(count);
            //         read_remote_process_memory(child, params[0], count, (void *) buf);
            //         buf[count] = '\0';
            //         printf("sys_stat(%p '%s', %ld) = %ld\n",
            //                params[0], buf, params[1], rax);
            //         // if (params[0] == 0 && (long int) rax > 0) {
            //         //     l->append(buf, rax);
            //         // }
            //         free(buf);
            //         // printf("enter: sys_read(%ld, %p, %ld)\n",
            //         //        params[0], params[1], params[2]);
            //     }
            //     else { /* Syscall exit */
            //         insyscall = 0;
            //     }
            // }

            else if(orig_rax == SYS_exit) {
                if (DEBUG) printf("sys_exit(%ld)\n", params[0]);
                l->save();
                break;
            }

            else if(orig_rax == SYS_exit_group) {
                if (DEBUG) printf("sys_exit_group(%ld)\n", params[0]);
                l->save();
                break;
            }

            else if(orig_rax == SYS_ioprio_get) {
                if (DEBUG) printf("sys_ioprio_get(%ld, %ld)\n", params[0], params[1]);
                l->save();
                break;
            }

            else {
                if (DEBUG) printf("[!] Unhandled syscall %d\n", orig_rax);
            }

            ptrace(PTRACE_SYSCALL, child, NULL, NULL);
        }
    }
    return 0;
}