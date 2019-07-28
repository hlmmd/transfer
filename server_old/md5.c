#include <stdio.h>
#include <openssl/md5.h>
#include <stdlib.h>
#include <string.h>
#include "md5.h"
void MYMD5(int fd, unsigned char* des)
{
	MD5_CTX c;
	unsigned char md5[MD5_DIGEST_LENGTH] = { 0 };
	char pData[BUFFERSIZE];
	int len;
	int i;
	MD5_Init(&c);
	while (1)
	{
		len = read(fd, pData, BUFFERSIZE);
		if (len <= 0)
			break;
		if (len > 0)
		{
			MD5_Update(&c, pData, len);
		}
	}
	MD5_Final(md5, &c);

	for (i = 0; i < MD5_DIGEST_LENGTH; i++)
	{
		des[i] = md5[i];
	}
}