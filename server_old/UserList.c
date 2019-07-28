#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "UserList.h"

Status UserListInit(UserList* UL)
{
	(*UL) = (UserList)malloc(sizeof(UserNode));
	if (!(*UL))
		exit(OVERFLOW);

	(*UL)->IPaddr = 0;
	(*UL)->Port   = 0;
	(*UL)->UserID = 0;
	(*UL)->Heart  = 0;

	(*UL)->Next   = NULL;
	return OK;
}

Status UserListDestroy(UserList* UL)
{
	UserList p = (*UL), q;
	while (p)
	{
		q = p->Next;
		free(p);
		p = q;
	}
	return OK;
}

int    UserListLocateElem(UserList UL, int UserID)
{
	UserList p = UL->Next;
	int       i = 1;

	while(p)
	{
		if (p->UserID == UserID)
			return i;
		p = p->Next;
		i++;
	}
        
	return 0;
}

int    UserListLength(UserList UL)
{
	UserList p = UL->Next;
	int       i = 0;

	while(p)
	{
		p = p->Next;
		i++;
	}
        
	return i;
}

Status UserListIfExists(UserList UL, int UserID)
{
	UserList p = UL->Next;

	while(p)
	{
		if (p->UserID == UserID)
			return TRUE;
		p = p->Next;
	}
        
	return FALSE;
}

Status UserListIfExistsbyIP(UserList UL, unsigned long IPaddr, unsigned short Port)
{
	UserList p = UL->Next;

	while(p)
	{
		if (p->IPaddr == IPaddr && p->Port == Port)
			return TRUE;
		p = p->Next;
	}
        
	return FALSE;
}

Status UserListInsert(UserList* UL, unsigned long IPaddr, unsigned short Port, int UserID)
{
	UserList s = (UserList)malloc(sizeof(UserNode));
	if (!s)
		exit(OVERFLOW);
	s->IPaddr = IPaddr;
	s->Port   = Port;
	s->UserID = UserID;
	s->Heart  = 0;
	s->Next    = (*UL)->Next;
	(*UL)->Next = s;
	return OK;
}

Status UserListDelete(UserList* UL, int UserID)
{
	UserList p = (*UL), q;
	for (; p && p->Next && p->Next->UserID != UserID; p = p->Next);

	if (!p || !p->Next)
		return ERR;
	q = p->Next;
	p->Next = p->Next->Next;
	free(q);
	return OK;
}

Status UserListDeletebyIP(UserList* UL, unsigned long IPaddr, unsigned short Port)
{
	UserList p = (*UL), q;
	for (; p && p->Next && (p->Next->IPaddr != IPaddr || p->Next->Port !=  Port); p = p->Next);

	if (!p || !p->Next)
		return ERR;
	q = p->Next;
	p->Next = p->Next->Next;
	free(q);
	return OK;
}
Status UserListSetHeart(UserList* UL, unsigned long IPaddr, unsigned short Port, int Heart)
{
	UserList p = (*UL), q;
	for (; p && p->Next && (p->Next->IPaddr != IPaddr || p->Next->Port != Port); p = p->Next);

	if (!p || !p->Next)
		return ERR;

	p->Next->Heart = Heart;
	return OK;
}
int UserListRetUserID(UserList UL, unsigned long IPaddr, unsigned short Port)
{
	UserList p = UL->Next;

	while(p)
	{
		if (p->IPaddr == IPaddr && p->Port == Port)
			return p->UserID;
		p = p->Next;
	}
        
	return -1;
}