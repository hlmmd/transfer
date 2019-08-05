

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

#include <event2/event.h>

#include "config.h"
#include "utils.h"

#include "qstring.h"

evutil_socket_t create_listener(uint32 ipaddr, uint16 port)
{

    evutil_socket_t listener;
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = ipaddr;
    sin.sin_port = htons(port);

    listener = socket(AF_INET, SOCK_STREAM, 0);

    evutil_make_socket_nonblocking(listener);

    {
        int one = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

        int recvbuff = 500 * 1024 * 1024;
        setsockopt(listener, SOL_SOCKET, SO_RCVBUF, (const char *)&recvbuff, sizeof(int));
    }

    if (bind(listener, (struct sockaddr *)&sin, sizeof(sin)) < 0)
    {
        perror("bind");
        exit(errno);
    }

    if (listen(listener, 16) < 0)
    {
        perror("listen");
        exit(errno);
    }

    return listener;
}

void free_transfer_user(struct transfer_user *state)
{
    qstring_free(&state->qstr);
    event_free(state->read_event);
    event_free(state->write_event);
    free(state);
    state = NULL;
}

//循环写，直到写到指定长度，如果失败返回-1
int transfer_send_size(struct transfer_user *user, evutil_socket_t fd, unsigned char *buffer, uint16 sendsize)
{

    int sended = 0;
    while (sended < sendsize)
    {
        int sendret = send(fd, buffer + sended, sendsize - sended, 0);
        if (sendret < 0 && errno == EAGAIN)
        {
            //在connect dest后，由于是非阻塞，此时tcp连接可能还未建立，如果直接send，会返回-1
            //这里如果send缓冲区满了，就会循环等待。
            continue;
        }
        else if (sendret <= 0)
        {
            //   printf("server down\n");
            break;
        }
        sended += sendret;
    }

    if (sended >= sendsize)
        return sendsize;

    //client 断开连接或 recv出错
    user->cfg->client_nums--;
    printf("send %d\n", user->cfg->client_nums);
    event_del(user->read_event);

    free_transfer_user(user);
    return -1;
}

int readdiff = 0;

//循环读，直到读到指定长度，如果失败返回-1
int transfer_read_size(struct transfer_user *user, evutil_socket_t fd, unsigned char *buffer, uint16 recvsize)
{

    struct timeval start;
    struct timeval end;
    gettimeofday(&start, NULL);

    uint16 tempsize = 0;
    int result = 0;
    while (tempsize < recvsize)
    {
        result = recv(fd, buffer + tempsize, recvsize - tempsize, 0);
        if (result < 0 && errno == EAGAIN)
        {
            //   usleep(100);
            continue;
        }

        if (result <= 0)
            break;
        tempsize += result;
    }

    gettimeofday(&end, NULL);

    readdiff += (1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec);

    if (tempsize >= recvsize)
        return recvsize;

    printf("%d\n", readdiff);

    //client 断开连接或 recv出错
    user->cfg->client_nums--;
    printf("recv %d\n", user->cfg->client_nums);
    event_del(user->read_event);

    free_transfer_user(user);
    return -1;
}

int transfer_upload_send_nextpktnum(struct transfer_user *user, evutil_socket_t fd)
{
    unsigned char header[HEAD_SIZE];
    memset(header, 0, HEAD_SIZE);
    header[0] = UPLOAD_STOC_NEXTPKTNUM;
    header[2] = user->temp_recv_pktnum >> 8;
    header[3] = user->temp_recv_pktnum;

    return transfer_send_size(user, fd, header, HEAD_SIZE);
}

int transfer_upload_send_nextchunknum(struct transfer_user *user, evutil_socket_t fd)
{

    unsigned char header[HEAD_SIZE];
    memset(header, 0, HEAD_SIZE);
    header[0] = UPLOAD_STOC_CHUNKNUM;
    header[3] = user->temp_recv_chuncknum >> 8;
    header[2] = user->temp_recv_chuncknum;
    //发送一个chunknum时，pkt清零。
    user->temp_recv_pktnum = 0;
    //user->temp_recv_chuncknum++;
    return transfer_send_size(user, fd, header, HEAD_SIZE);
}

int writefile_onepkt(int filefd, int chunk_num, int pkt_num, unsigned char *buffer, int writesize)
{

    lseek(filefd, chunk_num * CHUNK_SIZE + pkt_num * BUFFER_SIZE, SEEK_SET);

    //    printf_debug(buffer, writesize);

    return write(filefd, buffer, writesize);
}

