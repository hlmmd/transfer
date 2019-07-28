#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "PortCtrl.h"

void PortInit(PortList *pctrl)
{
	int i;

	(*pctrl) = (PortList)malloc(sizeof(PortCtrl));
	(*pctrl)->beginport = 10000;
	(*pctrl)->portsum = 3200;
	(*pctrl)->leftnum = 3200;
	for (i = 0; i < 100; i++)
		(*pctrl)->allport[i] = 0x00000000;
}

/**/
int PortAssign(PortList *pctrl)
{
	int i, j;

	if ((*pctrl)->leftnum == 0)
		return -1;

	for (i = 0; i < (*pctrl)->portsum / 32; i++)
		if ((*pctrl)->allport[i] != 0xffffffff)
			break;
	for (j = 31; j >= 0; j--)
		if (((*pctrl)->allport[i] & (1 << j)) == 0x00000000)
			break;

	(*pctrl)->allport[i] = ((*pctrl)->allport[i] | (1 << j));
	(*pctrl)->leftnum--;
	return (*pctrl)->beginport + 32 * i + 31 - j;
}

/**/
void PortRelease(PortList *pctrl, unsigned short port)
{
	short n = port - (*pctrl)->beginport;
	int i = n / 32;
	int j = 31 - n % 32;
	(*pctrl)->allport[i] = ((*pctrl)->allport[i] ^ (1 << j));
	(*pctrl)->leftnum++;
}