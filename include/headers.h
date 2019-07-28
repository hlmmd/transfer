#ifndef INCLUDE_HEADERS
#define INCLUDE_HEADERS

//所有报文头长度为4，包括1位报文类型，1位填充字节，2位报文长度（不包括报文头）
#define HEAD_SIZE 4

#define BUFFER_SIZE 4096

/* 上传 client -> server 请求报文头 */

//开始上传一个文件，后接文件名。
#define UPLOAD_CTOS_START 0x20

//上传一个块
#define UPLOAD_CTOS_ONECHUNK 0x21

//上传最后一个块
#define UPLOAD_CTOS_LASTCHUNK 0x22

/* 上传 server -> client 响应报文头 */

//请求文件块序号
#define UPLOAD_STOC_CHUNKNUM 0x30

//回传收到的文件校验和
#define UPLOAD_STOC_FILECHECKSUM 0x32

//拒绝请求
#define UPLOAD_STOC_REFUST 0x3F

/*  下载 client -> server 请求报文头 */

/*  下载 server -> client 响应报文头 */

#endif