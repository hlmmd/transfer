
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


int qstring_init(struct qstring **qstr)
{
    *qstr = (struct qstring *)malloc(sizeof(struct qstring));
    if (*qstr == NULL)
        return RETURN_ERROR;

    (*qstr)->node_num = 0;
    (*qstr)->length = 0;
    (*qstr)->head = (struct qnode *)malloc(sizeof(struct qnode));
    memset((*qstr)->head, 0, sizeof(struct qnode));
    (*qstr)->tail = (*qstr)->head;
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
    if (recvret < 0 && (errno == EAGAIN))
        return RETURN_SUCCESS;

    //对端断开
    if (recvret == 0)
    {
        close(socket_fd);
        return RETURN_SUCCESS;
    }

    return RETURN_ERROR;
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
            qstr->head = q;
            free(temp);
            qstr->node_num--;
        }
    }

    //更新长度
    qstr->length -= len;

    return RETURN_SUCCESS;
}

int main()
{







    struct qstring *qstr;

    //test qstring_init
    qstring_init(&qstr);

    char buffer[4096];
    for (int i = 0; i < 64; i++)
    {
        qstring_insert(qstr, buffer, 4096);
    }
    printf("%d\n", qstr->node_num);
    printf("%lld\n", qstr->length);

    qstring_remove(qstr, 41341);
    printf("%d\n", qstr->node_num);
    printf("%lld\n", qstr->length);

    for (int i = 0; i < 64; i++)
    {
        qstring_insert(qstr, buffer, 4095);
    }
    printf("%d\n", qstr->node_num);
    printf("%lld\n", qstr->length);

    printf("%d %d\n", qstr->head->start, qstr->head->end);
    printf("%d %d\n", qstr->tail->start, qstr->tail->end);

    return 0;
}