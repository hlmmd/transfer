#ifndef AES_H
#define AES_H
int encrypt(char *input_string, char* encrypt_string, int len);
void decrypt(char *encrypt_string, char *decrypt_string, int len);
ssize_t Recvfrom(int sockfd, char* buff, size_t nbytes, int flags, struct sockaddr *from, socklen_t *addrlen);
ssize_t Sendto(int sockfd, char* buff, size_t nbytes, int flags, const struct sockaddr *to, socklen_t addrlen);
#endif
//编译 -lcrypto