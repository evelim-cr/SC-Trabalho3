#ifndef _TELNET_H_
#define _TELNET_H_

int telnet_negotiate(int sock, unsigned char *buf, int len);
int telnet_login(uint32_t addr, int port);

#endif