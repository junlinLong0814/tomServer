#ifndef _HTTP_CONN_H_
#define _HTTP_CONN_H_
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fstream>
#include <string.h>
#include <string>
#include <iostream>
#include <sys/mman.h>
#include <stdarg.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h> 
#include "sqlConnpool.h"
#include "Log.h"

#ifndef RECV_BUF_SIZE 	
#define RECV_BUF_SIZE 4096		//接收缓冲区大小
#endif

#ifndef SEND_BUF_SIZE
#define SEND_BUF_SIZE 4096		//发送缓冲区大小
#endif


/*HTTP请求方法*/
enum METHOD
{
	GET=0,
	POST,
	HEAD,
	PUT,
	DELETE,
	TRACE,
	OPTIONS,
	CONNECT,
	PATCH
};
/*主状态机两种可能
1.请求分析请求行
2.请求分析请求头
3.请求分析请求内容*/
enum CHECK_STATE
{
    CHECK_STATE_REQUESTLINE=0,
    CHECK_STATE_REQUESTHEADER,
    CHECK_STATE_CONTENT
};
/*从状态机的三种状态
1.读取到一个完整的行
2.读取到一个不完整的行
3.读取到一个错误的行（即HTTP格式错误）
 */
enum LINE_STATUS
{
    LINE_COMPLETE=0,
    LINE_CONTINUE,
    LINE_BAD
};
/*HTTP的解析结果
1.GET请求
2.请求不完整
3.请求不合规
*/
enum HTTP_CODE
{
    GET_REQUEST,
    INCOMPLETE_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERRNO,
    CLOSED_CONNECTION
};


class httpConn
{
public:
	httpConn(int fd=-1);
	~httpConn();
	void close_conn(bool real_close=true);
	/*供主线程使用的工作函数*/
	bool testAPI(int epollfd,int fd);
	inline int getfd(){return httpfd_;}
private:
	/*epoll事件修改删除*/
	void removefd();
	void modfd();

	/*ET模式读*/
	bool read();
	/*初始化连接*/
	void init();
	/*解析HTTP请求*/
	HTTP_CODE process_read();
	/*被process_read调用 以分析 HTTP请求*/
	/*解析行内容*/
	LINE_STATUS parse_line();
	/*分析请求行*/
	HTTP_CODE parse_requestLine();
	/*分析请求头*/
	HTTP_CODE parse_requestHeader();
	/*解析HTTP报文*/
	HTTP_CODE parse_content();
	/*把请求文件写入文件缓冲区,目前仅支持2KB大小文件*/
	int addFile(const char* fileName);
	/*响应HTTP报文*/
	bool responseByRet(HTTP_CODE ret);
	/*下面这组函数被process_write调用 以填充 HTTP响应*/
private:
	char recvBuf_[RECV_BUF_SIZE];	//接收缓冲区
	char sendBuf_[SEND_BUF_SIZE];	//发送缓冲区
	int epollfd_;					//主线程的epollfd
	int httpfd_;					//该fd有待处理的http事件 
	int nlastReadIndex_;			//接收缓冲区结尾位置
	int nRowStart_;					//http报文每行行首
	int nRowEnd_;					//指向解析最新数据的最后一个行尾
	int readData_;					//本次读取的数据量
	int nLastWriteIndex_;			//写缓冲区结尾位置

	CHECK_STATE check_state_;		//主状态机状态	
	METHOD method_;					//请求方法

	char* url_;						//客户请求目标文件的文件名
	char* version_;					//协议版本号，仅支持1.1
	char* host_;					//主机名
	int contentLength_;				//请求的消息体长度
	bool linger_;					//是否长连接

	char* fileAddress_;				//客户请求的目标文件被mmap到内存中的起始位置

	sqlConnPool* sqlPool_;			//数据库连接池(单例类)
	

};

#endif
