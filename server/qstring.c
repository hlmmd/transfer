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

#include "qstring.h"

int qstring_init(struct qstring **qstr)
{
    *qstr = (struct qstring *)malloc(sizeof(struct qstring));
    if (*qstr == NULL)
        return RETURN_ERROR;

    (*qstr)->node_num = 0;
    (*qstr)->length = 0;
    (*qstr)->head = (struct qnode *)malloc(sizeof(struct qnode));
    memset((*qstr)->head, 0, sizeof(struct qnode));

    (*qstr)->head->start = (*qstr)->head->end = QNODE_SIZE - 2;

    (*qstr)->tail = (*qstr)->head;
    return RETURN_SUCCESS;
}

int qstring_free(struct qstring **qstr)
{
    struct qnode *q = (*qstr)->head;
    while (q)
    {
        struct qnode *temp = q;
        q = q->next;
        free(temp);
        temp = NULL;
    }
    free(*qstr);
    *qstr = NULL;
    return RETURN_SUCCESS;
}

uint64 qstring_length(struct qstring *qstr)
{
    return qstr->length;
}

int qstring_insert(struct qstring *qstr, void *buffer, uint32 buffer_len)
{
    struct qnode *q = qstr->tail;

    uint32 insert_len = 0;
    while (insert_len < buffer_len)
    {
        //比较剩余长度和当前结点剩余长度大小，写入较小长度的数据
        uint32 remain_len = buffer_len - insert_len;
        uint32 qnode_remain = QNODE_SIZE - q->end;
        uint32 to_insert_len = remain_len > qnode_remain ? qnode_remain : remain_len;

        memcpy(q->data + q->end, buffer + insert_len, to_insert_len);
        q->end += to_insert_len;
        insert_len += to_insert_len;

        //当前结点缓冲区满，申请下一结点
        if (q->end == QNODE_SIZE)
        {
            q->next = (struct qnode *)malloc(sizeof(struct qnode));
            if (q->next == NULL)
                exit(0);
            q = q->next;
            memset(q, 0, sizeof(struct qnode));
            qstr->node_num++;
        }
    }

    //更新长度
    qstr->tail = q;
    qstr->length += buffer_len;
    return RETURN_SUCCESS;
}

int qstring_recv_epoll_et(struct qstring *qstr, int socket_fd)
{
    struct qnode *q = qstr->tail;
    int recvret = 0;
    while (1)
    {
        uint32 qnode_remain = QNODE_SIZE - q->end;
        recvret = recv(socket_fd, q->data + q->end, qnode_remain, 0);
        //客户端退出或者读完
        if (recvret <= 0)
        {
            break;
        }
        q->end += recvret;
        qstr->length += recvret;
        //当前结点缓冲区满，申请下一结点
        if (q->end == QNODE_SIZE)
        {
            q->next = (struct qnode *)malloc(sizeof(struct qnode));
            if (q->next == NULL)
                exit(0);
            q = q->next;
            memset(q, 0, sizeof(struct qnode));
            qstr->tail = q;
            qstr->node_num++;
        }
    }

    //当前socket缓冲区已经读完
    if (recvret < 0 && (errno == EAGAIN || errno == EINPROGRESS))
        return RETURN_SUCCESS;

    //对端断开
    if (recvret == 0)
    {
        return RETURN_CLOSE;
    }

    return RETURN_ERROR;
}

uint32 qstring_process_copy(struct qstring *qstr, void *buffer, int *copylen)
{
    if (qstr->length < HEAD_SIZE)
        return RETURN_ERROR;

    struct qnode *q = qstr->head;
    uint32 to_process = HEAD_SIZE;

    bool next_node = false;

    //读报文头
    uint32 qlen = q->end - q->start;
    if (qlen <= HEAD_SIZE && (q->end == QNODE_SIZE))
    {
        next_node = true;
        memcpy(buffer, q->data + q->start, qlen);
        if (qlen != HEAD_SIZE)
            memcpy(buffer + qlen, q->next->data, HEAD_SIZE - qlen);
    }
    else
    {
        memcpy(buffer, q->data + q->start, HEAD_SIZE);
    }
    uint16 type = *((uint16 *)(buffer));
    uint16 pktlen = *((uint16 *)(buffer + sizeof(uint16)));

    //未接收到制定的长度，不处理
    if ((pktlen + HEAD_SIZE) > qstr->length)
        return RETURN_ERROR;

    //读取报文头时读到了下一个块
    if (next_node == true)
    {
        next_node = false;
        struct qnode *temp = q;
        q = q->next;
        q->start = HEAD_SIZE - qlen;
        free(temp);
        temp = NULL;
        qstr->node_num--;
    }
    else
    {
        q->start += HEAD_SIZE;
    }

    //读取剩下的长度
    to_process = pktlen;

    qlen = q->end - q->start;
    if (qlen <= to_process && (q->end == QNODE_SIZE))
    {
        next_node = true;
        memcpy(buffer + HEAD_SIZE, q->data + q->start, qlen);
        if (qlen != to_process)
            memcpy(buffer + HEAD_SIZE + qlen, q->next->data, to_process - qlen);
    }
    else
    {
        memcpy(buffer + HEAD_SIZE, q->data + q->start, to_process);
    }
    //读取报文时读到了下一个块
    if (next_node == true)
    {
        struct qnode *temp = q;
        q = q->next;
        q->start = to_process - qlen;
        free(temp);
        temp = NULL;
        qstr->node_num--;
    }
    else
    {
        q->start += to_process;
    }

    *copylen = HEAD_SIZE + pktlen;
    qstr->length -= (HEAD_SIZE + pktlen);

    qstr->head = q;

    return RETURN_SUCCESS;
}

uint32 qstring_remove(struct qstring *qstr, uint32 len)
{
    if (qstr->length < len)
        return RETURN_ERROR;

    //从头部删除
    struct qnode *q = qstr->head;

    uint32 removed_len = 0;
    while (removed_len < len)
    {
        uint32 remain_len = len - removed_len;
        uint32 qnode_len = q->end - q->start;
        uint32 to_remove_len = remain_len > qnode_len ? qnode_len : remain_len;

        q->start += to_remove_len;
        removed_len += to_remove_len;

        //当前结点缓冲区读完，删除
        if (q->end == q->start && q->end == QNODE_SIZE)
        {
            struct qnode *temp = q;
            q = q->next;
            free(temp);
            qstr->node_num--;
        }
    }

    //更新长度
    qstr->length -= len;
    qstr->head = q;

    return RETURN_SUCCESS;
}

// #include "tqueue.h"

// int tqueue_init(struct tqueue **head)
// {
//     *head = (struct tqueue *)malloc(sizeof(struct tqueue));
//     if (*head == NULL)
//         return -1;
//     struct tqueue *q = *head;
//     q->head = (struct tqnode *)malloc(sizeof(struct tqnode));
//     memset(q->head, 0, sizeof(struct tqnode));
//     //q->length = 0;
//     q->tail = head;

//     return 0;
// }

// int tqueue_enqueue(struct tqueue *qu, struct transfer_packet *pkt, int pktlen)
// {
//     struct tqnode *tq = (struct tqnode *)malloc(sizeof(struct tqnode));
//     memset(tq, 0, sizeof(*tq));
//     tq->end = pktlen;
//     tq->pkt = pkt;

//     struct tqnode *temp = qu->tail;
//     temp->next = tq;

//     return 0;
// }
