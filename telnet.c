#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>

#include "telnet.h"

#define MAX_CMD_SIZE 2048
#define SOCKET_ADDRESS "wormsocket"

int telnet_login(char *addr, char *username, char *password) {
    char buf[MAX_CMD_SIZE];
    int fd, len, i;
    char *command;
    char *argv[3];

    command = "/usr/bin/telnet";
    argv[0] = command;
    argv[1] = addr;
    argv[2] = NULL;

    if ((fd = run_command(command, argv, NULL)) < 0) {
        perror("telnet: run_command");
        return -1;
    }

    if (expect(fd, "login:") < 0) {
        perror("telnet: login");
        return -1;
    }
    write(fd, username, strlen(username));
    write(fd, "\n", 1);

    if (expect(fd, "Password:") < 0) {
        perror("telnet: password");
        return -1;
    }
    write(fd, password, strlen(password));
    write(fd, "\n", 1);

    while (1) {
        if (wait_input(fd, 20) < 0) {
            perror("telnet: response");
            return -1;
        }

        if ((len = read(fd, buf, sizeof(buf) - 1)) <= 0) {
            perror("telnet: response2");
            return -1;
        }

        buf[len] = '\0';

        // Login incorrect message, fail immediately
        if (memcmp(buf, "Login incorrect", strlen("Login incorrect")) == 0) {
            return -1;
        }

        for (i=0; i<strlen(buf); i++) {
            if (buf[i] == '$') {
                // we have a shell, login success!
                return fd;
            }
        }
    }

    return -1;
}

int run_command(char *command, char *argv[], char *envp[]) {
    int s, fd, res, client_addr_len, pid, flags;
    struct sockaddr_un addr, client_addr;

    pid = fork();

    if (pid == 0) { // child continues here
        if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
            perror("server: socket");
            exit(-1);
        }

        unlink(SOCKET_ADDRESS);

        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, SOCKET_ADDRESS, sizeof(addr.sun_path)-1);

        if (bind(s, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
            perror("server: bind");
            exit(-1);
        }

        if (listen(s, 5) < 0) {
            perror("server: listen");
            exit(-1);
        }

        if ((fd = accept(s, (struct sockaddr*) &client_addr, &client_addr_len)) < 0) {
            perror("server: accept");
            exit(-1);
        }

        // redirect stdin
        if (dup2(fd, STDIN_FILENO) == -1) {
            perror("redirecting stdin");
            exit(-1);
        }

        // redirect stdout
        if (dup2(fd, STDOUT_FILENO) == -1) {
            perror("redirecting stdout");
            exit(-1);
        }

        // redirect stderr
        if (dup2(fd, STDERR_FILENO) == -1) {
            perror("redirecting stderr");
            exit(-1);
        }

        // run child process image
        // replace this with any exec* function find easier to use ("man exec")
        res = execve(command, argv, envp);

        // if we get here at all, an error occurred, but we are in the child
        // process, so just exit
        exit(res);
    }
    else { // parent continues here
        sleep(1);

        if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
            perror("server: socket");
            return -1;
        }

        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, SOCKET_ADDRESS, sizeof(addr.sun_path)-1);

        if (connect(s, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
            perror("client: connect");
            return -1;
        }

        return s;
    }
}

int wait_input(int fd, int sec) {
    fd_set rset;
    struct timeval tv;
    int n, error, flags;

    error = 0;
    flags = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("read_timeout");
        return -1;
    }

    FD_ZERO(&rset);
    FD_SET(fd, &rset);
    tv.tv_sec = sec;
    tv.tv_usec = 0;

    /* now we wait until more data is received then the tcp low level
     * watermark, which should be setted to 1 in this case (1 is default)
     */
    n = select(fd + 1, &rset, NULL, NULL, &tv);
    if (n == 0) {
        if (fcntl(fd, F_SETFL, flags) == -1) {
            return -1;
        }
        errno = ETIMEDOUT;
        return -1;
    }
    if (n == -1) {
        return -1;
    }

    if (FD_ISSET(fd, &rset)) {
        if (fcntl(fd, F_SETFL, flags) == -1) {
            return -1;
        }

        return 0;
    }
    else {
        if (fcntl(fd, F_SETFL, flags) == -1) {
            return -1;
        }

        errno = ETIMEDOUT;
        return -1;
    }
}


int expect(int fd, const char *pattern) {
    char buf[MAX_CMD_SIZE];
    int n;

    do {
        if (wait_input(fd, 20) < 0) {
            return -1;
        }

        if ((n = read(fd, buf, sizeof(buf) - 1)) <= 0) {
            return -1;
        }

        buf[n] = '\0';
    } while (memcmp(buf, pattern, strlen(pattern)) != 0);

    return 0;
}
