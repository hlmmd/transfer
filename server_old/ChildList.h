#define TRUE		1
#define FALSE		0
#define OK		1
#define ERR		0
#define INFEASIBE	-1
#define OVERFLOW	-2

typedef int Status;

typedef struct ChildNode
{
	int Pid;
	short Port;
	char Md5[16];
	struct ChildNode* Next;
}ChildNode, *ChildList;

Status ChildListInit(ChildList* CL);
Status ChildListDestroy(ChildList* CL);
int    ChildListLocateElem(ChildList CL, int Pid);
int    ChildListLength(ChildList CL);
Status ChildListIfExists(ChildList CL, char* Md5);
Status ChildListInsert(ChildList* CL, int Pid, short Port, char* Md5);
Status ChildListDelete(ChildList* CL, int Pid);
short  ChildListGetPort(ChildList CL, char* Md5, short* Port);
short  ChildListGetPortByPid(ChildList CL, int Pid, short* Port);