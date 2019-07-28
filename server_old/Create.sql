DROP DATABASE IF EXISTS OnlineDisk;
CREATE DATABASE OnlineDisk;
USE OnlineDisk;

DROP TABLE IF EXISTS Log;
DROP TABLE IF EXISTS Operation;
DROP TABLE IF EXISTS User;
DROP TABLE IF EXISTS File_User;
DROP TABLE IF EXISTS Directory;
DROP TABLE IF EXISTS File;

/*�����ļ���*/
CREATE TABLE File
(
	File_ID BIGINT PRIMARY KEY NOT NULL AUTO_INCREMENT,        /*�ļ�ID*/
	File_Size BIGINT NOT NULL DEFAULT 0,                       /*�ļ���С*/
	File_MD5 CHAR(50) UNIQUE,                                  /*�ļ�MD5ֵ*/
	File_Index_Num INT NOT NULL DEFAULT 0,                     /*�ļ���������*/
	File_Complete BOOLEAN NOT NULL DEFAULT 0,                  /*�ļ�������*/
	File_Pack_Num INT NOT NULL DEFAULT 0                       /*�����*/
);

/*�û�����Ŀ¼��*/
CREATE TABLE Directory
(
	Directory_ID BIGINT PRIMARY KEY NOT NULL AUTO_INCREMENT,   /*Ŀ¼ID*/
	Directory_Name VARCHAR(128) NOT NULL,                       /*Ŀ¼��*/
	Directory_Parent BIGINT,                                   /*Ŀ¼��Ŀ¼*/
	
	FOREIGN KEY(Directory_Parent) REFERENCES Directory(Directory_ID)
);

/*�û������ļ���*/
CREATE TABLE File_User
(
	File_User_ID BIGINT PRIMARY KEY NOT NULL AUTO_INCREMENT,   /*�û��ļ�ID*/
	File_User_Name VARCHAR(128) NOT NULL,                       /*�û��ļ���*/
	File_User_Parent BIGINT,                                   /*�ļ���Ŀ¼*/
	File_User_Index BIGINT,                                    /*�ļ�����*/
	
	FOREIGN KEY(File_User_Parent) REFERENCES Directory(Directory_ID),
	FOREIGN KEY(File_User_Index) REFERENCES File(File_ID)
);


/*�û���*/
CREATE TABLE User
(
	User_ID INT PRIMARY KEY NOT NULL AUTO_INCREMENT,           /*�û�ID(����)*/
	User_Name VARCHAR(128) NOT NULL UNIQUE,                     /*�û���*/
	User_Password VARCHAR(50) NOT NULL,                        /*�û�����*/
	User_RootDir BIGINT NOT NULL,                              /*�û���Ŀ¼ID*/

	FOREIGN KEY(User_RootDir) REFERENCES Directory(Directory_ID)
);

/*�������ͱ�*/
CREATE TABLE Operation
(
	Operation_Type_ID INT PRIMARY KEY NOT NULL,      /*��������ID*/
	Operation_Name VARCHAR(20) NOT NULL UNIQUE       /*����������*/
);

/*��־��*/
CREATE TABLE Log
(
	Log_ID BIGINT PRIMARY KEY NOT NULL AUTO_INCREMENT,         /*Log ID*/
	User_ID INT NOT NULL,                                      /*�û�ID*/
	Time DATETIME NOT NULL,                                    /*ʱ��*/
	Operation_ID INT NOT NULL,                                 /*��������ID*/
	Operate_File_ID BIGINT NOT NULL,                          /*�����ļ�ID*/

	FOREIGN KEY(Operation_ID) REFERENCES Operation(Operation_Type_ID)
);

INSERT INTO File VALUES(1, 0, '00000000000000000000000000000000', 0, 1, 0);

insert into Operation values(1, "SIGNIN");
insert into Operation values(2, "SIGNUP");
insert into Operation values(3, "SIGNOUT");
insert into Operation values(4, "USERFS");
insert into Operation values(5, "UPLOADFILE");
insert into Operation values(6, "UPLOADDIR");
insert into Operation values(7, "DOWNLOADFILE");
insert into Operation values(8, "CHANGEFILENAME");
insert into Operation values(9, "COPYFILE");
insert into Operation values(10, "COPYDIR");
insert into Operation values(11, "MOVEFILE");
insert into Operation values(12, "MOVEDIR");
insert into Operation values(13, "DELETEFILE");
insert into Operation values(14, "DELETEDIR");
