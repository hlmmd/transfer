#ifndef INCLUDE_QSTRING
#define INCLUDE_QSTRING

#include "config.h"

#define QNODE_SIZE (65536)

struct qnode
{
    uint32 start;
    uint32 end;
    unsigned char data[QNODE_SIZE];
    struct qnode *next;
};

struct qstring
{
    uint64 length;
    uint32 node_num;
    struct qnode *head;
    struct qnode *tail;
};


int qstring_init(struct qstring **qstr);
uint64 qstring_length(struct qstring *qstr);

int qstring_insert(struct qstring *qstr, void *buffer, uint32 buffer_len);
int qstring_recv_epoll_et(struct qstring *qstr, int socket_fd);
uint32 qstring_remove(struct qstring *qstr, uint32 len);

#endif

// #ifndef INCLUDE_TQUEUE
// #define INCLUDE_TQUEUE

// #include "config.h"

// struct tqnode
// {
//     uint16 start;
//     uint16 end;
//     struct transfer_packet *pkt;
//     struct tqnode *next;
// };

// struct tqueue
// {
//     struct tqnode *head;
//     struct tqnode *tail;
//     //   uint32 length;
// };

// int tqueue_init(struct tqueue **head);
// int tqueue_enqueue(struct tqueue *qu, struct transfer_packet *pkt, int pktlen);

// #endif