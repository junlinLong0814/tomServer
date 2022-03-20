#ifndef _HTTP_SERVER_H_
#define _HTTP_SERVER_H_

#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <vector>
#include "workServer.h"
//#include "Client.h"
#include "httpConn.h"
#include "Log.h"

#define MAX_EVENT_NUMBER 10000

class httpServer
{
public:
	httpServer(int port=8888);
	~httpServer();
	/*用于开启服务器*/
	void startServer();
private:
	/*服务器工作函数*/
	int run();
	/*用于开启工作服务器*/
	int startWorker();
	/*注册 fd 非阻塞*/
	int setNonBlock(int fd);
	/*注册 Listenfd 上 EPOLLIN 事件*/
	void addfd(int fd,bool et_enable);
	/*将客户分配给负载低的工作线程*/
	void assignClient(int fd);
	
private:
	epoll_event events_[MAX_EVENT_NUMBER];				//epoll事件
	std::vector<workServer*> workers_;					//工作线程
	struct sockaddr_in address_;						//服务器地址
	struct sockaddr_in client_address_;					//客户地址
	socklen_t client_addrlength_;						//客户地址结构体长度
	int listenfd_;										//监听socket
	int clientfd_;										//客户fd
	int epollfd_;										//epollfd
	short port_;										//端口
};

#endif
