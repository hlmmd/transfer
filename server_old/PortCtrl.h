#ifndef PORTCTRL_H
#define PORTCTRL_H

/*�˿ڿ���*/
typedef struct PortCtrl {
	int allport[100];  /*�˿ڷ����־����λΪ1��ʾ�ö˿�(����λ��+beginport)�ѱ�����*/
	unsigned short beginport;   /*��ʼ����Ķ˿ںţ� �ɷ���Ķ˿ںŴ�beginport��beginport+portsum*/
	int portsum;    /*�ܵĶ˿ڿɷ�����*/
	int leftnum;    /*ʣ��ɷ���˿���*/
}PortCtrl, *PortList;

void PortInit(PortList* pctrl);
int PortAssign(PortList* pctrl);  /*�˿ڷ���*/
void PortRelease(PortList* pctrl, unsigned short port); /*�˿��ͷ�*/

#endif