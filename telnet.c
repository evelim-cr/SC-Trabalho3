#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <termios.h>
#include <fcntl.h>
#include <arpa/telnet.h>

#define CMD 0xff
#define CMD_ECHO 1
#define CMD_WINDOW_SIZE 31

#define MAX_COMMAND_SIZE 512

int telnet_negotiate(int sock, char *buf, int len) {
    int i;

    if (buf[1] == DO && buf[2] == CMD_WINDOW_SIZE) {
        unsigned char tmp1[10] = {255, 251, 31};
        if (send(sock, tmp1, 3 , 0) < 0)
            return 0;

        unsigned char tmp2[10] = {255, 250, 31, 0, 80, 0, 24, 255, 240};
        if (send(sock, tmp2, 9, 0) < 0)
            return 0;

        return 1;
    }

    for (i = 0; i < len; i++) {
        if (buf[i] == DO)
            buf[i] = WONT;
        else if (buf[i] == WILL)
            buf[i] = DO;
    }

    if (send(sock, buf, len, 0) < 0)
        return 0;

    return 1;
}

int telnet_login(uint32_t addr, int port) {
    int sock;
    struct sockaddr_in server;
    unsigned char buf[MAX_COMMAND_SIZE + 1];
    int len;
    int i;

    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1) {
        perror("Could not create socket. Error");
        return -1;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = addr;

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("connect failed. Error");
        return -1;
    }

    while (1) {
        // start by reading a single byte
        len = recv(sock, buf, 1, 0);

        if (len < 0)
            return -1;
        else if (len == 0) {
            fprintf(stderr, "Connection closed by the remote end\n");
            return -1;
        }

        if (buf[0] == CMD) {
            // read 2 more bytes
            len = recv(sock , buf+1 , 2, 0);

            if (len  < 0)
                return -1;
            else if (len == 0) {
                fprintf(stderr, "Connection closed by the remote end\n");
                return -1;
            }

            telnet_negotiate(sock, buf, 3);
        }
        else {
            // read the rest of the buffer
            len = recv(sock, buf+1, MAX_COMMAND_SIZE-1, 0);
            buf[len] = '\0';

            if (len  < 0)
                return -1;
            else if (len == 0) {
                fprintf(stderr, "Connection closed by the remote end\n");
                return -1;
            }

            printf("buffer = %s\n", buf);

            if (strncmp(buf, "login:", 6) == 0) {
                sprintf(buf, "dumb\n\r");
                if (send(sock, buf, strlen(buf), 0) < 0)
                    return -1;
            }

            if (strncmp(buf, "Password:", 9) == 0) {
                sprintf(buf, "123mudar\n\r");
                if (send(sock, buf, strlen(buf), 0) < 0)
                    return -1;

                return sock;
            }
        }
    }
}
