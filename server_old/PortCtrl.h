#ifndef PORTCTRL_H
#define PORTCTRL_H

/*端口控制*/
typedef struct PortCtrl {
	int allport[100];  /*端口分配标志，该位为1表示该端口(所在位数+beginport)已被分配*/
	unsigned short beginport;   /*开始分配的端口号， 可分配的端口号从beginport至beginport+portsum*/
	int portsum;    /*总的端口可分配数*/
	int leftnum;    /*剩余可分配端口数*/
}PortCtrl, *PortList;

void PortInit(PortList* pctrl);
int PortAssign(PortList* pctrl);  /*端口分配*/
void PortRelease(PortList* pctrl, unsigned short port); /*端口释放*/

#endif