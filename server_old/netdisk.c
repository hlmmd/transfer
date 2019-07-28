#include "netdisk.h"
#include "PortCtrl.h"
#include "aes.h"
#include "md5.h"
#include "UserList.h"
#include "ChildList.h"
#include "UserChildList.h"

ChildList CL; //子进程链表
PortList pctrl;

const md5_char[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	
void my_fun(int signo)
{
	int pid;
	unsigned short port;
	
	signal(signo, my_fun);
	while((pid = waitpid(-1, NULL, WNOHANG)) > 0){
		ChildListGetPortByPid(CL, pid, &port);
		if(ChildListDelete(&CL, pid) < 0) {
			printf("find pid in port list error\n");
			continue;
		}
		PortRelease(&pctrl, port);
	}
}

int main(int argc, char* argv[])
{
	struct sockaddr_in host_addr, client_addr;
	int sockfd;
	unsigned short port = 8080;
	int flag;
	
	char recvbuf[RECVBUFSIZE];
	char sendbuf[SENDBUFSIZE];
	int retrecv;
	int addrlen;
	
	int i, j;
	char* itoctemp;
	int userid;
	long int rootdirid;
	long int userfileid;
	long int realfileid;
	long int pdirid;
	long int dirid;
	long int filesize;
	char dirname[DIRNAMESIZE];
	char filename[FILENAMESIZE];
	unsigned char md5temp[20];
	char mtoftemp[3];
	char md5[50];
	
	char timenow[20];
	int loguserid;

	long int nuserfileid;
	long int ndirid;
	char nfilename[FILENAMESIZE];
	
	char full;  /*文件完整性*/
	int packid;
	
	int pid;
	struct timeval begin;
	struct timeval end;

	UserList UL;  //用户链表
	UserList p;
	
	int flags;
	fd_set rfds;
	int retselect;
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	
	char username[NAMESIZE];  //用户名字
	char password[PASSWORDSIZE];  //密码
	char sqlbuf[SQLBUFSIZE];//用于登录时查询数据库
	
	short childport;
	
	MYSQL     *mysql;
	MYSQL_RES *result;
	MYSQL_ROW row;
	
	PortInit(&pctrl);
	
	if ((mysql = mysql_init(NULL))==NULL) {
		printf("mysql_init failed\n");
		exit(-1);
	}

	while (mysql_real_connect(mysql,"localhost","root", "root123", "OnlineDisk",0, NULL, 0)==NULL) {
		printf("mysql_real_connect failed(%d)\n", mysql_error(mysql));
		sleep(1);
	}

	mysql_set_character_set(mysql, "gbk"); 
	
	memset(&host_addr, 0, sizeof(host_addr));
	host_addr.sin_family = AF_INET;
	host_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	host_addr.sin_port = htons(port);
	
	if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("create socket error");
		exit(-1);
	}
	
	flags = fcntl(sockfd, F_GETFL, 0);
	fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

	//设置udp缓冲区大小
	//setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));  
	
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1) 
	{ 
		perror("setsockopt"); 
		exit(1); 
	}
	
	if(bind(sockfd, (struct sockaddr*)&host_addr, sizeof(host_addr)) == -1){
		perror("bind error");
		exit(-1);
	}

	UserListInit(&UL);  //用户链表初始化
	ChildListInit(&CL); //子进程链表初始化
	
	gettimeofday(&begin, NULL);

	while(1) {
		gettimeofday(&end, NULL);
		if ((end.tv_sec - begin.tv_sec) > HEART_DELAY)
		{
			gettimeofday(&begin, NULL);
			p = UL->Next;
			while (p)
			{
				if (p->Heart == 1)
				{	
					sendbuf[0] = HEART_REQ;
					sendbuf[1] = 14;
					client_addr.sin_addr.s_addr = htonl(p->IPaddr);
					client_addr.sin_port = htons(p->Port);
					Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
				}
				else if (p->Heart > 1)
				{
					printf("User  %d Timeout\n", p->Port);
					UserListDeletebyIP(&UL, p->IPaddr, p->Port);
					p = UL->Next;
					continue;
				}
				UserListSetHeart(&UL, p->IPaddr, p->Port, p->Heart + 1);
				p = p->Next;
			}
		}
		FD_ZERO(&rfds);
		FD_SET(sockfd, &rfds);
		tv.tv_sec = SECONDS;
		tv.tv_usec = 0;
		retselect = Select(sockfd+1, &rfds, NULL, NULL, &tv);
		if(retselect == -1){
			perror("select error");
			exit(-1);
		}
		else if(retselect == 0){
			continue;
		}
		
		if(FD_ISSET(sockfd, &rfds)){
			addrlen = sizeof(client_addr);
			retrecv = Recvfrom(sockfd, recvbuf, RECVBUFSIZE, 0, (struct sockaddr*)&client_addr, &addrlen);
			if(retrecv < 0) {
				perror("recvfrom error");
				continue;
			}
			if(retrecv < 2)
				continue;
		
			switch(recvbuf[0]){
				/*登录请求*/
				case SIGNINREQ:
					/*获取用户名和密码*/
					for(i=0; i<recvbuf[2]; i++)
						username[i] = recvbuf[4+i];
					username[i] = '\0';
					for(j=0; j<recvbuf[3]; j++)
						password[j] = recvbuf[4+i+j];
					password[j] = '\0';
					sprintf(sqlbuf, "select * from User where User_Name = '%s' and User_Password = MD5('%s');", username, password);
					/*登录查询数据库*/
					if(mysql_query(mysql, sqlbuf)){
						printf("error select User table\n");
						return -1;
					}
					if ((result = mysql_store_result(mysql))==NULL) {
						printf("mysql_store_result failed\n" );
						return -1;
					}
					if(mysql_num_rows(result) > 0){ 
						/*用户名密码正确*/
						/*发送虚目录*/
						/*获取用户根目录id*/
						row = mysql_fetch_row(result); 
						rootdirid = atoi(row[3]);
						userid = atoi(row[0]);
						mysql_free_result(result); 
						
						now(timenow);
						timenow[19]='\0';
						sprintf(sqlbuf, "insert into Log values(null, %d, '%s', 1, 0);", userid, timenow);
						if(mysql_query(mysql, sqlbuf)){
							printf("error insert Log table\n");
							return -1;	
						}

						if (UserListIfExists(UL, userid) == TRUE)
						{
							printf("User %s already login\n", row[1]);
							sendbuf[0] = SIGNINERR; 
							Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
							break;
						}
						/*分配端口*/
						if((childport = PortAssign(&pctrl)) == -1){
							printf("port assign error\n");
							break;
						}

						UserListInsert(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port), userid);
						printf("User  %s  %d Login\n", row[1], ntohs(client_addr.sin_port));

						pid = fork();
						if(pid < 0) {
							PortRelease(&pctrl, childport);
							perror("fork error");
							break;
						}
						if (pid > 0)
						{
							signal(SIGCHLD, my_fun);
							ChildListInsert(&CL, pid, childport, NULL);
						}
						else if(pid == 0){
							/*发送用户虚拟文件*/
							SendUserFS(sockfd, &client_addr, rootdirid, childport);
							exit(0);
						}
						/*插入到端口序列*/
					}
					else{
						char buff[16];
						mysql_free_result(result); 
						/*登录失败，发送失败回包*/
						sendbuf[0] = SIGNINERR; 

						sprintf(sqlbuf, "select * from User where User_Name = '%s';", username);
						/*登录查询数据库*/
						if(mysql_query(mysql, sqlbuf)){
							printf("error select User table\n");
							return -1;
						}
						if ((result = mysql_store_result(mysql))==NULL) {
							printf("mysql_store_result failed\n" );
							return -1;
						}
						if(mysql_num_rows(result) <= 0)
							sendbuf[1] = ERRUSERNAME; 

						else
							sendbuf[1] = ERRPASSWORD; 

						encrypt(sendbuf, buff, 16);
						mysql_free_result(result); 

						sendto(sockfd, buff, 16, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
						
					}
					break;
					
				/*注册请求*/
				case SIGNUPREQ:
					/*获取用户名和密码*/
					for(i=0; i<recvbuf[2]; i++)
						username[i] = recvbuf[4+i];
					username[i] = '\0';
					for(j=0; j<recvbuf[3]; j++)
						password[j] = recvbuf[4+i+j];
					password[j] = '\0';
					sprintf(sqlbuf, "select * from User where User_Name = '%s';", username);
					/*查询数据库检查用户名是否存在*/
					if(mysql_query(mysql, sqlbuf)){
						printf("error select User table\n");
						return -1;
					}
					if ((result = mysql_store_result(mysql))==NULL) {
						printf("mysql_store_result failed\n" );
						return -1;
					}
					if(mysql_num_rows(result) == 1){
						mysql_free_result(result);
						sprintf(sqlbuf, "select * from User where User_Name = '%s' and User_Password = MD5('%s');", username, password);
						if(mysql_query(mysql, sqlbuf)){
							printf("error select User table\n");
							return -1;
						}
						if ((result = mysql_store_result(mysql))==NULL) {
							printf("mysql_store_result failed\n" );
							return -1;
						}
						if(mysql_num_rows(result) == 1){
							mysql_free_result(result);
							/*用户名密码与数据库中的记录一致，认为是重传包，发送注册成功包*/
							sendbuf[0] = SIGNUPOK;  
							Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
							
						}
						else {/*注册失败，用户名已存在*/
							mysql_free_result(result);
							sendbuf[0] = SIGNUPERR; 

							Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
							
						}
					}
					else{
						mysql_free_result(result);
						/*注册成功，发包*/
						sendbuf[0] = SIGNUPOK;
						Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
						
						/*注册成功，写数据库*/
						sprintf(sqlbuf, "insert into Directory values(null, '%s', null);", username);
						if(mysql_query(mysql, sqlbuf)){
							printf("error insert Directory table\n");
							return -1;
						}
						sprintf(sqlbuf, "select Directory_ID from Directory where Directory_Name = '%s' and Directory_parent is null;", username);
						if(mysql_query(mysql, sqlbuf)){
							printf("error select Directory table\n");
							return -1;
						}
						if ((result = mysql_store_result(mysql))==NULL) {
							printf("mysql_store_result failed\n" );
							return -1;
						}
						row=mysql_fetch_row(result);  //获取用户根目录id
						sprintf(sqlbuf, "insert into User values(null, '%s', MD5('%s'), '%s');", username, password, row[0]);
						if(mysql_query(mysql, sqlbuf)){
							printf("error insert User table\n");
							return -1;
						}
						mysql_free_result(result);
						
						sprintf(sqlbuf, "select * from User where User_Name = '%s';", username);
						if(mysql_query(mysql, sqlbuf)){
							printf("error insert User table\n");
							return -1;
						}
						if ((result = mysql_store_result(mysql))==NULL) {
							printf("mysql_store_result failed\n" );
							return -1;
						}
						row=mysql_fetch_row(result);
						userid = atoi(row[0]);
						mysql_free_result(result);
						
						
						now(timenow);
						timenow[19]='\0';
						sprintf(sqlbuf, "insert into Log values(null, %d, '%s', 2, 0);", userid, timenow);
						if(mysql_query(mysql, sqlbuf)){
							printf("error insert Log table\n");
							return -1;
						}
					} 
					break;
					
				/*注销请求*/
				case SIGNOUTREQ:
					userid = UserListRetUserID(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port));
					now(timenow);
					timenow[19]='\0';
					sprintf(sqlbuf, "insert into Log values(null, %d, '%s', 3, 0);", userid, timenow);
					if(mysql_query(mysql, sqlbuf)){
						printf("error insert Log table\n");
						return -1;
					}
					
					printf("User  %d Logout\n", ntohs(client_addr.sin_port));
					UserListDeletebyIP(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port));
				
					sendbuf[0] = SIGNOUTOK;
					Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
					
					break;
				
				/*用户虚拟文件系统请求*/
				case USERFSREQ:
					if (UserListIfExistsbyIP(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port)) == FALSE) //用户未登录
					{
						sendbuf[0] = ERROR;
						sendbuf[1] = 14;
						Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
						break;
					}

					UserListSetHeart(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port), 0);
					
					userid = UserListRetUserID(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port));
					now(timenow);
					timenow[19]='\0';
					sprintf(sqlbuf, "insert into Log values(null, %d, '%s', 4, 0);", userid, timenow);
					if(mysql_query(mysql, sqlbuf)){
						printf("error insert Log table\n");
						return -1;
					}
					
					/*获取用户根目录id*/
					itoctemp = (char*)&rootdirid;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[j+2];
					
					/*分配端口*/
					if((childport = PortAssign(&pctrl)) == -1){
						printf("port assign error\n");
						break;
					}
					pid = fork();
					if(pid < 0) {
						PortRelease(&pctrl, childport);
						perror("fork error");
						break;
					}
					if (pid > 0)
					{
						signal(SIGCHLD, my_fun);
						ChildListInsert(&CL, pid, childport, NULL);
					}
					else if(pid == 0){
						/*发送用户虚拟文件*/
						SendUserFS(sockfd, &client_addr, rootdirid, childport);
						exit(0);
					}
					break;
				
				/*上传文件请求*/
				case UPFILEREQ:
					if (UserListIfExistsbyIP(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port)) == FALSE) //用户未登录
					{
						sendbuf[0] = ERROR;
						sendbuf[1] = 14;
						Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
						break;
					}

					UserListSetHeart(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port), 0);

					/*获取文件的父目录id、文件大小、MD5值、文件名*/
					itoctemp = (char*)&pdirid;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[2+j];
					itoctemp = (char*)&filesize;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[10+j];
					for(j=0; j<16; j++)
						md5temp[j] = recvbuf[18+j];
					memset(filename, 0, FILENAMESIZE);

					if (retrecv - 34 >= FILENAMESIZE)
					{
						memcpy(filename, &recvbuf[34], FILENAMESIZE - 1);
					}
					else
					{
						memcpy(filename, &recvbuf[34], retrecv - 34);
						filename[retrecv - 34] = 0;
					}
					
					/*md5转化成16进制*/
					for(i=0; i<16; i++){
						j = md5temp[i]/16;
						if(j < 10)
							md5[2*i] = j +'0';
						else
							md5[2*i] = j+'a'-10;
						j = md5temp[i]%16;
						if(j < 10)
							md5[2*i+1] = j +'0';
						else
							md5[2*i+1] = j+'a'-10;
					}
					md5[32] = '\0';
					/*在数据库里搜索是否已存在相同的文件*/
					sprintf(sqlbuf, "select * from File where File_MD5 = '%s';", md5);
					if(mysql_query(mysql, sqlbuf)){
						printf("error select File table\n");
						return -1;
					}
					if ((result = mysql_store_result(mysql))==NULL) {
						printf("mysql_store_result failed\n" );
						return -1;
					}
					if(mysql_num_rows(result) == 1 || filesize <= 0){
						/*数据库已存在该文件，秒传或断点续传*/
						row=mysql_fetch_row(result);
						full = row[4][0] - '0';
						realfileid = atoi(row[0]);
						packid = atoi(row[5]);
						mysql_free_result(result);
							/*检查File_User表是否存在该文件记录*/
						sprintf(sqlbuf, "select File_User_ID from File_User where File_User_Name = '%s' and File_User_Parent = %d;", filename, pdirid);
						if(mysql_query(mysql, sqlbuf)){
							printf("error select File_User table\n");
							return -1;
						}
						if ((result = mysql_store_result(mysql))==NULL) {
							printf("mysql_store_result failed\n" );
							return -1;
						}
						if(mysql_num_rows(result) == 0) {
							
							/*更新File数据库，关联加一*/
							sprintf(sqlbuf, "update File set File_Index_Num = File_Index_Num + 1 where File_ID = %d;", realfileid);
							if(mysql_query(mysql, sqlbuf)){
								printf("error update File table\n");
								return -1;
							}
							mysql_free_result(result);
							/*File_User表没有这条记录，添加记录*/
							sprintf(sqlbuf, "insert into File_User values(NULL, '%s', %d, %d);", filename, pdirid, realfileid);
							if(mysql_query(mysql, sqlbuf)){
								printf("error insert File_User table\n");
								return -1;
							}
							sprintf(sqlbuf, "select File_User_ID from File_User where File_User_Name = '%s' and File_User_Parent = %d;", filename, pdirid);
							if(mysql_query(mysql, sqlbuf)){
								printf("error select File_User table\n");
								return -1;
							}
							if ((result = mysql_store_result(mysql))==NULL) {
								printf("mysql_store_result failed\n" );
								return -1;
							}
							row=mysql_fetch_row(result);
							userfileid = atoi(row[0]);
							mysql_free_result(result);
						}
						else {
							/*更新File_User表*/
							row=mysql_fetch_row(result);
							userfileid = atoi(row[0]);
							mysql_free_result(result);
							sprintf(sqlbuf, "update File_User set File_User_Index = %d where File_User_ID = %d;", realfileid, userfileid);
							if(mysql_query(mysql, sqlbuf)){
								printf("error update File_User table\n");
								return -1;
							}
						}
						
						if(full != NOTCOMPLETEFILE || filesize <= 0) {
							/*文件完整，秒传*/
							/*发秒传包*/
							sendbuf[0] = UPSEC;
							itoctemp = (char*)&userfileid;
							for(j=0; j<8; j++)
								sendbuf[j+2] = itoctemp[j];
							
							Sendto(sockfd, sendbuf, 10, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
							
						}
						else {
							/*断点续传*/
							int i;
							sendbuf[0] = UPPORTSEND;
							sendbuf[1] = 6;
							itoctemp = (char*)&userfileid;
							for(i=0; i<8; i++)
								sendbuf[2+i] = itoctemp[i];
							Sendto(sockfd, sendbuf, 10, 0, (struct sockaddr*)&client_addr, sizeof((client_addr)));
						}
					}
					else{         //此时不可能有用户在上传该文件
						mysql_free_result(result);
						/*在File数据库插入一条记录*/
						sprintf(sqlbuf, "insert into File values(NULL, %d, '%s', 1, 0, 0);", filesize, md5);
						if(mysql_query(mysql, sqlbuf)){
							printf("error insert File table\n");
							return -1;
						}
						sprintf(sqlbuf, "select * from File where File_MD5 = '%s';", md5);
						if(mysql_query(mysql, sqlbuf)){
							printf("error select File table\n");
							return -1;
						}
						if ((result = mysql_store_result(mysql))==NULL) {
							printf("mysql_store_result failed\n" );
							return -1;
						}
						row=mysql_fetch_row(result);
						realfileid = atoi(row[0]);
						mysql_free_result(result);
						packid = 0;
						
						/*检查File_User表是否存在该文件记录*/
						sprintf(sqlbuf, "select File_User_ID from File_User where File_User_Name = '%s' and File_User_Parent = %d;", filename, pdirid);
						if(mysql_query(mysql, sqlbuf)){
							printf("error select File_User table\n");
							return -1;
						}
						if ((result = mysql_store_result(mysql))==NULL) {
							printf("mysql_store_result failed\n" );
							return -1;
						}
						if(mysql_num_rows(result) == 0) {
							mysql_free_result(result);
							/*在File_User数据库插入一条记录*/
							sprintf(sqlbuf, "insert into File_User values(NULL, '%s', %d, %d);", filename, pdirid, realfileid);
							if(mysql_query(mysql, sqlbuf)){
								printf("error insert File_User table\n");
								return -1;
							}
							sprintf(sqlbuf, "select File_User_ID from File_User where File_User_Name = '%s' and File_User_Parent = %d;", filename, pdirid);
							if(mysql_query(mysql, sqlbuf)){
								printf("error select File_User table\n");
								return -1;
							}
							if ((result = mysql_store_result(mysql))==NULL) {
								printf("mysql_store_result failed\n" );
								return -1;
							}
							if(mysql_num_rows(result) != 0)
							{
								row=mysql_fetch_row(result);
								userfileid = atoi(row[0]);
								mysql_free_result(result);
							}
							else
							{
								sendbuf[0] = ERROR;
								sendbuf[1] = 14;
								Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
								break;
							}
						}
						else {
							/*更新File_User表*/
							row=mysql_fetch_row(result);
							userfileid = atoi(row[0]);
							mysql_free_result(result);
							sprintf(sqlbuf, "update File_User set File_User_Index = %d where File_User_ID = %d;", realfileid, userfileid);
							if(mysql_query(mysql, sqlbuf)){
								printf("error update File_User table\n");
								return -1;
							}
						}
						sendbuf[0] = UPPORTSEND;
						sendbuf[1] = 6;
						itoctemp = (char*)&userfileid;
						for(i=0; i<8; i++)
							sendbuf[2+i] = itoctemp[i];
						Sendto(sockfd, sendbuf, 10, 0, (struct sockaddr*)&client_addr, sizeof((client_addr)));
					}
					
					userid = UserListRetUserID(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port));
					now(timenow);
					timenow[19]='\0';
					sprintf(sqlbuf, "insert into Log values(null, %d, '%s', 5, %d);", userid, timenow, userfileid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error insert Log table\n");
						return -1;
					}
						
					break;
					
				/*上传开始请求*/
				case UPBEGIN:
					if (UserListIfExistsbyIP(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port)) == FALSE) //用户未登录
					{
						sendbuf[0] = ERROR;
						sendbuf[1] = 14;
						Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
						break;
					}

					UserListSetHeart(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port), 0);

					/*获取文件的父目录id、文件大小、MD5值、文件名*/
					for(j=0; j<16; j++)
						md5temp[j] = recvbuf[2+j];
					
					/*md5转化成16进制*/
					for(i=0; i<16; i++){
						j = md5temp[i]/16;
						if(j < 10)
							md5[2*i] = j +'0';
						else
							md5[2*i] = j+'a'-10;
						j = md5temp[i]%16;
						if(j < 10)
							md5[2*i+1] = j +'0';
						else
							md5[2*i+1] = j+'a'-10;
					}
					md5[32] = '\0';

					sendbuf[0] = UPPORTSEND;
					sendbuf[1] = 12;

					if (ChildListIfExists(CL, md5temp) == TRUE)   //多用户上传
					{
						//发送端口号
						ChildListGetPort(CL, md5temp, &childport);
						itoctemp = (char*)&childport;
						for(i=0; i<2; i++)
							sendbuf[2+i] = itoctemp[i];
						Sendto(sockfd, sendbuf, 4, 0, (struct sockaddr*)&client_addr, sizeof((client_addr)));
						break;
					}

					else {
						sprintf(sqlbuf, "select * from File where File_MD5 = '%s';", md5);
						if(mysql_query(mysql, sqlbuf)){
							printf("error select File table\n");
							return -1;
						}
						if ((result = mysql_store_result(mysql))==NULL) {
							printf("mysql_store_result failed\n" );
							return -1;
						}
						if(mysql_num_rows(result) == 1){
							/*数据库已存在该文件，秒传或断点续传*/
							row=mysql_fetch_row(result);							
							full = row[4][0] - '0';
							realfileid = atoi(row[0]);
							filesize = atoi(row[1]);
							packid = atoi(row[5]);
							mysql_free_result(result);

							if(full != NOTCOMPLETEFILE) {
								/*文件完整，秒传*/
								/*发秒传包*/
								sendbuf[0] = UPSEC;
								sendbuf[1] = 14;
							
								Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));	
								break;					
							}
							
							if((childport = PortAssign(&pctrl)) == -1){
								printf("port assign error\n");
								break;
							}
							
							pid = fork();
							if(pid < 0) {
								PortRelease(&pctrl, childport);
								perror("fork error");
								break;
							}
							if (pid > 0)
							{
								signal(SIGCHLD, my_fun);
								ChildListInsert(&CL, pid, childport, md5temp);
							}
							else if(pid == 0){ 
								UploadFile(sockfd, &client_addr, realfileid, childport, packid, filesize, md5);
								exit(0);
							}
						}
						else
						{
							sendbuf[0] = ERROR;
							sendbuf[1] = 14;
							Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
							break;
						}
					}
					
					break;
					
				/*上传目录*/
				case UPDIRREQ:
					if (UserListIfExistsbyIP(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port)) == FALSE) //用户未登录
					{
						sendbuf[0] = ERROR;
						sendbuf[1] = 14;
						Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
						break;
					}

					UserListSetHeart(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port), 0);

					/*获取目录名、父目录id*/
					itoctemp = (char*)&pdirid;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[2+j];
					memset(dirname, 0, DIRNAMESIZE);
					if (retrecv - 10 >= DIRNAMESIZE)
					{
						memcpy(dirname, &recvbuf[10], DIRNAMESIZE - 1);
					}
					else
					{
						memcpy(dirname, &recvbuf[10], retrecv - 10);
						dirname[retrecv - 10] = 0;
					}
					
					/*检查数据库中是否存在该目录记录*/
					sprintf(sqlbuf, "select Directory_ID from Directory where Directory_Name = '%s' and Directory_Parent = %d;", dirname, pdirid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error insert Directory table\n");
						return -1;
					}
					if ((result = mysql_store_result(mysql))==NULL) {
						printf("mysql_store_result failed\n" );
						return -1;
					}
					if(mysql_num_rows(result) == 0){
						mysql_free_result(result);
						/*数据库没有记录，写数据库*/
						sprintf(sqlbuf, "insert into Directory(Directory_Name, Directory_Parent) values('%s', %d);", dirname, pdirid);
						if(mysql_query(mysql, sqlbuf)){
							printf("error insert Directory table\n");
							return -1;
						}
						sprintf(sqlbuf, "select Directory_ID from Directory where Directory_Name = '%s' and Directory_Parent = %d;", dirname, pdirid);
						if(mysql_query(mysql, sqlbuf)){
							printf("error select Directory table\n");
							return -1;
						}
						if ((result = mysql_store_result(mysql))==NULL) {
							printf("mysql_store_result failed\n" );
							return -1;
						}
						row=mysql_fetch_row(result);
						dirid = atoi(row[0]);
						mysql_free_result(result);
					}
					else{
						row=mysql_fetch_row(result);
						dirid = atoi(row[0]);
						mysql_free_result(result);
					}
					
					/*发目录秒回包*/
					sendbuf[0] = UPDIRACK;
					itoctemp = (char*)&dirid;
					for(j=0; j<8; j++)
						sendbuf[2+j] = itoctemp[j];
					
					Sendto(sockfd, sendbuf, 10, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
					
					userid = UserListRetUserID(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port));
					now(timenow);
					timenow[19]='\0';
					sprintf(sqlbuf, "insert into Log values(null, %d, '%s', 6, %d);", userid, timenow, dirid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error insert Log table\n");
						return -1;
					}
					
					break;
					
				/*下载请求*/
				case DOWNFILEREQ:
					if (UserListIfExistsbyIP(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port)) == FALSE) //用户未登录
					{
						sendbuf[0] = ERROR;
						sendbuf[1] = 14;
						Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
						break;
					}

					UserListSetHeart(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port), 0);

					/*获取用户文件id*/
					itoctemp = (char*)&userfileid;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[2+j];
					
					/*查询数据库，获取文件真实id、大小和MD5值*/
					sprintf(sqlbuf, "select File_User_Index from File_User where File_User_ID = %d;", userfileid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error select File_User table\n");
						return -1;
					}
					if ((result = mysql_store_result(mysql))==NULL) {
						printf("mysql_store_result failed\n" );
						return -1;
					}
					/*没有查询到该文件回错误包*/
					if(mysql_num_rows(result) == 0){
						mysql_free_result(result);
						sendbuf[0] = ERROR;
						Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
						
						break;
					}
					row=mysql_fetch_row(result);
					sprintf(sqlbuf, "select * from File where File_ID = '%s';", row[0]);
					mysql_free_result(result);
					if(mysql_query(mysql, sqlbuf)){
						printf("error select File table\n");
						return -1;
					}
					if ((result = mysql_store_result(mysql))==NULL) {
						printf("mysql_store_result failed\n" );
						return -1;
					}
					
					row=mysql_fetch_row(result);
					sendbuf[0] = DOWNFILEINFOACK;
					filesize = atoi(row[1]);
					itoctemp = (char*)&filesize;
					for(j=0; j<8; j++)
						sendbuf[2+j] = itoctemp[j];
					strcpy(md5, row[2]);
					/*md5转化*/
					for(j=0; j<16; j++){
						if(md5[2*j] >= '0' && md5[2*j] <= '9')
							sendbuf[10+j] = 16*(md5[2*j]-'0');
						else
							sendbuf[10+j] = 16*(md5[2*j]-'a'+10);
						if(md5[2*j+1] >= '0' && md5[2*j+1] <= '9')
							sendbuf[10+j] = sendbuf[10+j]+md5[2*j+1]-'0';
						else
							sendbuf[10+j] = sendbuf[10+j]+md5[2*j+1]-'a'+10;
					}
					mysql_free_result(result);
					
					Sendto(sockfd, sendbuf, 26, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
					
					userid = UserListRetUserID(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port));
					now(timenow);
					timenow[19]='\0';
					sprintf(sqlbuf, "insert into Log values(null, %d, '%s', 7, %d);", userid, timenow, userfileid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error insert Log table\n");
						return -1;
					}
						
					break;
					
				/*开始下载文件块请求*/
				case DOWNFILE:
					if (UserListIfExistsbyIP(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port)) == FALSE) //用户未登录
					{
						sendbuf[0] = ERROR;
						sendbuf[1] = 14;
						Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
						break;
					}

					UserListSetHeart(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port), 0);

					/*获取用户文件id*/
					itoctemp = (char*)&userfileid;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[j+2];
					
					/*分配端口*/
					if((childport = PortAssign(&pctrl)) == -1){
						printf("port assign error\n");
						break;
					}
					pid = fork();
					if(pid < 0) {
						PortRelease(&pctrl, childport);
						perror("fork error");
						break;
					}
					if (pid > 0)
					{
						signal(SIGCHLD, my_fun);
						ChildListInsert(&CL, pid, childport, NULL);
					}
					else if(pid == 0){
						SendUserFile(sockfd, &client_addr, userfileid, childport);
						exit(0);
					}
					break;
					
				/*秒下*/
				case DOWNSEC:
					UserListSetHeart(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port), 0);
					break;
					
				/*case HEART_REQ:
					sendbuf[0] = HEART_ACK;
					sendbuf[1] = 14;
					Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
					break;*/
					
				/*改名*/
				case CHANGEFILENAMEREQ:
					UserListSetHeart(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port), 0);
					itoctemp = (char*)&userfileid;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[j+2];
					strcpy(nfilename, &recvbuf[10]);
					
					sprintf(sqlbuf, "update File_User set File_User_Name = '%s' where File_User_ID = %d;", nfilename, userfileid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error update File_User table\n");
						return -1;
					}
					
					sendbuf[0] = CHANGEFILENAMEACK;
					Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
					
					userid = UserListRetUserID(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port));
					now(timenow);
					timenow[19]='\0';
					sprintf(sqlbuf, "insert into Log values(null, %d, '%s', 8, %d);", userid, timenow, userfileid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error insert Log table\n");
						return -1;
					}
					
					break;
					
				/*复制文件*/
				case COPYFILEREQ:
					UserListSetHeart(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port), 0);
					itoctemp = (char*)&userfileid;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[j+2];
					itoctemp = (char*)&ndirid;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[j+10];
					
					sprintf(sqlbuf, "select * from File_User where File_User_ID = %d;", userfileid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error select File_User table\n");
						return -1;
					}
					if ((result = mysql_store_result(mysql))==NULL) {
						printf("mysql_store_result failed\n" );
						return -1;
					}
					row = mysql_fetch_row(result);
					mysql_free_result(result);
					
					sprintf(sqlbuf, "select * from File_User where File_User_Parent = %d and File_User_Name = '%s';", ndirid, row[1]);
					if(mysql_query(mysql, sqlbuf)){
						printf("error select File_User table\n");
						return -1;
					}
					if ((result = mysql_store_result(mysql))==NULL) {
						printf("mysql_store_result failed\n" );
						return -1;
					}
					if(mysql_num_rows(result) == 0){
						mysql_free_result(result);
						sprintf(sqlbuf, "insert into File_User values(null, '%s', %d, '%s');", row[1], ndirid, row[3]);
						if(mysql_query(mysql, sqlbuf)){
							printf("error insert File_User table\n");
							return -1;
						}
						sprintf(sqlbuf, "update File set File_Index_Num = File_Index_Num + 1  where File_ID = '%s';", row[3]);
						if(mysql_query(mysql, sqlbuf)){
							printf("error update File table\n");
							return -1;
						}
					}
					sendbuf[0] = COPYFILEACK;
					Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
					
					userid = UserListRetUserID(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port));
					now(timenow);
					timenow[19]='\0';
					sprintf(sqlbuf, "insert into Log values(null, %d, '%s', 9, %d);", userid, timenow, userfileid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error insert Log table\n");
						return -1;
					}
					
					break;
					
				/*复制文件夹*/
				case COPYDIRREQ:
					UserListSetHeart(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port), 0);
					itoctemp = (char*)&dirid;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[j+2];
					itoctemp = (char*)&ndirid;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[j+10];
					
					sprintf(sqlbuf, "select * from Directory where Directory_ID = %d;", dirid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error select Directory table\n");
						return -1;
					}
					if ((result = mysql_store_result(mysql))==NULL) {
						printf("mysql_store_result failed\n" );
						return -1;
					}
					row = mysql_fetch_row(result);
					mysql_free_result(result);
					
					sprintf(sqlbuf, "select * from Directory where Directory_Parent = %d and Directory_Name = '%s';", ndirid, row[1]);
					if(mysql_query(mysql, sqlbuf)){
						printf("error select Directory table\n");
						return -1;
					}
					if ((result = mysql_store_result(mysql))==NULL) {
						printf("mysql_store_result failed\n" );
						return -1;
					}
					if(mysql_num_rows(result) == 0){
						mysql_free_result(result);
						CopyDir(mysql, dirid, ndirid);
					}
					
					sendbuf[0] = COPYDIRACK;
					Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
					
					userid = UserListRetUserID(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port));
					now(timenow);
					timenow[19]='\0';
					sprintf(sqlbuf, "insert into Log values(null, %d, '%s', 10, %d);", userid, timenow, dirid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error insert Log table\n");
						return -1;
					}
					
					break;
					
				/*移动文件*/
				case MOVEFILEREQ:
					UserListSetHeart(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port), 0);
					itoctemp = (char*)&userfileid;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[j+2];
					itoctemp = (char*)&ndirid;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[j+10];
					
					sprintf(sqlbuf, "update File_User set File_User_Parent = %d where File_User_ID = %d;", ndirid, userfileid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error update File_User table\n");
						return -1;
					}
					
					sendbuf[0] = MOVEFILEACK;
					Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
					
					userid = UserListRetUserID(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port));
					now(timenow);
					timenow[19]='\0';
					sprintf(sqlbuf, "insert into Log values(null, %d, '%s', 11, %d);", userid, timenow, userfileid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error insert Log table\n");
						return -1;
					}
					break;
					
				/*移动文件夹*/
				case MOVEDIRREQ:
					UserListSetHeart(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port), 0);
					itoctemp = (char*)&dirid;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[j+2];
					itoctemp = (char*)&ndirid;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[j+10];
					
					sprintf(sqlbuf, "update Directory set Directory_Parent = %d where Directory_ID = %d;", ndirid, dirid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error update Directory table\n");
						return -1;
					}
					
					sendbuf[0] = MOVEDIRACK;
					Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
					
					userid = UserListRetUserID(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port));
					now(timenow);
					timenow[19]='\0';
					sprintf(sqlbuf, "insert into Log values(null, %d, '%s', 12, %d);", userid, timenow, dirid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error insert Log table\n");
						return -1;
					}
					break;
					
				/*删除文件*/
				case DELETEFILEREQ:
					UserListSetHeart(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port), 0);
					itoctemp = (char*)&userfileid;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[j+2];
					
					sprintf(sqlbuf, "select * from File_User where File_User_ID = %d;", userfileid);
					printf("%s\n", sqlbuf);
					if(mysql_query(mysql, sqlbuf)){
						printf("error update File_User table\n");
						return -1;
					}
					if ((result = mysql_store_result(mysql))==NULL) {
						printf("mysql_store_result failed\n" );
						return -1;
					}
					
					if(mysql_num_rows(result) == 1){
						row = mysql_fetch_row(result);
						mysql_free_result(result);
						sprintf(sqlbuf, "delete from  File_User where File_User_ID = %d;", userfileid);
						if(mysql_query(mysql, sqlbuf)){
							printf("error delete File_User table\n");
							return -1;
						}
						sprintf(sqlbuf, "update File set File_Index_Num = File_Index_Num - 1  where File_ID = '%s';", row[3]);
						if(mysql_query(mysql, sqlbuf)){
							printf("error update File table\n");
							return -1;
						}
					}
					
					sendbuf[0] = DELETEFILEACK;
					Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
					
					userid = UserListRetUserID(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port));
					now(timenow);
					timenow[19]='\0';
					sprintf(sqlbuf, "insert into Log values(null, %d, '%s', 13, %d);", userid, timenow, userfileid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error insert Log table\n");
						return -1;
					}
					
					break;
					
				/*删除文件夹*/
				case DELETEDIRREQ:
					UserListSetHeart(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port), 0);
					itoctemp = (char*)&dirid;
					for(j=0; j<8; j++)
						itoctemp[j] = recvbuf[j+2];
					
					sprintf(sqlbuf, "select * from Directory where Directory_ID = %d;", dirid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error update File_User table\n");
						return -1;
					}
					if ((result = mysql_store_result(mysql))==NULL) {
						printf("mysql_store_result failed\n" );
						return -1;
					}
					
					if(mysql_num_rows(result) == 1){
						row = mysql_fetch_row(result);
						mysql_free_result(result);
						DeleteDir(mysql, dirid);
					}
					
					sendbuf[0] = DELETEDIRACK;
					Sendto(sockfd, sendbuf, 2, 0, (struct sockaddr*)&client_addr, sizeof(client_addr));
					
					userid = UserListRetUserID(UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port));
					now(timenow);
					timenow[19]='\0';
					sprintf(sqlbuf, "insert into Log values(null, %d, '%s', 14, %d);", userid, timenow, dirid);
					if(mysql_query(mysql, sqlbuf)){
						printf("error insert Log table\n");
						return -1;
					}
					
					break; 
				default:
					UserListSetHeart(&UL, ntohl(client_addr.sin_addr.s_addr), ntohs(client_addr.sin_port), 0);
					printf("HeartAck From %d\n", ntohs(client_addr.sin_port));
					break;
			}//switch
		}//FD_ISSET
	}//while

	UserListDestory(&UL);  //用户链表销毁
	ChildListDestory(&CL); //子进程链表销毁
	mysql_close(mysql);  
	close(sockfd);
	return 0;
}

