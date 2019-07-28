#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "UserChildList.h"

Status UserChildListInit(UserChildList* UCL)
{
	(*UCL) = (UserChildList)malloc(sizeof(UserChildNode));
	if (!(*UCL))
		exit(OVERFLOW);

	(*UCL)->IPaddr  = 0;
	(*UCL)->Port    = 0;
	(*UCL)->Packnum = 0;
	(*UCL)->RecvPack = 0;
	(*UCL)->Next   = NULL;
	return OK;
}

Status UserChildListDestroy(UserChildList* UCL)
{
	UserChildList p = (*UCL), q;
	while (p)
	{
		q = p->Next;
		free(p);
		p = q;
	}
	return OK;
}

int    UserChildListLocateElem(UserChildList UCL, long IPaddr, short Port)
{
	UserChildList p = UCL->Next;
	int       i = 1;

	while(p)
	{
		if (p->IPaddr == IPaddr && p->Port == Port)
			return i;
		p = p->Next;
		i++;
	}
        
	return 0;
}

int    UserChildListLength(UserChildList UCL)
{	
	UserChildList p = UCL->Next;
	int       i = 0;

	while(p)
	{
		p = p->Next;
		i++;
	}
        
	return i;
}

Status UserChildListIfExists(UserChildList UCL, long IPaddr, short Port)
{
	UserChildList p = UCL->Next;

	while(p)
	{
		if (p->IPaddr == IPaddr && p->Port == Port)
			return TRUE;
		p = p->Next;
	}
        
	return FALSE;
}

Status UserChildListInsert(UserChildList* UCL, long IPaddr, short Port, int Packnum)
{
	UserChildList s = (UserChildList)malloc(sizeof(UserChildNode));
	if (!s)
		exit(OVERFLOW);
	s->IPaddr  = IPaddr;
	s->Port    = Port;
	s->Packnum = Packnum;
	s->RecvPack = 0;
	s->Next    = (*UCL)->Next;
	(*UCL)->Next = s;
	return OK;
}

Status UserChildListDelete(UserChildList* UCL, long IPaddr, short Port)
{
	UserChildList p = (*UCL), q;
	for (; p && p->Next && (p->Next->IPaddr != IPaddr || p->Next->Port != Port); p = p->Next);

	if (!p || !p->Next)
		return ERR;
	q = p->Next;
	p->Next = p->Next->Next;
	free(q);
	return OK;
}

int    UserChildListGetPacknum(UserChildList UCL, long IPaddr, short Port, int* Packnum)
{
	UserChildList p = UCL, q;
	for (; p && p->Next && (p->Next->IPaddr != IPaddr || p->Next->Port != Port); p = p->Next);

	if (!p || !p->Next)
		return ERR;

	(*Packnum) = p->Next->Packnum;
	return OK;
}

int    UserChildListGetPacknumMin(UserChildList UCL, int* Packnum)
{
	UserChildList p = UCL->Next;
	int Min;

	if (!p)
	{
		(*Packnum) = -1;
		return -1;
	}
	Min = p->Packnum;
	while (p)
	{
		if (p->Packnum < Min)
			Min = p->Packnum;
		p = p->Next;
	}

	(*Packnum) = Min;
	return Min;
}

int    UserChildListGetPacknumMinExc(UserChildList UCL, int* Packnum, int ExcPacknum)
{
	UserChildList p = UCL->Next;
	int Min;

	if (!p)
	{
		(*Packnum) = -1;
		return -1;
	}
	Min = INT_MAX;
	while (p)
	{
		if (p->Packnum < Min && p->Packnum != ExcPacknum)
			Min = p->Packnum;
		p = p->Next;
	}
	if (Min == INT_MAX)
		Min = UCL->Next->Packnum;

	(*Packnum) = Min;
	return Min;
}

int    UserChildListGetPacknumMinGT(UserChildList UCL, int* Packnum, int ExcPacknum)
{
	UserChildList p = UCL->Next;
	int Min;

	if (!p)
	{
		(*Packnum) = -1;
		return -1;
	}
	Min = INT_MAX;
	while (p)
	{
		if (p->Packnum < Min && p->Packnum > ExcPacknum)
			Min = p->Packnum;
		p = p->Next;
	}
	if (Min == INT_MAX)
		Min = ExcPacknum;

	(*Packnum) = Min;
	return Min;
}

int    UserChildListGetPacknumMax(UserChildList UCL, int* Packnum)
{
	UserChildList p = UCL->Next;
	int Max;

	if (!p)
	{
		(*Packnum) = -1;
		return -1;
	}
	Max = p->Packnum;
	while (p)
	{
		if (p->Packnum > Max)
			Max = p->Packnum;
		p = p->Next;
	}

	(*Packnum) = Max;
	return Max;
}

Status UserChildListSetPacknum(UserChildList* UCL, long IPaddr, short Port, int Packnum)
{
	UserChildList p = (*UCL)->Next;
	int Max;

	if (!p)
	{
		return ERR;
	}
	while (p)
	{
		if (p->IPaddr == IPaddr && p->Port == Port)
		{
			p->Packnum = Packnum;
			return OK;
		}
		p = p->Next;
	}

	return ERR;
}

Status UserChildListIfRecvPackAll(UserChildList UCL, long IPaddr, short Port)
{
	UserChildList p = UCL->Next;

	while (p)
	{
		if (p->IPaddr == IPaddr && p->Port == Port)
		{
			if ((p->RecvPack & 0xff) == 0xff)
			{
				return TRUE;
			}
			else
				return FALSE;
		}
		p = p->Next;
	}
	return FALSE;
}

Status UserChildListIfRecvPack(UserChildList UCL, long IPaddr, short Port, int Packnum)
{
	int i;
	UserChildList p = UCL->Next;

	while (p)
	{
		if (p->IPaddr == IPaddr && p->Port == Port)
		{
			for (i = 0; i <= Packnum; i++)
			{
				if ((p->RecvPack & (1 << i)) != (1 << i))
				{
					return FALSE;
				}
			}
			return TRUE;
		}
		p = p->Next;
	}
	return FALSE;
}

Status UserChildListSetRecvPack(UserChildList* UCL, long IPaddr, short Port, int Packnum)
{
	UserChildList p = (*UCL)->Next;

	while (p)
	{
		if (p->IPaddr == IPaddr && p->Port == Port)
		{
			p->RecvPack |= (1 << Packnum);
			return OK;
		}
		p = p->Next;
	}
	return ERR;
}

Status UserChildListClearRecvPack(UserChildList* UCL, long IPaddr, short Port)
{
	UserChildList p = (*UCL)->Next;

	while (p)
	{
		if (p->IPaddr == IPaddr && p->Port == Port)
		{
			p->RecvPack &= 0x00;
			return OK;
		}
		p = p->Next;
	}
	return ERR;
}