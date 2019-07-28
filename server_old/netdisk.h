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

#define RECVBUFSIZE 1500  /*����buf��С*/
#define SENDBUFSIZE 1500  /*����buf��С*/
#define NAMESIZE 20  /*�û�������*/
#define PASSWORDSIZE 50  /*�û����볤��*/
#define SQLBUFSIZE 1500 /*sql��仺���С*/

#define FILETYPE 0x00    /*�ļ���־*/
#define DIRTYPE 0x11     /*Ŀ¼��־*/
#define FILENAMESIZE 128  /*�ļ�����С*/
#define FILEBLOCKSIZE 512 /*�ļ����С*/
#define PERPACKNUM  8
#define DIRNAMESIZE 128  /*Ŀ¼����С*/

#define SECONDS 10  /*��ʱ10sû���յ��ذ��ش�*/
#define ENDSECONDS 2
#define TIMES 5    /*�ش�5����Ϊʧ��*/

#define HEART_DELAY 60

#define HEART_REQ            0x60       //���������
#define HEART_ACK            0x70       //�����ذ�
/*����*/
#define ERROR 0xff
  
/*��¼*/
#define SIGNINREQ 0x00  /*��¼����*/
#define SIGNINOK 0x10  
#define SIGNINERR 0x11

#define ERRUSERNAME 0x10  
#define ERRPASSWORD 0x11

/*ע��*/
#define SIGNUPREQ 0x02
#define SIGNUPOK 0x12
#define SIGNUPERR 0x13

/*ע��*/
#define SIGNOUTREQ 0x04
#define SIGNOUTOK 0x14

/*�û����ļ�ϵͳ*/
#define USERFSREQ 0x05  /*�û����ļ�ϵͳ����*/
#define USERFSPORTSEND 0x15    /*�û����ļ�ϵͳ �������˿ڷ��� ��Ϊ��¼ȷ�ϰ�*/
#define USERFSPACK     0x16 /*�û����ļ�ϵͳ��*/
#define USERFSACK  0x07  /*ȷ�ϰ�*/
#define USERFSEND      0x17  /*������*/

/*�ϴ�*/
#define UPFILEREQ 0x20  /*�ϴ��ļ�����*/
#define UPSEC 0x30      /*�봫�ذ�*/
#define UPPORTSEND 0x31 /*�˿ڰ�*/
#define UPBEGIN 0x22   /*�ϴ���ʼ��*/
#define UPSENDPACKID 0x33	/*���������Ͱ����*/
#define UPFILEPACK 0x23 /*�ͻ����ϴ���*/
#define UPFILEEND 0x34	/*�ϴ�����*/
						
#define UPDIRREQ 0x25	/*�ϴ�Ŀ¼����*/
#define UPDIRACK 0x35	/*Ŀ¼�봫*/

/*����*/
#define DOWNFILEREQ 0x40    /*�����ļ�����*/
#define DOWNFILEINFOACK 0x50   /*�ļ���Ϣ�ذ�*/
#define DOWNSEC  0x41  /*����*/
#define DOWNFILE 0x42  /*��ʼ�����ļ�*/
#define DOWNPORTSEND  0x52 /*�˿ڷ����*/
#define DOWNFILEBLOCKREQ 0x43 /*�����ļ����*/
#define DOWNFILEBLOCK 0x53  /*�����ļ���*/
#define DOWNFILEEND  0x44   /*���ؽ���*/

/*����*/
#define CHANGEFILENAMEREQ 0x67
#define CHANGEFILENAMEACK 0x77

/*�����ļ�*/
#define COPYFILEREQ 0x61
#define COPYFILEACK 0x71

/*�����ļ���*/
#define COPYDIRREQ 0x62
#define COPYDIRACK 0x72

/*�ƶ��ļ�*/
#define MOVEFILEREQ 0x63
#define MOVEFILEACK 0x73

/*�ƶ��ļ���*/
#define MOVEDIRREQ 0x64
#define MOVEDIRACK 0x74

/*ɾ���ļ�*/
#define DELETEFILEREQ 0x65
#define DELETEFILEACK 0x75

/*ɾ���ļ���*/
#define DELETEDIRREQ 0x66
#define DELETEDIRACK 0x76

int now(char* datetime);
int CopyDir(MYSQL* mysql, long int dirid, long int ndirid);
int DeleteDir(MYSQL* mysql, long int dirid);

/*���������û��ļ�ϵͳ�����ݽṹ�뺯������*/
typedef struct FileInfoNode{
	long int pdirid;    /*��Ŀ¼id*/
	long int fileid;    /*�ļ�id*/
	char type;          /*�ļ����� FILE or DIR*/
	long int filesize;  /*�ļ���С*/
	char filename[FILENAMESIZE]; /*�ļ���*/
	struct FileInfoNode* next;
}FileInfoNode, *FileInfoList;

int InitFileInfoList(FileInfoList* L, long int rootdirid);
int DestoryFileInfoList(FileInfoList* L);
int InsertFileInfoList(FileInfoList* L, long int pdirid, long int fileid, char type, long int filesize, char* filename);
int SortFileInfoList(FileInfoList* L);
int SearchAllFileInfo(FileInfoList* L, MYSQL* mysql, long int dirid);
int SendUserFS(int sockfd, struct sockaddr_in* client_addr, long int rootdirid, unsigned short port);

/*���û������ļ�*/
int SendUserFile(int sockfd, struct sockaddr_in* client_addr, long int userfileid, unsigned short port);

/*�ϴ��ļ������ݽṹ�뺯������*/
#define COMPLETEFILE 0x01  /*�����ļ�*/
#define NOTCOMPLETEFILE 0x00 /*�������ļ�*/
#define NETDISKFILEPATH "/var/netdisk/"   /*�洢�ļ���·��*/

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
