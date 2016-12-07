#ifndef _RECON_H_
#define _RECON_H_

typedef struct {
    uint32_t addr;
    uint32_t netmask;
} ip_subnet;

typedef struct {
    int sock;
    int ready;
    int error;
    int open;
    struct sockaddr_in addr;
} socket_info;

int getLocalSubnets(ip_subnet **subnets);

void scanSubnets(ip_subnet *subnets, int subnet_count, void (*callback)(uint32_t,int,int));

int asyncTestConnection(socket_info *connections, int conn_count);

char *addrToString(uint32_t addr);

#endif
