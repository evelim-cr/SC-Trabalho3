#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>

#include "recon.h"

#define MAX_STRING_ARRAY 1
#define IP_FIELD_SIZE 16

#define SOCK_FAMILY AF_INET
#define SOCKET_TYPE SOCK_STREAM
#define SOCK_TCP 6

/**
*   Gera os ips a serem testados.
**/
void generateIPs(ip_type type, range *ip){

    //Fixos para os 3 tipos
    ip[2].start = 0;
    ip[2].end = 255;
    ip[3].start = 0;
    ip[3].end = 255;
    switch(type){
        case CLASS_A:
            ip[0].start = 10;
            ip[0].end = 10;
            ip[1].start = 0;
            ip[1].end = 255;
            break;
        case CLASS_B:
            ip[0].start = 172;
            ip[0].end = 172;
            ip[1].start = 16;
            ip[1].end = 31;
            break;
        case CLASS_C:
        default:
            ip[0].start = 192;
            ip[0].end = 192;
            ip[1].start = 168;
            ip[1].end = 168;
            break;
    }
}

int testConnection(char *ip, int porta){
    int mySocket;
    struct sockaddr_in connection;
    int connector;
    int len;
    char recebe[1024];
    char *pos;

    mySocket = socket(SOCK_FAMILY, SOCKET_TYPE, SOCK_TCP);
    connection.sin_family = SOCK_FAMILY;
    connection.sin_port = htons(porta);
    connection.sin_addr.s_addr = inet_addr(ip);

    bzero(&(connection.sin_zero),8);
    connector = connect(mySocket, (struct sockaddr * ) &connection, sizeof(connection));

    if(connector >= 0) {
        /* escreve qualquer coisa, já que alguns serviços só enviam
            * o banner depois de alguma escrita */
        len = write(mySocket, "teste", strlen("teste"));

        /* tenta realizar uma leitura, possivelmente capturando uma
            * mensagem de banner */
        bzero(recebe, sizeof(recebe));
        len = read(mySocket, recebe, 1024);

        /* ignora o \n e tudo que vem depois dele */
        if ((pos=strchr(recebe, '\n')) != NULL)
            *pos = '\0';

        printf("%s\t%d\t%s\n", ip, porta, recebe);
        return 1;
    }
    close(mySocket);
    return 0;
}

/**
*   Função que testa os IPs definidos em ips, utilizando o range de porta definidos em portRange.
**/
void startTestConnection(range ips[], range ports){
    char ip[IP_FIELD_SIZE];
    int campo1,campo2,campo3,campo4,porta;
    open_ip target;

    for(campo1 = ips[0].start; campo1 <= ips[0].end; campo1++){
        for(campo2 = ips[1].start; campo2 <= ips[1].end; campo2++){
            for(campo3 = ips[2].start; campo3 <= ips[2].end; campo3++){
                for(campo4 = ips[3].start; campo4 <= ips[3].end; campo4++){
                    sprintf(ip,"%d.%d.%d.%d",campo1,campo2,campo3,campo4);
                    strcpy(target.ip,ip);
                    for(porta = ports.start; porta <= ports.end; porta++){
                        printf("IP: %s\n",ip);
                        if(testConnection(ip,porta)){
                            if(porta == FTP)
                                target.open_ftp = 1;
                            else if(porta == TELNET)
                                target.open_telnet = 1;
                        }
                    }
                    if(target.open_ftp && target.open_telnet)
                        //TODO: Random Attack
                    else(target.open_ftp)
                        //TODO: Exploit
                    else(target.open_telnet)
                        //TODO: Brute force
                    target.open_ftp = 0;
                    target.open_telnet = 0;
                }
            }
        }
    }
}

/**
* Função que retorna o rannge da entrada str.
**/
int getRange(char *str, char *splitter){
    char *token = strtok(str, splitter);
    char *last = malloc(sizeof(str));
    while(token) {
        strcpy(last,token);
        token = strtok(NULL, splitter);
    }
    int range;
    if(strcmp(str,last) == 0){
        range = 0;
    }
    else{
        range = atoi(last);
    }
    free (last);
    return range;
}

/**
*   Função que remove o ultimo campo do IP.
**/
char * ipSplit(char *str){
    int ipLen = strlen(str);
    char *splittedIp;
    int split = 0;
    int aux = 0;
    int i;
    for(i = 0; i < ipLen; i++) {
        if(str[i] == '.')
            split = aux + 1;
        aux ++;
    }
    splittedIp =(char *) malloc(split + 1);
    memcpy(splittedIp, str, split);
    splittedIp[split] = '\0';
    return splittedIp;
}

/**
* Função que retorna o ultimo campo do IP, eg: se str = 192.168.0.25 retorna o valor 25
**/
int getLastField(char *str){
    int ipLen = strlen(str);
    char *splittedIp;
    int aux = 0;
    int i;
    char* field;
    field = (char *) malloc(4);
    for(i = 0; i < ipLen; i++) {
        if(str[i] == '.')
            aux = 0;
        else{
            field[aux] = str[i];
            aux ++;
        }
    }
    field[aux]='\0';
    return atoi(field);
}

/**
*   Função que faz a validação da entrada strValidate de acordo com o regex definido no pattern.
**/
int regexValidation(char *strValidate, char * pattern){
    regex_t reg;

    /* compila a ER passada em argv[1]
     * em caso de erro, a função retorna diferente de zero */
    if (regcomp(&reg , pattern, REG_EXTENDED|REG_NOSUB) != 0) {
        fprintf(stderr,"erro regcomp\n");
        exit(1);
    }
    /* tenta casar a ER compilada (&reg) com a entrada (argv[2])
     * se a função regexec retornar 0 casou, caso contrário não */
    if ((regexec(&reg, strValidate, 0, (regmatch_t *)NULL, 0)) == 0)
        return 1;
    else
        return 0;
}

/**
*   Função que Imprime o horário.
**/
void printTimestamp() {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char buf[255];

    strftime(buf, sizeof(buf), "%c", &tm);
    printf("%s", buf);
}
