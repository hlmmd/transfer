
#ifndef INCLUDE_UTILS
#define INCLUDE_UTILS

//设置守护进程
int daemonize();

//保证只有一个主进程实例
int open_only_once();

#endif