DROP DATABASE IF EXISTS OnlineDisk;
CREATE DATABASE OnlineDisk;
USE OnlineDisk;

DROP TABLE IF EXISTS Log;
DROP TABLE IF EXISTS Operation;
DROP TABLE IF EXISTS User;
DROP TABLE IF EXISTS File_User;
DROP TABLE IF EXISTS Directory;
DROP TABLE IF EXISTS File;

/*本地文件表*/
CREATE TABLE File
(
	File_ID BIGINT PRIMARY KEY NOT NULL AUTO_INCREMENT,        /*文件ID*/
	File_Size BIGINT NOT NULL DEFAULT 0,                       /*文件大小*/
	File_MD5 CHAR(50) UNIQUE,                                  /*文件MD5值*/
	File_Index_Num INT NOT NULL DEFAULT 0,                     /*文件关联数量*/
	File_Complete BOOLEAN NOT NULL DEFAULT 0,                  /*文件完整性*/
	File_Pack_Num INT NOT NULL DEFAULT 0                       /*包序号*/
);

/*用户参照目录表*/
CREATE TABLE Directory
(
	Directory_ID BIGINT PRIMARY KEY NOT NULL AUTO_INCREMENT,   /*目录ID*/
	Directory_Name VARCHAR(128) NOT NULL,                       /*目录名*/
	Directory_Parent BIGINT,                                   /*目录父目录*/
	
	FOREIGN KEY(Directory_Parent) REFERENCES Directory(Directory_ID)
);

/*用户参照文件表*/
CREATE TABLE File_User
(
	File_User_ID BIGINT PRIMARY KEY NOT NULL AUTO_INCREMENT,   /*用户文件ID*/
	File_User_Name VARCHAR(128) NOT NULL,                       /*用户文件名*/
	File_User_Parent BIGINT,                                   /*文件父目录*/
	File_User_Index BIGINT,                                    /*文件索引*/
	
	FOREIGN KEY(File_User_Parent) REFERENCES Directory(Directory_ID),
	FOREIGN KEY(File_User_Index) REFERENCES File(File_ID)
);


/*用户表*/
CREATE TABLE User
(
	User_ID INT PRIMARY KEY NOT NULL AUTO_INCREMENT,           /*用户ID(主键)*/
	User_Name VARCHAR(128) NOT NULL UNIQUE,                     /*用户名*/
	User_Password VARCHAR(50) NOT NULL,                        /*用户密码*/
	User_RootDir BIGINT NOT NULL,                              /*用户根目录ID*/

	FOREIGN KEY(User_RootDir) REFERENCES Directory(Directory_ID)
);

/*操作类型表*/
CREATE TABLE Operation
(
	Operation_Type_ID INT PRIMARY KEY NOT NULL,      /*操作类型ID*/
	Operation_Name VARCHAR(20) NOT NULL UNIQUE       /*操作类型名*/
);

/*日志表*/
CREATE TABLE Log
(
	Log_ID BIGINT PRIMARY KEY NOT NULL AUTO_INCREMENT,         /*Log ID*/
	User_ID INT NOT NULL,                                      /*用户ID*/
	Time DATETIME NOT NULL,                                    /*时间*/
	Operation_ID INT NOT NULL,                                 /*操作类型ID*/
	Operate_File_ID BIGINT NOT NULL,                          /*操作文件ID*/

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
