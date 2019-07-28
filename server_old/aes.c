#include <stdio.h>
#include <openssl/aes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

void encrypt(char *input_string, char *encrypt_string, int len)
{
	AES_KEY aes;
	unsigned char key[AES_BLOCK_SIZE]; // AES_BLOCK_SIZE = 16
	unsigned char iv[AES_BLOCK_SIZE];  // init vector
	int i;

	// Generate AES 128-bit key
	for (i = 0; i < 16; i++)
	{
		key[i] = i;
	}

	// Set encryption key
	for (i = 0; i < AES_BLOCK_SIZE; i++)
	{
		iv[i] = 0;
	}
	if (AES_set_encrypt_key(key, 128, &aes) < 0)
	{
		printf("Unable to set encryption key in AES\n");
		exit(0);
	}

	// encrypt (iv will change)
	AES_cbc_encrypt(input_string, encrypt_string, len, &aes, iv, AES_ENCRYPT);
}

void decrypt(char *encrypt_string, char *decrypt_string, int len)
{
	unsigned char key[AES_BLOCK_SIZE]; // AES_BLOCK_SIZE = 16
	unsigned char iv[AES_BLOCK_SIZE];  // init vector
	AES_KEY aes;
	int i;
	// Generate AES 128-bit key

	for (i = 0; i < 16; ++i)
	{
		key[i] = i;
	}

	// Set decryption key
	for (i = 0; i < AES_BLOCK_SIZE; ++i)
	{
		iv[i] = 0;
	}
	if (AES_set_decrypt_key(key, 128, &aes) < 0)
	{
		printf("Unable to set decryption key in AES\n");
		exit(-1);
	}

	// decrypt
	AES_cbc_encrypt(encrypt_string, decrypt_string, len, &aes, iv, AES_DECRYPT);
}

//(n/16+1)*16
ssize_t Recvfrom(int sockfd, char *buff, size_t nbytes, int flags, struct sockaddr *from, socklen_t *addrlen)
{
	int n;
	char recvbuf[1500];
	n = recvfrom(sockfd, recvbuf, nbytes, flags, from, addrlen);
	if (n == -1)
		return n;
	if (n < 16) //接收小于16字节错误返回0
		return 0;
	decrypt(recvbuf, buff, n);
	return n - buff[1];
}
ssize_t Sendto(int sockfd, char *buff, size_t nbytes, int flags, const struct sockaddr *to, socklen_t addrlen)
{
	int n;
	char sendbuf[1500];
	n = (nbytes / 16 + 1) * 16;
	if (nbytes % 16 == 0)
		buff[1] = 16;
	else
		buff[1] = 16 - nbytes % 16;
	encrypt(buff, sendbuf, n);
	return sendto(sockfd, sendbuf, n, flags, to, addrlen);
}
