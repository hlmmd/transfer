
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
                close(sockfd);
                printf("server down\n");
                exit(0);
                break;
            }
        }
    }
}

int readfile_onepkt(int filefd, int chunk_num, int pkt_num, unsigned char *buffer, int readsize)
{
    lseek(filefd, chunk_num * CHUNK_SIZE + pkt_num * BUFFER_SIZE, SEEK_SET);
    int ret = read(filefd, buffer, readsize);
    return ret;
}

int readfile_onechunk(int filefd, int chunk_num, void *buffer, int readsize)
{
    lseek(filefd, chunk_num * CHUNK_SIZE, SEEK_SET);
    int ret = read(filefd, buffer, readsize);
    return ret;
}

int main(int argc, char **argv)
{
    if (argc <= 3)
    {
        printf("usage: ./%s ip_address port filename\n", basename(argv[0]));
        printf("usage: ./%s 192.168.3.90 6000  /home/qinrui/Github/linux-master.zip \n", basename(argv[0]));
        return 1;
    }

    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(-1);
    }

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));

    if (inet_aton(argv[1], (struct in_addr *)&server_addr.sin_addr.s_addr) == 0)
    {
        exit(errno);
    }

    setnonblocking(sockfd);

    connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (errno != EINPROGRESS)
    {
        perror("connect");
        exit(errno);
    }
    printf("connected server !\n");

    unsigned char header_buffer[HEAD_SIZE + BUFFER_SIZE];

    struct transfer_packet *pktl = (struct transfer_packet *)malloc(sizeof(struct transfer_packet));
    struct transfer_packet *pkt = pktl;
    if (pkt == NULL)
        exit(0);
    memset(pkt, 0, sizeof(struct transfer_packet));

    char *filename = argv[3];
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
            printf("%d/%d\n", send_checkun_num, finfo->chunknum);

            uint32 read_chunk_size = readfile_onechunk(fd, send_checkun_num, file_buffer + HEAD_SIZE, CHUNK_SIZE);

            uint32 sended = 0;

            pkt = (struct transfer_packet *)file_buffer;
            uint16 send_pkt_num = 0;
            while (read_chunk_size >= BUFFER_SIZE)
            {
                pkt->type = UPLOAD_CTOS_ONEPKT;
                pkt->length = BUFFER_SIZE;

                uint16 sended_onepkt = 0;
                while (sended_onepkt < BUFFER_SIZE + HEAD_SIZE)
                {
                    int ret = send(sockfd, (void *)pkt + sended_onepkt, HEAD_SIZE + BUFFER_SIZE - sended_onepkt, 0);
                    if (ret < 0)
                        continue;
                    else if (ret == 0)
                    {
                        close(sockfd);
                        exit(0);
                    }
                    else
                        sended_onepkt += ret;
                }
                read_chunk_size -= BUFFER_SIZE;
                pkt = (struct transfer_packet *)((void *)pkt + pkt->length);
            }
            //发送完成
            if (read_chunk_size != 0)
            {

                printf("last\n");
                pkt->type = UPLOAD_CTOS_LASTPKT;
                pkt->length = read_chunk_size;
                uint16 sended_onepkt = 0;
                while (sended_onepkt < HEAD_SIZE + read_chunk_size)
                {
                    int ret = send(sockfd, (void *)pkt + sended_onepkt, HEAD_SIZE + read_chunk_size - sended_onepkt, 0);
                    printf("ret:%d\n", ret);
                    if (ret < 0)
                        continue;
                    else if (ret == 0)
                    {
                        close(sockfd);
                        exit(0);
                    }
                    else
                        sended_onepkt += ret;
                }
                read_chunk_size -= read_chunk_size;
                break;
            }
        }
    }

    printf("send ok %lld\n", finfo->filesize);
    gettimeofday(&end, NULL);
    diff = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    //  printf("thedifference is %.2ld\n", diff);

    double speed = finfo->filesize * 1.0 / 1024 / 1024 * 1000000 / diff;

    printf("speed is  %.2lfM/s\n", speed);
    free(pktl);
    free(file_buffer);
    close(sockfd);
    close(fd);
    return 0;
}