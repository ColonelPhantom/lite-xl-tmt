#include <pty.h>
#include <unistd.h>
#include <sys/wait.h>
#include <poll.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#define BUFFER_SIZE 256

int main(int argc, char **argv) {
    int master;

    pid_t pid = forkpty(&master, NULL, NULL, NULL);
    if(pid == -1) {
        // forkpty failed
        int err = errno;
        printf("Could not forkpty (%i)\n", err);
        return errno;
    }
    if (pid != 0) {
        struct pollfd fds[2] = {
            { .fd = master, .events = POLLIN },
            { .fd = STDIN_FILENO, .events = POLLIN },
        };

        char buffer[BUFFER_SIZE];
        size_t length;
        int status;

        while (1) {
            poll(&fds[0], 2, -1);
            if (fds[1].revents & POLLIN) {
                length = read(STDIN_FILENO, &buffer[0], BUFFER_SIZE);
                write(master, &buffer[0], length);
            }
            if (fds[0].revents & POLLIN) {
                length = read(master, &buffer[0], BUFFER_SIZE);
                write(STDOUT_FILENO, &buffer[0], length);
            }

            if (fds[0].revents & POLLHUP || fds[1].revents & POLLHUP) {
                waitpid(pid, &status, 0);
                printf("Terminated cleanly");
                return WEXITSTATUS(status);
            }

            if (fds[0].revents & POLLERR || fds[1].revents & POLLERR) {
                printf("crashed!");
                 return 1;
            }
        }
    } else {
        // child
        setenv("TERM", "ansi", 1);
        printf("launching %s", argv[1]);
        execvp(argv[1], &argv[1]);
        return errno;
    }
}
