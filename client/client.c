
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

int select_recv(int sockfd, char *buffer, int recvsize)
{
    fd_set rfds;
    int maxfd = sockfd;
    int ret = 0;
    int totalsize = 0;
    while (1)
    {
        FD_ZERO(&rfds);
        FD_SET(sockfd, &rfds);
        //select
        ret = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (ret == -1)
        {
            printf("select error\n");
            break;
        }
        else if (ret == 0)
            continue;
        else
        {
            bzero(buffer, HEAD_SIZE);
            int len = recv(sockfd, buffer + totalsize, recvsize - totalsize, 0);
            if (len > 0)
                totalsize += len;

            if (totalsize == recvsize)
                return recvsize;
            else if (len < 0)
                printf("receive failed!\n");
            else
            {
                printf("server down\n");
                break;
            }
        }
    }
}

int filediff = 0;

int readfile_onepkt(int filefd, int chunk_num, int pkt_num, unsigned char *buffer, int readsize)
{
    struct timeval start;
    struct timeval end;
    
        gettimeofday(&start, NULL);

    lseek(filefd, chunk_num * CHUNK_SIZE + pkt_num * BUFFER_SIZE, SEEK_SET);
    int ret = read(filefd, buffer, readsize);



    gettimeofday(&end, NULL);

filediff += (1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec);

    return ret;
}

int main(int argc, char **argv)
{

    // if (argc <= 2)
    // {
    //     printf("usage: ./%s ip_address port \n", basename(argv[0]));
    //     return 1;
    // }
    struct timeval start;
    struct timeval end;

    unsigned long diff;
    gettimeofday(&start, NULL);

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
    //if (inet_aton("10.60.148.139", (struct in_addr *)&server_addr.sin_addr.s_addr) == 0)
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

    //char *filename = "/home/qinrui/Github/LibeventBook.pdf";
    char *filename = "/home/qinrui/Github/linux-master.zip";
    // char *filename = "/home/qinrui/Github/LICENSE";
    int fd = 0;
    if ((fd = open(filename, O_RDONLY)) == -1)
    {
        printf("open erro ！\n");
        exit(-1);
    }

    memset(header_buffer, 0, HEAD_SIZE + BUFFER_SIZE);

    /*发送文件信息*/
    struct stat filestat;
    fstat(fd, &filestat);
    int last_bs = 0;
    struct fileinfo *finfo = (struct fileinfo *)(header_buffer + HEAD_SIZE);

    char *bname = basename(filename);
    int namelength = strlen(bname);
    //设置文件大小和文件名
    finfo->filesize = filestat.st_size;

    memcpy(finfo->filename, bname, namelength);

    finfo->chunknum = (int)(finfo->filesize / CHUNK_SIZE) + 1;

    int last_pkt = finfo->filesize % CHUNK_SIZE / BUFFER_SIZE + 1;

    //usleep(500);
    sleep(1);

    //发送文件名
    if (1)
    {
        uint16 sendsize = HEAD_SIZE + namelength + sizeof(uint64);

        form_header(header_buffer, UPLOAD_CTOS_START, sendsize);
        send(sockfd, header_buffer, sendsize, 0);
    }

    uint16 send_checkun_num = 0;
    uint16 send_pkt_num = 0;
    uint64 totalsendsize = 0;
    while (1)
    {

        unsigned char recv_header[HEAD_SIZE];
        select_recv(sockfd, recv_header, HEAD_SIZE);
        int send_pkt_size = 0;
        if (recv_header[0] == UPLOAD_STOC_CHUNKNUM)
        {
            header_buffer[0] = UPLOAD_CTOS_ONEPKT;
            send_pkt_num = 0;
            send_checkun_num = (recv_header[2] << 8) + recv_header[3];

            //printf_debug(header_buffer+)

            printf("ccc %d\n", send_checkun_num);
            send_pkt_size = readfile_onepkt(fd, send_checkun_num, send_pkt_num, header_buffer + HEAD_SIZE, BUFFER_SIZE);
        }
        else if (recv_header[0] == UPLOAD_STOC_NEXTPKTNUM)
        {
            header_buffer[0] = UPLOAD_CTOS_ONEPKT;
            //send_pkt_num = *((uint16 *)(recv_header + 2));
            send_pkt_num = (recv_header[2] << 8) + recv_header[3];
            //   printf("ppp %d\n", send_pkt_num);
            send_pkt_size = readfile_onepkt(fd, send_checkun_num, send_pkt_num, header_buffer + HEAD_SIZE, BUFFER_SIZE);
        }

        header_buffer[2] = send_pkt_size >> 8;
        header_buffer[3] = send_pkt_size;
        totalsendsize += send_pkt_size;

        if (send_pkt_size < BUFFER_SIZE)
        {
            header_buffer[0] = UPLOAD_CTOS_LASTPKT;
            send(sockfd, header_buffer, HEAD_SIZE + send_pkt_size, 0);

            //  printf_debug(header_buffer , send_pkt_size+ HEAD_SIZE);

            break;
        }
        else
        {
            header_buffer[0] = UPLOAD_CTOS_ONEPKT;
            send(sockfd, header_buffer, HEAD_SIZE + send_pkt_size, 0);
        }
    }

    printf("send ok %lld\n", totalsendsize);

    gettimeofday(&end, NULL);
    diff = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    //  printf("thedifference is %.2ld\n", diff);

    double speed = totalsendsize * 1.0 / 1024 / 1024 * 1000000 / diff;


    printf("%d %d\n",filediff, diff);


    printf("speed is  %.2lfM/s\n", speed);

    //设置为守护进程
    //daemonize();

    //保证只有一个主进程实例
    //int oncefd = open_only_once();

    // while (1)
    //     ;

    //close(oncefd);
    return 0;
}