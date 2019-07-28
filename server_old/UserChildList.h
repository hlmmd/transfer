#define TRUE		1
#define FALSE		0
#define OK		1
#define ERR		0
#define INFEASIBE	-1
#define OVERFLOW	-2

#define INT_MAX 1 << 31 - 1

typedef int Status;

typedef struct UserChildNode
{
	long IPaddr;
	short Port;
	char RecvPack;
	int Packnum;
	struct UserChildNode* Next;
}UserChildNode, *UserChildList;

Status UserChildListInit(UserChildList* UCL);
Status UserChildListDestroy(UserChildList* UCL);
int    UserChildListLocateElem(UserChildList UCL, long IPaddr, short Port);
int    UserChildListLength(UserChildList UCL);
Status UserChildListIfExists(UserChildList UCL, long IPaddr, short Port);
Status UserChildListInsert(UserChildList* UCL, long IPaddr, short Port, int Packnum);
Status UserChildListDelete(UserChildList* UCL, long IPaddr, short Port);
int    UserChildListGetPacknum(UserChildList UCL, long IPaddr, short Port, int* Packnum);
int    UserChildListGetPacknumMin(UserChildList UCL, int* Packnum);
int    UserChildListGetPacknumMinExc(UserChildList UCL, int* Packnum, int ExcPacknum);
int    UserChildListGetPacknumMinGT(UserChildList UCL, int* Packnum, int ExcPacknum);
int    UserChildListGetPacknumMax(UserChildList UCL, int* Packnum);
Status UserChildListSetPacknum(UserChildList* UCL, long IPaddr, short Port, int Packnum);
Status UserChildListIfRecvPackAll(UserChildList UCL, long IPaddr, short Port);
Status UserChildListIfRecvPack(UserChildList UCL, long IPaddr, short Port, int Packnum);
Status UserChildListSetRecvPack(UserChildList* UCL, long IPaddr, short Port, int Packnum);
Status UserChildListClearRecvPack(UserChildList* UCL, long IPaddr, short Port);