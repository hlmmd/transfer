
#ifndef INCLUDE_UTILS
#define INCLUDE_UTILS

#include "config.h"

//设置守护进程
int daemonize();

//保证只有一个主进程实例
int open_only_once();

//设置非阻塞socket
int setnonblocking(int fd);

int form_header(char *head, unsigned char type, uint16 length);

void printf_debug(unsigned char * str,uint32 length);

#endif