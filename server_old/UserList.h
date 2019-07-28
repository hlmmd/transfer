#define TRUE		1
#define FALSE		0
#define OK		1
#define ERR		0
#define INFEASIBE	-1
#define OVERFLOW	-2

typedef int Status;

typedef struct UserNode
{
	unsigned long IPaddr;
	unsigned short Port;
	int UserID;
	int Heart;
	struct UserNode* Next;
}UserNode, *UserList;

Status UserListInit(UserList* UL);
Status UserListDestroy(UserList* UL);
int    UserListLocateElem(UserList UL, int UserID);
int    UserListLength(UserList UL);
Status UserListIfExists(UserList UL, int UserID);
Status UserListIfExistsbyIP(UserList UL, unsigned long IPaddr, unsigned short Port);
Status UserListInsert(UserList* UL, unsigned long IPaddr, unsigned short Port, int UserID);
Status UserListDelete(UserList* UL, int UserID);
Status UserListDeletebyIP(UserList* UL, unsigned long IPaddr, unsigned short Port);
Status UserListSetHeart(UserList* UL, unsigned long IPaddr, unsigned short Port, int Heart);
int    UserListRetUserID(UserList UL, unsigned long IPaddr, unsigned short Port);