uint32 qstring_process_nonecopy(struct qstring *qstr)
{
    if (qstr->length < HEAD_SIZE)
        return RETURN_ERROR;
    struct qnode *q = qstr->head;
    if (q->end - q->start >= HEAD_SIZE)
    {
        uint16 type = *((uint16 *)(q->data + q->start));
        uint16 pktlen = *((uint16 *)(q->data + q->start + sizeof(uint16)));
        printf_debug(q->data + q->start, HEAD_SIZE);
    }
    qstring_remove(qstr, HEAD_SIZE);
    exit(0);

    return RETURN_SUCCESS;
}

void transfer_read(evutil_socket_t fd, short events, void *arg)
{
    struct transfer_packet *pkt = (struct transfer_packet *)malloc(sizeof(struct transfer_packet));
    memset(pkt, 0, sizeof(*pkt));
    struct transfer_user *user = (struct transfer_user *)arg;

    int recvret = qstring_recv_epoll_et(user->qstr, fd);

    printf("unprocess size:%lld\n", user->qstr->length);
    if ((recvret == RETURN_ERROR) || (recvret == RETURN_CLOSE))
    {
        free(pkt);
        pkt = NULL;
        printf("%d\n", recvret);
        user->cfg->client_nums--;
        printf("%d\n", user->cfg->client_nums);
        event_del(user->read_event);
        free_transfer_user(user);
        return;
    }
    int pktlen = 0;
    while (1)
    {
        int process_ret = qstring_process_copy(user->qstr, pkt, &pktlen);

        printf("unprocess size:%lld\n", user->qstr->length);

        if (process_ret == RETURN_ERROR)
        {
            free(pkt);
            pkt = NULL;
            return;
        }
        if (pkt->type == UPLOAD_CTOS_START)
        {
            struct fileinfo *finfo = (struct fileinfo *)(((unsigned char *)pkt) + HEAD_SIZE);
            printf("filesize:%lld, filename:%s\n", finfo->filesize, finfo->filename);
            memcpy(&user->finfo, finfo, sizeof(struct fileinfo));

            if ((user->filefd = open(finfo->filename, O_RDWR | O_CREAT, 0644)) == -1)
            {
                printf("open erro ！\n");
                exit(-1);
            }
            user->temp_recv_chuncknum = user->temp_recv_pktnum = 0;
            //向client发送要传输的文件块序号
            int r = transfer_upload_send_nextchunknum(user, fd);
        }
        else if (pkt->type == UPLOAD_CTOS_ONEPKT)
        {
            writefile_onepkt(user->filefd, user->temp_recv_chuncknum, user->temp_recv_pktnum, pkt->data, pkt->length);
            user->temp_recv_pktnum++;
            if (user->temp_recv_pktnum == PKTS_PER_CHUNK)
            {
                user->temp_recv_pktnum = 0;
                user->temp_recv_chuncknum++;
                printf("next chunk:%d\n", user->temp_recv_chuncknum);
                transfer_upload_send_nextchunknum(user, fd);
            }
            else
            {
                //    transfer_upload_send_nextpktnum(user, fd);
            }
        }
        else
        {
            free(pkt);
            pkt = NULL;
            printf("error header!!!!\n");
            user->cfg->client_nums--;
            printf("%d\n", user->cfg->client_nums);
            event_del(user->read_event);
            free_transfer_user(user);
            return;
        }
    }

    //qstring_process_nonecopy(user->qstr);
    exit(0);
    return;

    unsigned char header_buffer[BUFFER_SIZE + HEAD_SIZE];
    memset(pkt, 0, sizeof(*pkt));

    //接收报文头
    int ret = 0;
    //  ret += transfer_read_size(user, fd, pkt, sizeof(*pkt));
    if (ret == -1)
        return;

    uint16 datalength = (header_buffer[2] << 8) + header_buffer[3];

    //ret += transfer_read_size(user, fd, header_buffer + ret, datalength);

    printf_debug(header_buffer, HEAD_SIZE);

    //usleep(1000);

    //传输文件开始。发送文件名和文件大小
    if (header_buffer[1] == UPLOAD_CTOS_START)
    {
        struct fileinfo *finfo = (struct fileinfo *)(header_buffer + HEAD_SIZE);
        printf("filesize:%lld, filename:%s\n", finfo->filesize, finfo->filename);
        memcpy(&user->finfo, finfo, sizeof(struct fileinfo));

        if ((user->filefd = open(finfo->filename, O_RDWR | O_CREAT, 0644)) == -1)
        {
            printf("open erro ！\n");
            exit(-1);
        }
        user->temp_recv_chuncknum = user->temp_recv_pktnum = 0;
        int r = transfer_upload_send_nextchunknum(user, fd);
        // printf("%d\n", r);
    }
    else if (header_buffer[0] == UPLOAD_CTOS_ONEPKT)
    {
        //   usleep(1000);
        //    printf("\ndatalength: %d\n", datalength);
        writefile_onepkt(user->filefd, user->temp_recv_chuncknum, user->temp_recv_pktnum, header_buffer + HEAD_SIZE, datalength);
        user->temp_recv_pktnum++;
        if (user->temp_recv_pktnum == PKTS_PER_CHUNK)
        {
            user->temp_recv_pktnum = 0;
            user->temp_recv_chuncknum++;
            transfer_upload_send_nextchunknum(user, fd);
        }
        else
        {
            //    transfer_upload_send_nextpktnum(user, fd);
        }
    }
    else if (header_buffer[0] == UPLOAD_CTOS_LASTPKT)
    {
        // printf_debug(header_buffer + HEAD_SIZE, datalength);
        printf("c : %d %d\n", user->temp_recv_chuncknum, user->temp_recv_pktnum);
        // printf("\ndatalength: %d\n", datalength);
        writefile_onepkt(user->filefd, user->temp_recv_chuncknum, user->temp_recv_pktnum, header_buffer + HEAD_SIZE, datalength);
        close(user->filefd);
    }

    // int i;
    // uint32 recvsize = 0;
    // ssize_t result;
    // while (1)
    // {
    //     assert(user->write_event);
    //     result = recv(fd, header_buffer, HEAD_SIZE, 0);
    //     if (result <= 0)
    //         break;
    //     printf("%ld\n", result);

    //     uint16 nextlength = header_buffer[2] << 8 + header_buffer[3];
    //     result = recv(fd, header_buffer, HEAD_SIZE, 0);
    //     if (result <= 0)
    //         break;
    //     printf("%ld\n", result);
    // }

    // //当前数据已接收完。
    // if (result < 0 && errno == EAGAIN)
    //     return;

    // //client 断开连接或 recv出错
    // user->cfg->client_nums--;
    // printf("%d\n", user->cfg->client_nums);
    // event_del(user->read_event);

    // free_transfer_user(user);
}

