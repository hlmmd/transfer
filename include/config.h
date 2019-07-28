#ifndef INCLUDE_CONFIG
#define INCLUDE_CONFIG

#define USER_LIMIT 65535

#define FILENAME_MAXLEN 128

#define CHUNK_SIZE 4 * 1024 * 1024 //4M

#include "headers.h"

#include <event2/event.h>

#ifndef __cplusplus
typedef enum
{
    false = 0,
    true = 1
} bool;
#endif

typedef unsigned char uint8;
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned long long int uint64;

struct transfer_config
{
    struct event_base *base;
    uint32 client_nums;
    uint32 ip_addr;
    uint16 port;
};

struct transfer_user
{
    struct transfer_config *cfg;
    char buffer[HEAD_SIZE + BUFFER_SIZE];
    uint32 buffer_used;

    uint32 n_written;
    uint32 write_upto;

    struct event *read_event;
    struct event *write_event;
};

/*文件信息*/
struct fileinfo
{
    uint64 filesize;                   //文件大小
    char filename[FILENAME_MAXLEN]; //文件名
    int chunknum;                   //分块数量
    //int chun;                     //标准分块大小
};

#endif