int now(char* datetime)
{
	time_t timep;
	struct tm *p;
	
	time(&timep);
	p=localtime(&timep);
	sprintf(datetime, "%4d-%02d-%02d %02d:%02d:%02d", (1900+p->tm_year), (1+p->tm_mon), p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
	return 0;
}


int CopyDir(MYSQL* mysql, long int dirid, long int ndirid)
{
	char sqlbuf[SQLBUFSIZE];
	long int pdirid;
	long int ddirid;
	
	MYSQL_RES *result;
	MYSQL_ROW row;
	
	sprintf(sqlbuf, "select * from Directory where Directory_ID = %d;", dirid);
	if(mysql_query(mysql, sqlbuf)){
		printf("error select Directory table\n");
		return -1;
	}
	if ((result = mysql_store_result(mysql))==NULL) {
		printf("mysql_store_result failed\n" );
		return -1;
	}
	row = mysql_fetch_row(result);
	mysql_free_result(result);
	
	sprintf(sqlbuf, "insert into Directory values(null, '%s', %d);", row[1], ndirid);
	if(mysql_query(mysql, sqlbuf)){
		printf("error insert Directory table\n");
		return -1;
	}
	sprintf(sqlbuf, "select Directory_ID from Directory where Directory_Name = '%s' and Directory_Parent = %d;", row[1], ndirid);
	if(mysql_query(mysql, sqlbuf)){
		printf("error select Directory table\n");
		return -1;
	}
	if ((result = mysql_store_result(mysql))==NULL) {
		printf("mysql_store_result failed\n" );
		return -1;
	}
	row = mysql_fetch_row(result);
	mysql_free_result(result);
	
	pdirid = atoi(row[0]);
	
	sprintf(sqlbuf, "select * from File_User where File_User_Parent = %d", dirid);
	if(mysql_query(mysql, sqlbuf)){
			printf("error sql query");
			return -1;
	}
	if ((result = mysql_store_result(mysql))==NULL) {
    	printf("mysql_store_result failed\n" );
    	return -1;
    }
	if(mysql_num_rows(result) > 0) 
		while((row=mysql_fetch_row(result))!=NULL) {
			sprintf(sqlbuf, "insert into File_User values(null, '%s', %d, '%s');", row[1], pdirid, row[3]);
			if(mysql_query(mysql, sqlbuf)){
				printf("error insert File_User table\n");
				return -1;
			}
			sprintf(sqlbuf, "update File set File_Index_Num = File_Index_Num + 1 where File_ID = '%s';", row[3]);
			if(mysql_query(mysql, sqlbuf)){
				printf("error update File table\n");
				return -1;
			}
		}
	mysql_free_result(result);
	
	sprintf(sqlbuf, "select * from Directory where Directory_Parent = %d", dirid);
	if(mysql_query(mysql, sqlbuf)){
			printf("error sql query");
			return -1;
	}
	if ((result = mysql_store_result(mysql))==NULL) {
    	printf("mysql_store_result failed\n" );
    	return -1;
    }
	if(mysql_num_rows(result) > 0) 
		while((row=mysql_fetch_row(result))!=NULL) {
			ddirid = atoi(row[0]);
			CopyDir(mysql, ddirid, pdirid);
		}
	mysql_free_result(result);
}

int DeleteDir(MYSQL* mysql, long int dirid)
{
	char sqlbuf[SQLBUFSIZE];
	long int ddirid;
	
	MYSQL_RES *result;
	MYSQL_ROW row;
	
	sprintf(sqlbuf, "select * from File_User where File_User_Parent = %d", dirid);
	if(mysql_query(mysql, sqlbuf)){
			printf("error select File_User table\n");
			return -1;
	}
	if ((result = mysql_store_result(mysql))==NULL) {
    	printf("mysql_store_result failed\n" );
    	return -1;
    }
	if(mysql_num_rows(result) > 0) 
		while((row=mysql_fetch_row(result))!=NULL) {
			sprintf(sqlbuf, "delete from  File_User where File_User_ID = '%s';", row[0]);
			if(mysql_query(mysql, sqlbuf)){
				printf("error delete File_User table\n");
				return -1;
			}
			sprintf(sqlbuf, "update File set File_Index_Num = File_Index_Num - 1  where File_ID = '%s';", row[3]);
			if(mysql_query(mysql, sqlbuf)){
				printf("error update File table\n");
				return -1;
			}
		}
	mysql_free_result(result);
	
	sprintf(sqlbuf, "select * from Directory where Directory_Parent = %d", dirid);
	if(mysql_query(mysql, sqlbuf)){
			printf("error sql query");
			return -1;
	}
	if ((result = mysql_store_result(mysql))==NULL) {
    	printf("mysql_store_result failed\n" );
    	return -1;
    }
	if(mysql_num_rows(result) > 0) 
		while((row=mysql_fetch_row(result))!=NULL) {
			ddirid = atoi(row[0]);
			DeleteDir(mysql, ddirid);
		}
	mysql_free_result(result);
	
	sprintf(sqlbuf, "delete from  Directory where Directory_ID = %d;", dirid);
	if(mysql_query(mysql, sqlbuf)){
		printf("error delete File_User table\n");
		return -1;
	}
}

/*用户虚拟文件系统函数*/

int InitFileInfoList(FileInfoList* L, long int rootdirid)
{
	(*L) = (FileInfoNode*)malloc(sizeof(FileInfoNode));
	if (!(*L))
		exit(-1);
	(*L)->pdirid = 0;
	(*L)->fileid = rootdirid;
	(*L)->type = DIRTYPE;
	(*L)->filesize = 0;
	strcpy((*L)->filename, "root");
	(*L)->next = NULL;
	
	return 0;
}

int DestoryFileInfoList(FileInfoList* L)
{
	FileInfoList p = (*L), q;
	while (p)
	{
		q = p->next;
		free(p);
		p = q;
	}
	
	return 0;
}

int InsertFileInfoList(FileInfoList* L, long int pdirid, long int fileid, char type, long int filesize, char* filename)
{
	FileInfoNode* s = (FileInfoNode*)malloc(sizeof(FileInfoNode));
	if(!s)
		exit(-1);
	s->pdirid = pdirid;
	s->fileid = fileid;
	s->type = type;
	s->filesize = filesize;
	strcpy(s->filename, filename);
	s->next = (*L)->next;
	(*L)->next = s;

	return 0;
}

int SortFileInfoList(FileInfoList* L)
{
	FileInfoList p = (*L)->next, pn, q, pf;
	int i, j, k;
	
	pf = (*L);
	while(p) 
	{
		pn = p->next;
		
		j = 0;
		for(q=(*L); q!=p; q=q->next) {
			if(q->pdirid > p->pdirid)
				break;
			else if(q->pdirid == p->pdirid) {
				if(q->fileid > p->fileid)
					break;
			}	
			j++;
		}
		if(q != p){
			q=(*L);
			for(k=0; k<j-1; k++)
				q = q->next;
			pf->next = p->next;
			p->next = q->next;
			q->next = p;
		}
		else{
			pf = p;
		}
		p = pn;
	}
}

/*在数据库中找到所有的属于某一用户目录和文件信息*/
int SearchAllFileInfo(FileInfoList* L, MYSQL* mysql, long int dirid)
{
	char sqlbuf[SQLBUFSIZE];
	MYSQL_RES *result1;  
	MYSQL_RES *result2;	
    MYSQL_ROW  row1;
	MYSQL_ROW  row2;
	
	long int pdirid;    /*父目录id*/
	long int fileid;    /*文件id*/
	char type;          /*文件种类 FILE or DIR*/
	long int filesize;  /*文件大小*/
	char filename[FILENAMESIZE]; /*文件名*/
	char full;
	
	int i;
	
	sprintf(sqlbuf, "select * from File_User where File_User_Parent = %d", dirid);
	if(mysql_query(mysql, sqlbuf)){
			printf("error sql query");
			return -1;
	}
	if ((result1 = mysql_store_result(mysql))==NULL) {
    	printf("mysql_store_result failed\n" );
    	return -1;
    }
	if(mysql_num_rows(result1) > 0) 
		while((row1=mysql_fetch_row(result1))!=NULL) {
			fileid = atoi(row1[0]);
			strcpy(filename, row1[1]);
			pdirid = atoi(row1[2]);
			type = FILETYPE;
			sprintf(sqlbuf, "select * from File where File_ID = '%s'", row1[3]);
			if(mysql_query(mysql, sqlbuf)){
				printf("error sql query");
				return -1;
			}
			if ((result2 = mysql_store_result(mysql))==NULL) {
				printf("mysql_store_result failed\n" );
				return -1;
			}
			row2=mysql_fetch_row(result2);
			filesize = atoi(row2[1]);
			full = row2[4][0] - '0';
			mysql_free_result(result2); 
			if(full != NOTCOMPLETEFILE) {
				if(InsertFileInfoList(L, pdirid, fileid, type, filesize, filename) < 0) {
					printf("insert file info list\n");
					exit(-1);
				}
			}

		}
	mysql_free_result(result1);
	
	sprintf(sqlbuf, "select * from Directory where Directory_Parent = %d", dirid);
	if(mysql_query(mysql, sqlbuf)){
			printf("error sql query");
			return -1;
	}
	if ((result1 = mysql_store_result(mysql))==NULL) {
    	printf("mysql_store_result failed\n" );
    	return -1;
    }
	if(mysql_num_rows(result1) > 0) 
		while((row1=mysql_fetch_row(result1))!=NULL) {
			fileid = atoi(row1[0]);
			strcpy(filename, row1[1]);
			pdirid = atoi(row1[2]);
			type = DIRTYPE;
			filesize = 0;
			if(InsertFileInfoList(L, pdirid, fileid, type, filesize, filename) < 0) {
				printf("insert file info list\n");
				exit(-1);
			}
			SearchAllFileInfo(L, mysql, fileid);
		}
	mysql_free_result(result1);
	return 0;
}

/*发送用户虚拟文件系统*/
int SendUserFS(int sockfd, struct sockaddr_in* client_addr, long int rootdirid, unsigned short port)
{
	struct sockaddr_in zhost_addr, zclient_addr;
	int zsockfd;
	
	char recvbuf[RECVBUFSIZE];
	char sendbuf[SENDBUFSIZE];
	int retrecv;
	int addrlen;
	
	char* itoctemp;
	int i, j, k;
	int flag;
	
	int flags;
	fd_set rfds;
	int retselect;
	struct timeval tv;
	tv.tv_sec = SECONDS;
	tv.tv_usec = 0;
	
	char sqlbuf[SQLBUFSIZE];//用于登录时查询数据库
	
	MYSQL     *mysql;
	MYSQL_RES *result;
	MYSQL_ROW row;
	
	FileInfoList L;
	
	if ((mysql = mysql_init(NULL))==NULL) {
		printf("mysql_init failed\n");
		exit(-1);
	}

	while (mysql_real_connect(mysql,"localhost","root", "root123", "OnlineDisk",0, NULL, 0)==NULL) {
		printf("mysql_real_connect failed(%d)\n", mysql_error(mysql));
		sleep(1);
	}

	mysql_set_character_set(mysql, "gbk"); 
	
	memset(&zhost_addr, 0, sizeof(zhost_addr));
	zhost_addr.sin_family = AF_INET;
	zhost_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	zhost_addr.sin_port = htons(port);
	
	if((zsockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("create socket error");
		exit(-1);
	}
	
	flags = fcntl(zsockfd, F_GETFL, 0);
	fcntl(zsockfd, F_SETFL, flags | O_NONBLOCK);

	//设置udp缓冲区大小
	//setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));  
	
	if (setsockopt(zsockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1) 
	{ 
		perror("setsockopt"); 
		exit(1); 
	}
	
	if(bind(zsockfd, (struct sockaddr*)&zhost_addr, sizeof(zhost_addr)) == -1){
		perror("bind error");
		exit(-1);
	}
	
	sendbuf[0] = USERFSPORTSEND;
	itoctemp = (char*)&port;
	for(j=0; j<2; j++)
		sendbuf[2+j] = itoctemp[j];
	
	for(i=0; i<TIMES; i++) {
		/*发送端口包*/
		Sendto(sockfd, sendbuf, 4, 0, (struct sockaddr*)client_addr, sizeof((*client_addr)));

		/*等待接收确认包，超时重传*/
		FD_ZERO(&rfds);
		FD_SET(zsockfd, &rfds);
		tv.tv_sec = SECONDS;
		tv.tv_usec = 0;
		retselect = Select(zsockfd+1, &rfds, NULL, NULL, &tv);
		if(retselect == -1){
			perror("select error");
			exit(-1);
		}
		else if(retselect == 0){
			continue;
		}
			
		if(FD_ISSET(zsockfd, &rfds)){
			addrlen = sizeof(zclient_addr);
			retrecv = Recvfrom(zsockfd, recvbuf, RECVBUFSIZE, 0, (struct sockaddr*)&zclient_addr, &addrlen);
			if(recvbuf[0] == USERFSACK)
				break;
		}
	}
	if(i == TIMES){
		printf("send user file system port fail\n");
		exit(-1);
	}
	
	/*发送用户虚拟文件系统包*/
	InitFileInfoList(&L, rootdirid);
	SearchAllFileInfo(&L, mysql, rootdirid);
	SortFileInfoList(&L);
	FileInfoList p = L, q;
	while (p)
	{
		q = p->next;
		/*写包*/
		sendbuf[0] = USERFSPACK;
		itoctemp = (char*)&(p->pdirid);
		for(j=0; j<8; j++)
			sendbuf[2+j] = itoctemp[j];
		itoctemp = (char*)&(p->fileid);
		for(j=0; j<8; j++)
			sendbuf[10+j] = itoctemp[j];
		sendbuf[18] = p->type;
		itoctemp = (char*)&(p->filesize);
		for(j=0; j<8; j++)
			sendbuf[19+j] = itoctemp[j];
		strcpy(&sendbuf[27], p->filename);
		
		/*发包*/
		for(i=0; i<TIMES; i++) {
			Sendto(zsockfd, sendbuf, 27+FILENAMESIZE, 0, (struct sockaddr*)&zclient_addr, sizeof(zclient_addr));
						
			/*等待接收确认包，超时重传*/
			FD_ZERO(&rfds);
			FD_SET(zsockfd, &rfds);
			tv.tv_sec = SECONDS;
			tv.tv_usec = 0;
			retselect = Select(zsockfd+1, &rfds, NULL, NULL, &tv);
			if(retselect == -1){
				perror("select error");
				exit(-1);
			}
			else if(retselect == 0){
				continue;
			}
				
			if(FD_ISSET(zsockfd, &rfds)){
				addrlen = sizeof(zclient_addr);
				retrecv = Recvfrom(zsockfd, recvbuf, RECVBUFSIZE, 0, (struct sockaddr*)&zclient_addr, &addrlen);
				if(recvbuf[0] == USERFSACK)
					break;
			}
		}
		if(i == TIMES){
			printf("send user file system pack fail\n");
			break;
		}
		
		p = q;
	}
	
	/*发结束包*/
	sendbuf[0] = USERFSEND;

	Sendto(zsockfd, sendbuf, 2, 0, (struct sockaddr*)&zclient_addr, sizeof(zclient_addr));
			
	DestoryFileInfoList(&L);
	mysql_close(mysql);
	close(zsockfd);
	return 0;
}

/*给用户发送文件*/
int SendUserFile(int sockfd, struct sockaddr_in* client_addr, long int userfileid, unsigned short port)
{
	struct sockaddr_in zhost_addr, zclient_addr;
	int zsockfd;
	
	int fd;
	int nread;
	
	int packid;
	int npackid;
	int packall;
	long int realfileid;
	long int filesize;
	unsigned char md5[20];
	char filename[33];
	char mtontemp[3];
	
	char recvbuf[RECVBUFSIZE];
	char sendbuf[SENDBUFSIZE];
	int retrecv;
	int addrlen;
	
	char* itoctemp;
	int i, j, k;
	int flag;
	
	int flags;
	fd_set rfds;
	int retselect;
	struct timeval tv;
	tv.tv_sec = SECONDS;
	tv.tv_usec = 0;
	
	char sqlbuf[SQLBUFSIZE];//用于登录时查询数据库
	
	MYSQL     *mysql;
	MYSQL_RES *result;
	MYSQL_ROW row;
	
	if ((mysql = mysql_init(NULL))==NULL) {
		printf("mysql_init failed\n");
		exit(-1);
	}

	while (mysql_real_connect(mysql,"localhost","root", "root123", "OnlineDisk",0, NULL, 0)==NULL) {
		printf("mysql_real_connect failed(%d)\n", mysql_error(mysql));
		sleep(1);
	}

	mysql_set_character_set(mysql, "gbk"); 
	
	memset(&zhost_addr, 0, sizeof(zhost_addr));
	zhost_addr.sin_family = AF_INET;
	zhost_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	zhost_addr.sin_port = htons(port);
	
	if((zsockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("create socket error");
		exit(-1);
	}
	
	flags = fcntl(zsockfd, F_GETFL, 0);
	fcntl(zsockfd, F_SETFL, flags | O_NONBLOCK);

	//设置udp缓冲区大小
	//setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));  
	
	if (setsockopt(zsockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1) 
	{ 
		perror("setsockopt"); 
		exit(1); 
	}
	
	if(bind(zsockfd, (struct sockaddr*)&zhost_addr, sizeof(zhost_addr)) == -1){
		perror("bind error");
		exit(-1);
	}
	
	sendbuf[0] = DOWNPORTSEND;
	itoctemp = (char*)&port;
	for(j=0; j<2; j++)
		sendbuf[2+j] = itoctemp[j];
	
	for(i=0; i<TIMES; i++) {
		/*发送端口包*/
		Sendto(sockfd, sendbuf, 4, 0, (struct sockaddr*)client_addr, sizeof((*client_addr)));
		
		/*等待接收确认包，超时重传*/
		FD_ZERO(&rfds);
		FD_SET(zsockfd, &rfds);
		tv.tv_sec = SECONDS;
		tv.tv_usec = 0;
		retselect = Select(zsockfd+1, &rfds, NULL, NULL, &tv);
		if(retselect == -1){
			perror("select error");
			exit(-1);
		}
		else if(retselect == 0){
			continue;
		}
			
		if(FD_ISSET(zsockfd, &rfds)){
			addrlen = sizeof(zclient_addr);
			retrecv = Recvfrom(zsockfd, recvbuf, RECVBUFSIZE, 0, (struct sockaddr*)&zclient_addr, &addrlen);
			if(recvbuf[0] == DOWNFILEBLOCKREQ) {
				itoctemp = (char*)&packid;
				for(j=0; j<4; j++)
					itoctemp[j] = recvbuf[2+j];
				break;
			}
		}
	}
	if(i == TIMES){
		printf("send user file system port fail\n");
		exit(-1);
	}
	
	/*查询数据库获取文件真实id、大小、MD5*/
	sprintf(sqlbuf, "select File_User_Index from File_User where File_User_ID = %d;", userfileid);
	if(mysql_query(mysql, sqlbuf)){
		printf("error select File_User table\n");
		return -1;
	}
	if ((result = mysql_store_result(mysql))==NULL) {
		printf("mysql_store_result failed\n" );
		return -1;
	}
	row=mysql_fetch_row(result);
	sprintf(sqlbuf, "select * from File where File_ID = '%s';", row[0]);
	mysql_free_result(result);
	if(mysql_query(mysql, sqlbuf)){
		printf("error select File table\n");
		return -1;
	}
	if ((result = mysql_store_result(mysql))==NULL) {
		printf("mysql_store_result failed\n" );
		return -1;
	}			
	row=mysql_fetch_row(result);
	realfileid = atoi(row[0]);
	filesize = atoi(row[1]);
	strcpy(filename, NETDISKFILEPATH);
	strcat(filename, row[2]);
	mysql_free_result(result);
	
	/*md5值转化为文件名*/
	/*filename[0] = '\0';
	for(i=0; i<16; i++){
		sprintf(mtontemp, "%02x", md5[i]);
		strcat(filename, mtontemp);
	}*/
	
	if((fd = open(filename, O_RDONLY)) == -1) {
		perror("open file fail");
		exit(-1);
	}
	if(lseek(fd, FILEBLOCKSIZE*packid, SEEK_SET) == -1) {
		perror("cannot seek");
		exit(-1);
	}

	packall = filesize/FILEBLOCKSIZE + ((filesize%FILEBLOCKSIZE==0) ? 0 : 1);
	
	/*发送用户文件块包*/
	while(1){
		/*写包*/
		sendbuf[0] = DOWNFILEBLOCK;
		itoctemp = (char*)&packid;
		for(j=0; j<4; j++)
			sendbuf[2+j] = itoctemp[j];
		
		nread = read(fd, &sendbuf[6], FILEBLOCKSIZE);

		sendbuf[1] = (16 - (nread + 6) % 16) % 16;
		
		/*发包*/
		for(i=0; i<TIMES; i++) {
			Sendto(zsockfd, sendbuf, 6+nread, 0, (struct sockaddr*)&zclient_addr, sizeof(zclient_addr));
			
			/*等待接收确认包，超时重传*/
			FD_ZERO(&rfds);
			FD_SET(zsockfd, &rfds);
			tv.tv_sec = SECONDS;
			tv.tv_usec = 0;
			retselect = Select(zsockfd+1, &rfds, NULL, NULL, &tv);
			if(retselect == -1){
				perror("select error");
				exit(-1);
			}
			else if(retselect == 0){
				continue;
			}
				
			if(FD_ISSET(zsockfd, &rfds)){
				addrlen = sizeof(zclient_addr);
				retrecv = Recvfrom(zsockfd, recvbuf, RECVBUFSIZE, 0, (struct sockaddr*)&zclient_addr, &addrlen);
				if(recvbuf[0] == DOWNFILEBLOCKREQ) {
					itoctemp = (char*)&npackid;
					for(j=0; j<4; j++)
						itoctemp[j] = recvbuf[2+j];
					packid = npackid;
					if(lseek(fd, FILEBLOCKSIZE*npackid, SEEK_SET) == -1) {
						perror("cannot seek zz");
						exit(-1);
					}
					break;
				}
				else if (recvbuf[0] == DOWNFILEEND)
				{
					mysql_close(mysql);
					close(zsockfd);
					close(fd);
					return 0;
				}
			}
		}
		if(i == TIMES){
			printf("send user file system pack fail\n");
			break;
		}
	}
	
	/*发结束包*/
	/*sendbuf[0] = DOWNFILEEND;
	itoctemp = (char*)&userfileid;
	for(j=0; j<8; j++)
		sendbuf[j+2] = itoctemp[j];
	
	
	Sendto(zsockfd, sendbuf, 10, 0, (struct sockaddr*)&zclient_addr, sizeof(zclient_addr));*/
			
	mysql_close(mysql);
	close(zsockfd);
	close(fd);
	return 0;
}

int UploadFile(int sockfd, struct sockaddr_in* client_addr, long int realfileid, unsigned short port, int beginpackid, long int filesize, char* md5)
{
	struct sockaddr_in zhost_addr, zclient_addr;
	int zsockfd;
	
	int fd;
	FILE* fp;
	int nwrite;
	
	int packid;
	int npackid;
	int packrecvid;
	int maxpackid;
	int end_sig;
	char mtontemp[3];
	char filename[50];
	
	unsigned char finalmd5[16];
	
	long int xfileid;
	long int xfilesize;
	int xusernum;
	
	char recvbuff[RECVBUFSIZE];
	char sendbuff[SENDBUFSIZE];
	int retrecv;
	int addrlen;
	
	char* itoctemp;
	int i, j, k;
	int flag;
	
	int flags;
	fd_set rfds;
	int retselect;
	struct timeval tv;
	tv.tv_sec = SECONDS;
	tv.tv_usec = 0;

	UserChildList UCL;
	
	struct flock lock; 
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0; 
	lock.l_len = 0;
	
	char sqlbuf[SQLBUFSIZE];//用于登录时查询数据库
	
	MYSQL     *mysql;
	MYSQL_RES *result;
	MYSQL_ROW row;

	if ((mysql = mysql_init(NULL))==NULL) {
		printf("mysql_init failed\n");
		exit(-1);
	}

	while (mysql_real_connect(mysql,"localhost","root", "root123", "OnlineDisk",0, NULL, 0)==NULL) {
		printf("mysql_real_connect failed(%d)\n", mysql_error(mysql));
		sleep(1);
	}

	mysql_set_character_set(mysql, "gbk"); 
	
	memset(&zhost_addr, 0, sizeof(zhost_addr));
	zhost_addr.sin_family = AF_INET;
	zhost_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	zhost_addr.sin_port = htons(port);
	
	if((zsockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("create socket error");
		exit(-1);
	}
	
	flags = fcntl(zsockfd, F_GETFL, 0);
	fcntl(zsockfd, F_SETFL, flags | O_NONBLOCK);

	//设置udp缓冲区大小
	//setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));  
	
	if (setsockopt(zsockfd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) == -1) 
	{ 
		perror("setsockopt"); 
		exit(1); 
	}
	
	if(bind(zsockfd, (struct sockaddr*)&zhost_addr, sizeof(zhost_addr)) == -1){
		perror("bind error");
		exit(-1);
	}

	UserChildListInit(&UCL);

	packid = beginpackid;
	end_sig = 0;

	
	/*md5值转化为文件名*/
	strcpy(filename, NETDISKFILEPATH);
	strcat(filename, md5);

	if((fd = open(filename, O_WRONLY|O_CREAT)) == -1) {
		perror("open file fail");
		exit(-1);
	}
	if(fchmod(fd, S_IRWXU|S_IRWXG|S_IRWXO) == -1) {
		perror("chmod error");
		exit(-1);
	}
	/*发送请求文件块包*/

	//发送端口号
	sendbuff[0] = UPPORTSEND;
	sendbuff[1] = 12;
	itoctemp = (char*)&port;
	for(i=0; i<2; i++)
		sendbuff[2+i] = itoctemp[i];
	Sendto(zsockfd, sendbuff, 4, 0, (struct sockaddr*)client_addr, sizeof((*client_addr)));

	maxpackid = filesize/FILEBLOCKSIZE + ((filesize%FILEBLOCKSIZE==0) ? 0 : 1);
	while(1){
		FD_ZERO(&rfds);
		FD_SET(zsockfd, &rfds);
		tv.tv_sec = SECONDS;
		tv.tv_usec = 0;
		retselect = Select(zsockfd+1, &rfds, NULL, NULL, &tv);
		if(retselect == -1){
			perror("select error");
			exit(-1);
		}
		else if(retselect == 0){
			//所有用户断开
			if (packid > beginpackid)
			{
				sprintf(sqlbuf, "update File set File_Complete = 0, File_Pack_Num = %d where File_ID = %d;", packid, realfileid);
				if(mysql_query(mysql, sqlbuf)){
					printf("error update File table\n");
					return -1;
				}
			}
			close(fd);
			UserChildListDestroy(&UCL);
			mysql_close(mysql);
			close(zsockfd);
			return 0;
		}
				
		if(FD_ISSET(zsockfd, &rfds)){
			addrlen = sizeof(zclient_addr);
			retrecv = Recvfrom(zsockfd, recvbuff, RECVBUFSIZE, 0, (struct sockaddr*)&zclient_addr, &addrlen);
			if (recvbuff[0] == UPBEGIN) 
			{
				if (UserChildListIfExists(UCL, ntohl(zclient_addr.sin_addr.s_addr), ntohs(zclient_addr.sin_port)) == TRUE)
				{
					UserChildListGetPacknum(UCL, ntohl(zclient_addr.sin_addr.s_addr), ntohs(zclient_addr.sin_port), &npackid);
					UserChildListDelete(&UCL, ntohl(zclient_addr.sin_addr.s_addr), ntohs(zclient_addr.sin_port));
				}
				else
				{
					UserChildListGetPacknumMax(UCL, &npackid);
					npackid++;
				}
				if (npackid <=0)
				{
					npackid = packid;
				}
				if (npackid >= maxpackid)
					npackid = maxpackid - 1;
				UserChildListInsert(&UCL, ntohl(zclient_addr.sin_addr.s_addr), ntohs(zclient_addr.sin_port), npackid);
				sendbuff[0] = UPSENDPACKID;
				itoctemp = (char*)&npackid;
				for(j=0; j<4; j++)
					sendbuff[2+j] = itoctemp[j];
				Sendto(zsockfd, sendbuff, 6, 0, (struct sockaddr*)&zclient_addr, sizeof(zclient_addr));
			}
			
			else if(recvbuff[0] == UPFILEPACK) {
				packrecvid = (unsigned char)recvbuff[5] * (1 << 24) + (unsigned char)recvbuff[4] * (1 << 16) + (unsigned char)recvbuff[3] * (1 << 8) + (unsigned char)recvbuff[2];
				UserChildListGetPacknum(UCL, ntohl(zclient_addr.sin_addr.s_addr), ntohs(zclient_addr.sin_port), &npackid);
				if(packrecvid >= npackid && packrecvid < npackid + PERPACKNUM) 
				{
					UserChildListSetRecvPack(&UCL, ntohl(zclient_addr.sin_addr.s_addr), ntohs(zclient_addr.sin_port), packrecvid - npackid);
					lseek(fd, packrecvid * FILEBLOCKSIZE, SEEK_SET);
					write(fd, &recvbuff[6], retrecv-6);
					if (UserChildListIfRecvPackAll(UCL, ntohl(zclient_addr.sin_addr.s_addr), ntohs(zclient_addr.sin_port)) == TRUE || (packrecvid >= maxpackid - 1 && 
						UserChildListIfRecvPack(UCL, ntohl(zclient_addr.sin_addr.s_addr), ntohs(zclient_addr.sin_port), packrecvid - npackid) == TRUE))
					{
						UserChildListClearRecvPack(&UCL, ntohl(zclient_addr.sin_addr.s_addr), ntohs(zclient_addr.sin_port));
						if (packid == npackid)
						{	
							UserChildListGetPacknumMinGT(UCL, &packid, npackid);
							if (packid == npackid)
							{
								packid += PERPACKNUM;
								if (end_sig == 1)
								{
									//上传完成
									sprintf(sqlbuf, "update File set File_Complete = 1, File_Pack_Num = %d where File_ID = %d;", maxpackid, realfileid);
									if(mysql_query(mysql, sqlbuf)){
										printf("error update File table\n");
										return -1;
									}
									sendbuff[0] = UPFILEEND;
									UserChildList p = UCL->Next;
									while (p)
									{
										zclient_addr.sin_addr.s_addr = htonl(p->IPaddr);
										zclient_addr.sin_port = htons(p->Port);
										sendbuff[0] = UPFILEEND;
										Sendto(zsockfd, sendbuff, 2, 0, (struct sockaddr*)&zclient_addr, sizeof(zclient_addr));
										p = p->Next;
									}
									break;
								}
							}
							if (packid >= maxpackid)
							{
								//上传完成
								sprintf(sqlbuf, "update File set File_Complete = 1, File_Pack_Num = %d where File_ID = %d;", maxpackid, realfileid);
								if(mysql_query(mysql, sqlbuf)){
									printf("error update File table\n");
									return -1;
								}
								UserChildList p = UCL->Next;
								sendbuff[0] = UPFILEEND;
								while (p)
								{
									zclient_addr.sin_addr.s_addr = htonl(p->IPaddr);
									zclient_addr.sin_port = htons(p->Port);
									Sendto(zsockfd, sendbuff, 2, 0, (struct sockaddr*)&zclient_addr, sizeof(zclient_addr));
									p = p->Next;
								}
								break;
							}
						}
						if (end_sig == 0)
						{
							UserChildListGetPacknumMax(UCL, &npackid);
							npackid += PERPACKNUM;
							if (npackid >= maxpackid)
							{
								end_sig = 1;
								UserChildListGetPacknumMinExc(UCL, &npackid, packrecvid - packrecvid % PERPACKNUM);
							}
						}
						else
						{
							UserChildListGetPacknumMinGT(UCL, &npackid, packrecvid - packrecvid % PERPACKNUM);
						}
						UserChildListSetPacknum(&UCL, ntohl(zclient_addr.sin_addr.s_addr), ntohs(zclient_addr.sin_port), npackid);
						sendbuff[0] = UPSENDPACKID;
						itoctemp = (char*)&npackid;
						for(j=0; j<4; j++)
							sendbuff[2+j] = itoctemp[j];
						Sendto(zsockfd, sendbuff, 6, 0, (struct sockaddr*)&zclient_addr, sizeof(zclient_addr));
					}
				}
			}
		}
	}

	close(fd);
	fd = open(filename, O_RDONLY);
	MYMD5(fd, finalmd5);
	for (i = 0; i < 16; i++)
	{
		if (md5[i*2] != md5_char[finalmd5[i] / 16] || md5[i*2+1] != md5_char[finalmd5[i] % 16])
		break;
	}
	if (i != 16)
	{
		sprintf(sqlbuf, "update File set File_Complete = 0, File_Pack_Num = 0 where File_ID = %d;", realfileid);
		if(mysql_query(mysql, sqlbuf))
		{
			printf("error update File table\n");
			return -1;
		}
	}

	UserChildListDestroy(&UCL);
	mysql_close(mysql);

	sendbuff[0] = UPFILEEND;
	while(1)
	{
		FD_ZERO(&rfds);
		FD_SET(zsockfd, &rfds);
		tv.tv_sec = ENDSECONDS;
		tv.tv_usec = 0;
		retselect = Select(zsockfd+1, &rfds, NULL, NULL, &tv);
		if(retselect == -1){
			perror("select error");
			exit(-1);
		}
		else if(retselect == 0){
			break;
		}
		if(FD_ISSET(zsockfd, &rfds)){
			retrecv = Recvfrom(zsockfd, recvbuff, RECVBUFSIZE, 0, (struct sockaddr*)&zclient_addr, &addrlen);
			Sendto(zsockfd, sendbuff, 2, 0, (struct sockaddr*)&zclient_addr, sizeof(zclient_addr));
		}
	}
	close(zsockfd);
	close(fd);
	return 0;
}