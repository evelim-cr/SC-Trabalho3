#ifndef _TELNET_H_
#define _TELNET_H_

int telnet_login(char *addr, char *username, char *password);

int run_command(char *command, char *argv[], char *envp[]);
int wait_input(int fd, int sec);
int expect(int fd, const char *pattern);

#endif