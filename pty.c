#include <pty.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#include "minivt.h"

#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define MAX_PTY_SIZE 999
#define BUFFER_SIZE 2048

void callback(int type, vt_answer_t *ans, void *p) {
    int master = *(int*)p;
    struct winsize wp = { 0 };

    switch (type) {
    // FIXME: ignoring MSG_CONTENT here
    case VT_MSG_PASS:
        write(master, ans->buffer.b, ans->buffer.len);
        break;
    case VT_MSG_RESIZE:
        wp.ws_col = MIN(MAX_PTY_SIZE, ans->point.c);
        wp.ws_row = MIN(MAX_PTY_SIZE, ans->point.r);
        ioctl(master, TIOCSWINSZ, &wp);
        break;
    }
}

int main(int argc, char **argv) {
#define LOG_ERR(S) (err = errno, perror(S), err)
    
    int master, err;
    pid_t pid;

    if (argc < 2) {
        fputs("insufficient arguments", stderr);
        return EXIT_FAILURE;
    }

    pid = forkpty(&master, NULL, NULL, NULL);
    if(pid == -1)
        return LOG_ERR("forkpty(): ");

    if (pid != 0) {
        // parent
        struct pollfd fds[2] = {
            { .fd = master, .events = POLLIN },
            { .fd = STDIN_FILENO, .events = POLLIN },
        };

        vt_parser_t *vt = vtnew(callback, &master);
        char buffer[BUFFER_SIZE];
        size_t length;
        int status;

        while (1) {
            poll(fds, 2, -1);
            if (fds[1].revents & POLLIN) {
                length = read(STDIN_FILENO, buffer, BUFFER_SIZE);
                vtparse(vt, buffer, length);
            }
            if (fds[0].revents & POLLIN) {
                length = read(master, &buffer[0], BUFFER_SIZE);
                write(STDOUT_FILENO, &buffer[0], length);
            }

            if (fds[0].revents & POLLHUP || fds[1].revents & POLLHUP) {
                waitpid(pid, &status, 0);
                vtfree(vt);
                return WEXITSTATUS(status);
            }
            if (fds[0].revents & POLLERR || fds[1].revents & POLLERR) {
                vtfree(vt);
                return EXIT_FAILURE;
            }
        }
    } else {
        // child
        setenv("TERM", "ansi", 1);
        execvp(argv[1], argv + 1);
        return LOG_ERR("execvp(): ");
    }
}
