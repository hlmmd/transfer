
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

int select_send(int sockfd, char *buffer, int recvsize)
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

int select_recv(int sockfd, void *buffer, int recvsize)
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

int readfile_onechunk(int filefd, int chunk_num, void *buffer, int readsize)
{
    struct timeval start;
    struct timeval end;

    gettimeofday(&start, NULL);

    lseek(filefd, chunk_num * CHUNK_SIZE, SEEK_SET);
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
    // if (inet_aton("10.60.148.139", (struct in_addr *)&server_addr.sin_addr.s_addr) == 0)
    //     if (inet_aton("120.27.249.122", (struct in_addr *)&server_addr.sin_addr.s_addr) == 0)
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

    struct transfer_packet *pkt = (struct transfer_packet *)malloc(sizeof(struct transfer_packet));
    if (pkt == NULL)
        exit(0);
    memset(pkt, 0, sizeof(struct transfer_packet));

    //char *filename = "/home/qinrui/Github/LibeventBook.pdf";
    char *filename = "/home/qinrui/Github/linux-master.zip";
    // char *filename = "/home/qinrui/Github/LICENSE";
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
    struct fileinfo *finfo = (struct fileinfo *)(pkt->data);

    char *bname = basename(filename);
    int namelength = strlen(bname);
    //设置文件大小和文件名
    finfo->filesize = filestat.st_size;
    memcpy(finfo->filename, bname, namelength);

    finfo->chunknum = (int)(finfo->filesize / CHUNK_SIZE) + 1;

    int last_pkt = finfo->filesize % CHUNK_SIZE / BUFFER_SIZE + 1;

    //usleep(500);
    sleep(1);

    struct timeval start;
    struct timeval end;

    unsigned long diff;
    gettimeofday(&start, NULL);

    printf("%lld\n", finfo->filesize);

    // if (1)
    // {
    //     int sended = 0;
    //     while (sended < (finfo->filesize))
    //     {
    //         // usleep(100);
    //         int s = BUFFER_SIZE > (finfo->filesize - sended) ? finfo->filesize : (finfo->filesize - sended);
    //         int ret = send(sockfd, pkt, s, 0);
    //         if (ret < 0)
    //             continue;
    //         sended += ret;
    //     }

    //     gettimeofday(&end, NULL);
    //     diff = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    //     //  printf("thedifference is %.2ld\n", diff);

    //     double speed = finfo->filesize * 1.0 / 1024 / 1024 * 1000000 / diff;

    //     printf("%d %d\n", filediff, diff);

    //     printf("speed is  %.2lfM/s\n", speed);
    // }
    // close(sockfd);
    // exit(0);

    //发送文件名
    if (1)
    {
        uint16 sendsize = HEAD_SIZE + namelength + sizeof(uint64);
        pkt->type = UPLOAD_CTOS_START;
        pkt->length = sendsize - 4;
        send(sockfd, pkt, sendsize, 0);
    }

    uint16 send_checkun_num = 0;
    uint16 send_pkt_num = 0;
    uint64 totalsendsize = 0;
    unsigned char *file_buffer = (unsigned char *)malloc(CHUNK_SIZE + HEAD_SIZE);
    while (1)
    {
        unsigned char recv_header[HEAD_SIZE];
        select_recv(sockfd, pkt, HEAD_SIZE);
        int send_pkt_size = 0;

        if (pkt->type == UPLOAD_STOC_CHUNKNUM)
        {
            uint16 send_checkun_num = pkt->length;
            printf("ccc %d\n", send_checkun_num);

            uint32 read_chunk_size = readfile_onechunk(fd, send_checkun_num, file_buffer + HEAD_SIZE, CHUNK_SIZE);

            uint32 sended = 0;

            pkt = file_buffer;
            uint16 send_pkt_num = 0;
            while (read_chunk_size >= BUFFER_SIZE)
            {
                //   printf("%lld \n", ( (int) pkt -(int) file_buffer));
                pkt->type = UPLOAD_CTOS_ONEPKT;
                pkt->length = BUFFER_SIZE;

                uint16 sended_onepkt = 0;
                while (sended_onepkt < BUFFER_SIZE + HEAD_SIZE)
                {
                    int ret = send(sockfd, (void *)pkt + sended_onepkt, HEAD_SIZE + BUFFER_SIZE - sended_onepkt, 0);
                    if (ret < 0)
                        continue;
                    else if (ret == 0)
                        exit(0);
                    else
                        sended_onepkt += ret;
                }
                read_chunk_size -= BUFFER_SIZE;
                pkt = (struct transfer_packet *)((void *)pkt + pkt->length);
            }
            //发送完成
            if (read_chunk_size != 0)
            {
                pkt->type = UPLOAD_CTOS_LASTPKT;
                pkt->length = read_chunk_size;

                // printf_debug(pkt->data, pkt->length);

                uint16 sended_onepkt = 0;
                while (sended_onepkt < HEAD_SIZE + read_chunk_size)
                {
                    int ret = send(sockfd, (void *)pkt + sended_onepkt, HEAD_SIZE + read_chunk_size - sended_onepkt, 0);
                    if (ret < 0)
                        continue;
                    else if (ret == 0)
                        exit(0);
                    else
                        sended_onepkt += ret;
                }
                read_chunk_size -= read_chunk_size;

                break;
            }
            //    printf("aaa\n");
        }

        // if (recv_header[0] == UPLOAD_STOC_CHUNKNUM)
        // {

        //     header_buffer[0] = UPLOAD_CTOS_ONEPKT;
        //     send_pkt_num = 0;
        //     send_checkun_num = (recv_header[2] << 8) + recv_header[3];

        //     //printf_debug(header_buffer+)

        //     for (int i = 0; i < PKTS_PER_CHUNK; i++)
        //     {
        //         header_buffer[0] = UPLOAD_CTOS_ONEPKT;
        //         send_pkt_size = readfile_onepkt(fd, send_checkun_num, send_pkt_num, header_buffer + HEAD_SIZE, BUFFER_SIZE);
        //         if (send_pkt_size < BUFFER_SIZE)
        //             break;
        //         totalsendsize += send_pkt_size;
        //         header_buffer[2] = send_pkt_size >> 8;
        //         header_buffer[3] = send_pkt_size;
        //         send(sockfd, header_buffer, HEAD_SIZE + send_pkt_size, 0);
        //         printf("ccc %d\n", send_checkun_num);
        //     }
        //     //send_pkt_size = readfile_onepkt(fd, send_checkun_num, send_pkt_num, header_buffer + HEAD_SIZE, BUFFER_SIZE);
        // }
        // // else if (recv_header[0] == UPLOAD_STOC_NEXTPKTNUM)
        // // {
        // //     header_buffer[0] = UPLOAD_CTOS_ONEPKT;
        // //     //send_pkt_num = *((uint16 *)(recv_header + 2));
        // //     send_pkt_num = (recv_header[2] << 8) + recv_header[3];
        // //     //   printf("ppp %d\n", send_pkt_num);
        // //     send_pkt_size = readfile_onepkt(fd, send_checkun_num, send_pkt_num, header_buffer + HEAD_SIZE, BUFFER_SIZE);
        // // }

        // if (send_pkt_size == BUFFER_SIZE)
        //     continue;

        // header_buffer[2] = send_pkt_size >> 8;
        // header_buffer[3] = send_pkt_size;
        // totalsendsize += send_pkt_size;

        // if (send_pkt_size < BUFFER_SIZE)
        // {
        //     header_buffer[0] = UPLOAD_CTOS_LASTPKT;
        //     send(sockfd, header_buffer, HEAD_SIZE + send_pkt_size, 0);

        //     //  printf_debug(header_buffer , send_pkt_size+ HEAD_SIZE);

        //     break;
        // }
        // else
        // {
        //     header_buffer[0] = UPLOAD_CTOS_ONEPKT;
        //     send(sockfd, header_buffer, HEAD_SIZE + send_pkt_size, 0);
        // }
    }

    printf("send ok %lld\n", finfo->filesize);

    gettimeofday(&end, NULL);
    diff = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    //  printf("thedifference is %.2ld\n", diff);

    double speed = finfo->filesize * 1.0 / 1024 / 1024 * 1000000 / diff;

    printf("%d %d\n", filediff, diff);

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