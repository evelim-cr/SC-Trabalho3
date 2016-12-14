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
#include <fcntl.h>
#include <errno.h>
#include <ifaddrs.h>

#include "recon.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define FTP_PORT 21
#define TELNET_PORT 23
#define CONNECT_TIMEOUT 2
#define MAX_CONNECTIONS 16

int getLocalSubnets(ip_subnet **subnets) {
    struct ifaddrs *ifaddr, *ifa;
    struct sockaddr_in *addr_in, *netmask_in;
    int count;

    if (getifaddrs(&ifaddr) < 0) {
        fprintf(stderr, "Erro ao obter interfaces locais.\n");
        return -1;
    }

    count = 0;
    (*subnets) = malloc(128 * sizeof(ip_subnet));

    (*subnets)[count].addr = inet_addr("10.2.0.0");
    (*subnets)[count].netmask = inet_addr("255.255.255.0");
    count++;

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        // pula interfaces sem endereço ipv4
        if ((ifa->ifa_addr == NULL) || (ifa->ifa_addr->sa_family != AF_INET))
            continue;

        // pular interface de loopback
        if (strcmp(ifa->ifa_name, "lo") == 0)
            continue;

        addr_in = (struct sockaddr_in *) ifa->ifa_addr;
        netmask_in = (struct sockaddr_in *) ifa->ifa_netmask;

        (*subnets)[count].addr = addr_in->sin_addr.s_addr;
        (*subnets)[count].netmask = netmask_in->sin_addr.s_addr;
        count++;
    }

    freeifaddrs(ifaddr);

    (*subnets)[count].addr = inet_addr("192.168.0.0");
    (*subnets)[count].netmask = inet_addr("255.255.0.0");
    count++;

    (*subnets)[count].addr = inet_addr("10.0.0.0");
    (*subnets)[count].netmask = inet_addr("255.0.0.0");
    count++;

    (*subnets)[count].addr = inet_addr("172.16.0.0");
    (*subnets)[count].netmask = inet_addr("255.240.0.0");
    count++;

    return count;
}

void scanSubnets(ip_subnet *subnets, int subnet_count, void (*callback)(uint32_t,int,int)) {
    socket_info ftp_connections[MAX_CONNECTIONS];
    socket_info telnet_connections[MAX_CONNECTIONS];
    uint32_t begin, end, cur;
    uint32_t block_size, block_offset;
    struct in_addr addr_in, netmask_in;
    int i;
    int ftp_open, telnet_open;

    for (i=0; i<subnet_count; i++) {
        begin = subnets[i].addr & subnets[i].netmask;
        end = subnets[i].addr ^ ~subnets[i].netmask;

        addr_in.s_addr = subnets[i].addr;
        netmask_in.s_addr = subnets[i].netmask;

        printf("# SCANNING SUBNET: %s", inet_ntoa(addr_in));
        printf("/%s\n", inet_ntoa(netmask_in));

        cur=begin;
        while (cur < end) {
            block_size = MIN(ntohl(end) - ntohl(cur), MAX_CONNECTIONS);

            for (block_offset=0; block_offset<block_size; block_offset++) {
                ftp_connections[block_offset].addr.sin_family = AF_INET;
                ftp_connections[block_offset].addr.sin_port = htons(FTP_PORT);
                ftp_connections[block_offset].addr.sin_addr.s_addr = cur;

                telnet_connections[block_offset].addr.sin_family = AF_INET;
                telnet_connections[block_offset].addr.sin_port = htons(TELNET_PORT);
                telnet_connections[block_offset].addr.sin_addr.s_addr = cur;

                printf("# Scanning %s...\n", inet_ntoa(ftp_connections[block_offset].addr.sin_addr));

                cur+=htonl(1);
            }

            asyncTestConnection(ftp_connections, block_size);
            asyncTestConnection(telnet_connections, block_size);

            for (block_offset=0; block_offset<MAX_CONNECTIONS; block_offset++) {
                ftp_open = ftp_connections[block_offset].open;
                telnet_open = telnet_connections[block_offset].open;

                if (ftp_open || telnet_open) {
                    callback(ftp_connections[block_offset].addr.sin_addr.s_addr, ftp_open, telnet_open);
                }
            }
        }
    }
}


int asyncTestConnection(socket_info *connections, int conn_count) {
    int res;
    long arg;
    fd_set myset;
    struct timeval tv;
    int valopt;
    socklen_t lon;
    int i;

    for (i=0; i<conn_count; i++) {
        connections[i].ready = 0;
        connections[i].error = 0;
        connections[i].open = 0;

        connections[i].sock = socket(AF_INET, SOCK_STREAM, 0);

        // set non-blocking
        arg = fcntl(connections[i].sock, F_GETFL, NULL);
        arg |= O_NONBLOCK;
        fcntl(connections[i].sock, F_SETFL, arg);

        // trying to connect with timeout
        res = connect(connections[i].sock, (struct sockaddr *)&connections[i].addr, sizeof(struct sockaddr));
        if ((res < 0) && (errno != EINPROGRESS)) {
            connections[i].ready = 1;
            connections[i].error = errno;
        }
    }

    while (1) {
        tv.tv_sec = CONNECT_TIMEOUT;
        tv.tv_usec = 0;

        FD_ZERO(&myset);

        for (i=0; i<conn_count; i++) {
            if (!connections[i].ready) {
                FD_SET(connections[i].sock, &myset);
            }
        }

        res = select(connections[conn_count-1].sock+1, NULL, &myset, NULL, &tv);

        if (res <= 0) {
            // timeout
            break;
        }

        for (i=0; i<conn_count; i++) {
            // verifica se a conexão ainda não está pronta
            if (!FD_ISSET(connections[i].sock, &myset)) {
                continue;
            }

            // verifica o estado da conexão e coloca no atributo error do connections
            lon = sizeof(int);
            getsockopt(connections[i].sock, SOL_SOCKET, SO_ERROR, (void*)(&connections[i].error), &lon);

            // conexão pronta
            connections[i].ready = 1;

            // se não houve erros, então a porta está aberta
            if (!connections[i].error) {
                connections[i].open = 1;
            }
        }
    }

    for (i=0; i<conn_count; i++) {
        // set blocking again
        arg = fcntl(connections[i].sock, F_GETFL, NULL);
        arg &= ~O_NONBLOCK;
        fcntl(connections[i].sock, F_SETFL, arg);

        // close the connection
        close(connections[i].sock);
    }
}


char *addrToString(uint32_t addr) {
    struct in_addr a;
    a.s_addr = addr;
    return inet_ntoa(a);
}