
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h> //inet_ntoa()
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/param.h>
#include <sys/select.h>
#include <assert.h>
#include <sys/epoll.h>
#include <sys/stat.h>

#include "utils.h"
#include "config.h"
#include "transfer.h"

int main()
{

    struct transfer_config *cfg = (struct transfer_config *)malloc(sizeof(struct transfer_config));

    cfg->client_nums = 0;

    struct in_addr s;
    //inet_aton("172.16.187.123", (struct in_addr *)&s.s_addr);
    //inet_aton("120.27.249.122", (struct in_addr *)&s.s_addr);
    cfg->ip_addr = s.s_addr;

    cfg->ip_addr = 0;
    cfg->port = 6000;

    transfer_loop(cfg);

    //设置为守护进程
    //daemonize();

    //保证只有一个主进程实例
    //int oncefd = open_only_once();

    // while (1)
    //     ;

    //close(oncefd);
    return 0;
}