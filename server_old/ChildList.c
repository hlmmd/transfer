#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "ChildList.h"

Status ChildListInit(ChildList* CL)
{
	(*CL) = (ChildList)malloc(sizeof(ChildNode));
	if (!(*CL))
		exit(OVERFLOW);

	(*CL)->Pid = 0;
	(*CL)->Port   = 0;
	memset((*CL)->Md5, 0, sizeof((*CL)->Md5));

	(*CL)->Next   = NULL;
	return OK;
}

Status ChildListDestroy(ChildList* CL)
{
	ChildList p = (*CL), q;
	while (p)
	{
		q = p->Next;
		free(p);
		p = q;
	}
	return OK;
}

int    ChildListLocateElem(ChildList CL, int Pid)
{
	ChildList p = CL->Next;
	int       i = 1;

	while(p)
	{
		if (p->Pid == Pid)
			return i;
		p = p->Next;
		i++;
	}
        
	return 0;
}

int    ChildListLength(ChildList CL)
{
	ChildList p = CL->Next;
	int       i = 0;

	while(p)
	{
		p = p->Next;
		i++;
	}
        
	return i;
}

Status ChildListIfExists(ChildList CL, char* Md5)
{
	ChildList p = CL->Next;

	if (Md5 == NULL)
		return FALSE;
	while(p)
	{
		if (p->Md5 != NULL && memcmp(p->Md5, Md5, sizeof(p->Md5)) == 0)
			return TRUE;
		p = p->Next;
	}
        
	return FALSE;
}

Status ChildListInsert(ChildList* CL, int Pid, short Port, char* Md5)
{
	ChildList s = (ChildList)malloc(sizeof(ChildNode));
	if (!s)
		exit(OVERFLOW);
	s->Pid    = Pid;
	s->Port   = Port;
	if (Md5 == NULL)
		memset(s->Md5, 0, sizeof(s->Md5));
	else
		memcpy(s->Md5, Md5, sizeof(s->Md5));
	s->Next    = (*CL)->Next;
	(*CL)->Next = s;
	return OK;
}

Status ChildListDelete(ChildList* CL, int Pid)
{
	ChildList p = (*CL), q;
	for (; p && p->Next && p->Next->Pid != Pid; p = p->Next);

	if (!p || !p->Next)
		return ERR;
	q = p->Next;
	p->Next = p->Next->Next;
	free(q);
	return OK;
}

short  ChildListGetPort(ChildList CL, char* Md5, short* Port)
{
	ChildList p = CL, q;
	for (; p && p->Next && (p->Next->Md5 == NULL || memcmp(p->Next->Md5, Md5, sizeof(p->Next->Md5)) != 0); p = p->Next);

	if (!p || !p->Next)
		return ERR;

	(*Port) = p->Next->Port;
	return OK;
}

short  ChildListGetPortByPid(ChildList CL, int Pid, short* Port)
{
	ChildList p = CL, q;
	for (; p && p->Next && p->Next->Pid != Pid; p = p->Next);

	if (!p || !p->Next)
		return ERR;

	(*Port) = p->Next->Port;
	return OK;
}