void transfer_write(evutil_socket_t fd, short events, void *arg)
{
    // struct fd_state *state = (struct fd_state *)arg;

    // while (state->n_written < state->write_upto)
    // {
    //     ssize_t result = send(fd, state->buffer + state->n_written,
    //                           state->write_upto - state->n_written, 0);
    //     if (result < 0)
    //     {
    //         if (errno == EAGAIN) // XXX use evutil macro
    //             return;
    //         free_fd_state(state);
    //         return;
    //     }
    //     assert(result != 0);

    //     state->n_written += result;
    // }

    // if (state->n_written == state->buffer_used)
    //     state->n_written = state->write_upto = state->buffer_used = 1;

    // event_del(state->write_event);
}

struct transfer_user *
alloc_transfer_user(struct transfer_config *cfg, evutil_socket_t fd)
{
    struct event_base *base = cfg->base;

    struct transfer_user *user = (struct transfer_user *)malloc(sizeof(struct transfer_user));

    if (!user)
        return NULL;

    user->cfg = cfg;

    qstring_init(&user->qstr);

    user->read_event = event_new(base, fd, EV_READ | EV_PERSIST | EV_ET, transfer_read, user);
    if (!user->read_event)
    {
        free(user);
        user = NULL;
        return NULL;
    }
    user->write_event = event_new(base, fd, EV_WRITE | EV_PERSIST | EV_ET, transfer_write, user);

    if (!user->write_event)
    {
        event_free(user->read_event);
        free(user);
        user = NULL;
        return NULL;
    }

    user->buffer_used = user->n_written = user->write_upto = 0;

    assert(user->write_event);
    return user;
}

void transfer_accept(evutil_socket_t listener, short event, void *arg)
{

    struct transfer_config *cfg = (struct transfer_config *)arg;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    int fd = accept(listener, (struct sockaddr *)&ss, &slen);
    if (fd < 0)
    { // XXXX eagain??
        perror("accept");
    }
    else
    {
        cfg->client_nums++;
        printf("%d\n", cfg->client_nums);
        struct transfer_user *user;
        evutil_make_socket_nonblocking(fd);
        user = alloc_transfer_user(cfg, fd);
        assert(user); /*XXX err*/
        assert(user->write_event);
        event_add(user->read_event, NULL);
    }
}

int transfer_loop(struct transfer_config *cfg)
{

    evutil_socket_t listener = create_listener(cfg->ip_addr, cfg->port);

    assert(listener >= 0);
    struct event_base *base;
    struct event *listener_event;

    base = event_base_new();

    if (!base)
        return -1; /*XXXerr*/

    cfg->base = base;

    listener_event = event_new(base, listener, EV_READ | EV_PERSIST | EV_ET, transfer_accept, (void *)cfg);
    /*XXX check it */
    event_add(listener_event, NULL);

    event_base_dispatch(base);
}