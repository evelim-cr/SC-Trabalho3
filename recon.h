#ifndef _RECON_H_
#define _RECON_H_

// #include <stdio.h>
// #include <stdlib.h>
// #include <unistd.h>
// #include <string.h>
// #include <regex.h>
// #include <sys/types.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
// #include <netdb.h>
// #include <time.h>

#define MAX_STRING_ARRAY 1
#define IP_FIELD_SIZE 16

#define SOCK_FAMILY AF_INET
#define SOCKET_TYPE SOCK_STREAM
#define SOCK_TCP 6

typedef enum {
    CLASS_A,
    CLASS_B,
    CLASS_C
}ip_type;

typedef enum {
    FTP = 21,
    TELNET = 23
}service;

typedef struct {
    unsigned int start;
    unsigned int end;
}range;

typedef struct {
    char ip[IP_FIELD_SIZE];
    unsigned short int open_ftp;
    unsigned short int open_telnet;
}open_ip;

/**
*   Gera os ips a serem testados.
**/
void generateIPs(ip_type type, range *ip);

int testConnection(char* ip, int porta);

/**
*   Funcao que remove o ultimo campo do IP.
**/
char * ipSplit(char *str);

/**
* Funcao que retorna o ultimo campo do IP, eg: se str = 192.168.0.25 retorna o valor 25
**/
int getLastField(char *str);

/**
*   Funcao que faz a validacao da entrada strValidate de acordo com o regex definido no pattern.
**/
int regexValidation(char *strValidate, char * pattern);

/**
*   Funcao que Imprime o horario.
**/
void printTimestamp();
#endif
