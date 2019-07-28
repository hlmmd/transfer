
#define _GNU_SOURCE

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

int main(int argc, char **argv)
{

    // if (argc <= 2)
    // {
    //     printf("usage: ./%s ip_address port \n", basename(argv[0]));
    //     return 1;
    // }

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(-1);
    }

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;

    server_addr.sin_port = htons(6000);
    if (inet_aton("192.168.3.90", (struct in_addr *)&server_addr.sin_addr.s_addr) == 0)
    {
        //   perror(argv[1]);
        exit(errno);
    }

    // server_addr.sin_port = htons(atoi(argv[2]));
    // if (inet_aton(argv[1], (struct in_addr *)&server_addr.sin_addr.s_addr) == 0)
    // {
    //     perror(argv[1]);
    //     exit(errno);
    // }

    setnonblocking(sockfd);

    connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (errno != EINPROGRESS)
    {
        perror("connect");
        exit(errno);
    }
    printf("connected server !\n");

    unsigned char header_buffer[HEAD_SIZE + BUFFER_SIZE];

    char *filename = "/home/qinrui/Github/linux-master.zip";
    int fd = 0;
    if ((fd = open(filename, O_RDONLY)) == -1)
    {
        printf("open erro ！\n");
        exit(-1);
    }

    /*发送文件信息*/
    struct stat filestat;
    fstat(fd, &filestat);
    int last_bs = 0;
    struct fileinfo *finfo = (struct fileinfo *)(header_buffer + HEAD_SIZE);

    char *bname = basename(filename);
    printf("%s\n", bname);

    int namelength = strlen(bname);

    finfo->filesize = filestat.st_size;
    printf("%lld", finfo->filesize);
    memcpy(finfo->filename, bname, namelength);

    usleep(500);

    //发送文件名
    if (1)
    {
        memset(header_buffer, 0, HEAD_SIZE + BUFFER_SIZE);
        uint16 sendsize = HEAD_SIZE + namelength + sizeof(uint64);

        printf("%d\n", sendsize);

        form_header(header_buffer, UPLOAD_CTOS_START, sendsize);
        send(sockfd, header_buffer, sendsize, 0);
    }

    //设置为守护进程
    //daemonize();

    //保证只有一个主进程实例
    //int oncefd = open_only_once();

    while (1)
        ;

    //close(oncefd);
    return 0;
}