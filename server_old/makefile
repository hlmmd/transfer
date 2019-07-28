all : netdisk
netdisk : netdisk.c PortCtrl.c aes.c UserList.c ChildList.c UserChildList.c md5.c
	gcc -o netdisk netdisk.c PortCtrl.c aes.c UserList.c ChildList.c UserChildList.c md5.c -I/usr/include/mysql -L/usr/lib64/mysql -lmysqlclient -lcrypto
clean :
	rm -f netdisk