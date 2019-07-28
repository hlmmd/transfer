#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <fcntl.h>
#include <signal.h>
#include <mysql.h>
#include <sys/select.h>
#include <time.h>
#include <sys/stat.h>

#define RECVBUFSIZE 1500  /*接收buf大小*/
#define SENDBUFSIZE 1500  /*发送buf大小*/
#define NAMESIZE 20  /*用户名长度*/
#define PASSWORDSIZE 50  /*用户密码长度*/
#define SQLBUFSIZE 1500 /*sql语句缓存大小*/

#define FILETYPE 0x00    /*文件标志*/
#define DIRTYPE 0x11     /*目录标志*/
#define FILENAMESIZE 128  /*文件名大小*/
#define FILEBLOCKSIZE 512 /*文件块大小*/
#define PERPACKNUM  8
#define DIRNAMESIZE 128  /*目录名大小*/

#define SECONDS 10  /*超时10s没有收到回包重传*/
#define ENDSECONDS 2
#define TIMES 5    /*重传5次认为失败*/

#define HEART_DELAY 60

#define HEART_REQ            0x60       //心跳请求包
#define HEART_ACK            0x70       //心跳回包
/*错误*/
#define ERROR 0xff
  
/*登录*/
#define SIGNINREQ 0x00  /*登录请求*/
#define SIGNINOK 0x10  
#define SIGNINERR 0x11

#define ERRUSERNAME 0x10  
#define ERRPASSWORD 0x11

/*注册*/
#define SIGNUPREQ 0x02
#define SIGNUPOK 0x12
#define SIGNUPERR 0x13

/*注销*/
#define SIGNOUTREQ 0x04
#define SIGNOUTOK 0x14

/*用户虚文件系统*/
#define USERFSREQ 0x05  /*用户虚文件系统请求*/
#define USERFSPORTSEND 0x15    /*用户虚文件系统 服务器端口分配 作为登录确认包*/
#define USERFSPACK     0x16 /*用户虚文件系统包*/
#define USERFSACK  0x07  /*确认包*/
#define USERFSEND      0x17  /*结束包*/

/*上传*/
#define UPFILEREQ 0x20  /*上传文件请求*/
#define UPSEC 0x30      /*秒传回包*/
#define UPPORTSEND 0x31 /*端口包*/
#define UPBEGIN 0x22   /*上传开始包*/
#define UPSENDPACKID 0x33	/*服务器发送包序号*/
#define UPFILEPACK 0x23 /*客户端上传包*/
#define UPFILEEND 0x34	/*上传结束*/
						
#define UPDIRREQ 0x25	/*上传目录请求*/
#define UPDIRACK 0x35	/*目录秒传*/

/*下载*/
#define DOWNFILEREQ 0x40    /*下载文件请求*/
#define DOWNFILEINFOACK 0x50   /*文件信息回包*/
#define DOWNSEC  0x41  /*秒下*/
#define DOWNFILE 0x42  /*开始下载文件*/
#define DOWNPORTSEND  0x52 /*端口分配包*/
#define DOWNFILEBLOCKREQ 0x43 /*请求文件块包*/
#define DOWNFILEBLOCK 0x53  /*发送文件块*/
#define DOWNFILEEND  0x44   /*下载结束*/

/*改名*/
#define CHANGEFILENAMEREQ 0x67
#define CHANGEFILENAMEACK 0x77

/*复制文件*/
#define COPYFILEREQ 0x61
#define COPYFILEACK 0x71

/*复制文件夹*/
#define COPYDIRREQ 0x62
#define COPYDIRACK 0x72

/*移动文件*/
#define MOVEFILEREQ 0x63
#define MOVEFILEACK 0x73

/*移动文件夹*/
#define MOVEDIRREQ 0x64
#define MOVEDIRACK 0x74

/*删除文件*/
#define DELETEFILEREQ 0x65
#define DELETEFILEACK 0x75

/*删除文件夹*/
#define DELETEDIRREQ 0x66
#define DELETEDIRACK 0x76

int now(char* datetime);
int CopyDir(MYSQL* mysql, long int dirid, long int ndirid);
int DeleteDir(MYSQL* mysql, long int dirid);

/*传送虚拟用户文件系统的数据结构与函数定义*/
typedef struct FileInfoNode{
	long int pdirid;    /*父目录id*/
	long int fileid;    /*文件id*/
	char type;          /*文件种类 FILE or DIR*/
	long int filesize;  /*文件大小*/
	char filename[FILENAMESIZE]; /*文件名*/
	struct FileInfoNode* next;
}FileInfoNode, *FileInfoList;

int InitFileInfoList(FileInfoList* L, long int rootdirid);
int DestoryFileInfoList(FileInfoList* L);
int InsertFileInfoList(FileInfoList* L, long int pdirid, long int fileid, char type, long int filesize, char* filename);
int SortFileInfoList(FileInfoList* L);
int SearchAllFileInfo(FileInfoList* L, MYSQL* mysql, long int dirid);
int SendUserFS(int sockfd, struct sockaddr_in* client_addr, long int rootdirid, unsigned short port);

/*给用户发送文件*/
int SendUserFile(int sockfd, struct sockaddr_in* client_addr, long int userfileid, unsigned short port);

/*上传文件的数据结构与函数定义*/
#define COMPLETEFILE 0x01  /*完整文件*/
#define NOTCOMPLETEFILE 0x00 /*非完整文件*/
#define NETDISKFILEPATH "/var/netdisk/"   /*存储文件的路径*/

int UploadFile(int sockfd, struct sockaddr_in* client_addr, long int realfileid, unsigned short port, int beginpackid, long int filesize, char* md5);


/**/
int Select(int maxfdp1, fd_set* rfds, fd_set* wfds, fd_set* efds, struct timeval* tv)
{
	int retselect;
	while(1){
		retselect = select(maxfdp1, rfds, wfds, efds, tv);
		if(retselect == -1 && errno == EINTR)
			continue;
		else
			break;
	}
	return retselect;
}
