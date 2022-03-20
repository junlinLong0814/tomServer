#ifndef _WORK_SERVER_H_
#define _WORK_SERVER_H_

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
#include <map>
#include <mutex>
#include <thread>
#include "threadpool.hpp"
#include "httpConn.h"
#include "sqlConnpool.h"
#include "Log.h"

#define WORK_MAX_EVENT_NUMBER 10000

class workServer
{
public:
	workServer();
	~workServer();

	/*查看现在负载情况*/
	int load();
	/*启动工作服务器*/
	void startWorkerServer();
	/*将新客户加入到缓冲队列中*/
	void addClient(int fd);
private:
	/*注册 fd 非阻塞*/
	int setNonBlock(int fd);
	/*注册 Listenfd 上 EPOLLIN 事件*/
	void addfd(int fd,bool et,bool one_shot);
	/*删除 Epoll上 的一个fd*/
	void removefd(int fd);
	/*运行函数*/
	void run();
	/*处理活跃fd*/
	void workOnFds(int number);
private:
	std::vector<int> pHttpConnBuf_;							//主线程与工作线程的缓冲队列
	std::mutex mPHttpConnBuf_;								//互斥访问缓冲队列
	httpConn* httpConns_;									//一组http解析类
	std::thread* ptheard_;									//工作线程
	threadPool<workServer>* pThreadPool_;					//线程池
	sqlConnPool* pSqlPool_;									//数据库连接池
	int epollfd_;											//epollfd
	epoll_event events_[WORK_MAX_EVENT_NUMBER];				//epoll events
};

#endif
