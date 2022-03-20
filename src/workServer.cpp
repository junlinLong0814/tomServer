#include "workServer.h"
#define DATABASE_USER "ljl"
#define DATABASE_PWD "123456"

#ifndef MAX_FD
#define MAX_FD 65536
#endif

int workServer::setNonBlock(int fd)
{
	int old_option = fcntl(fd,F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd,F_SETFL,new_option);
	return old_option;
}

void workServer::removefd(int fd)
{
	epoll_ctl(epollfd_,EPOLL_CTL_DEL,fd,0);
}

void workServer::addfd(int fd,bool et,bool one_shot)
{
	epoll_event event;
	event.data.fd=fd;
	event.events = EPOLLIN | EPOLLRDHUP;
	if(et)
	{
		event.events |= EPOLLET;
	}
	if(one_shot)
	{
		event.events |= EPOLLONESHOT;	
	}
	epoll_ctl(epollfd_,EPOLL_CTL_ADD,fd,&event);
	setNonBlock(fd);
}

workServer::workServer():ptheard_(nullptr),epollfd_(-1)
{
	httpConns_ = new httpConn[MAX_FD];

	pSqlPool_ = sqlConnPool::getInstance();
	pSqlPool_->init("localhost",DATABASE_USER,DATABASE_PWD,"test",0,15,0);	
}


void workServer::startWorkerServer()
{
	ptheard_ = new std::thread(&workServer::run,this);
	ptheard_->detach();
	pThreadPool_ = new threadPool<workServer>(4,10000);
}

void workServer::addClient(int fd)
{

	std::lock_guard<std::mutex> lock(mPHttpConnBuf_);
	pHttpConnBuf_.push_back(fd);
}

void workServer::run()
{
	epollfd_ = epoll_create(WORK_MAX_EVENT_NUMBER);
	if(epollfd_ == -1)
	{
		LOG_ERROR("WorkServer epoll fd errno : %d\n",errno);
		return ;
	}
	while(true)
	{
		if(!pHttpConnBuf_.empty())
		{
			std::lock_guard<std::mutex> lock(mPHttpConnBuf_);
			for(auto tmpFd : pHttpConnBuf_)
			{
				addfd(tmpFd,true,true);
				//addfd(fd,true,false);
			}
			{
				std::vector<int>().swap(pHttpConnBuf_);
			}
			pHttpConnBuf_.clear();
		}
		int activefds =  epoll_wait(epollfd_,events_,WORK_MAX_EVENT_NUMBER,10);
		if(-1 == activefds)
		{
			LOG_ERROR("WorkServer epoll_wait errno : %d\n",errno);
			break;
		} 
		workOnFds(activefds);
	}
	close(epollfd_);
	epollfd_ = -1;
}	

void workServer::workOnFds(int number)
{
	for(int i=0 ;i<number; ++i)
	{
		int activefd = events_[i].data.fd;

		if(events_[i].events & EPOLLIN)
		{
			//httpConns_[activefd].testAPI(this->epollfd_,activefd);
			pThreadPool_->addTask([this,activefd](){this->httpConns_[activefd].testAPI(this->epollfd_,activefd);});
		}
		else if(events_[i].events & EPOLLRDHUP)
		{
			/*test sth*/
			LOG_WARNING("EPOLLRDHUP!\n");
			removefd(activefd);
			close(activefd);
		}
		else
		{
			LOG_WARNING("WorkServer epoll sth else happened\n");
			continue;
		}
	}
}

int workServer::load()
{
	std::lock_guard<std::mutex> lock(mPHttpConnBuf_);
	return pHttpConnBuf_.size();
}

workServer::~workServer()
{
	std::lock_guard<std::mutex> lock(mPHttpConnBuf_);
	
	delete[] httpConns_;

	for(auto tmpfd=pHttpConnBuf_.begin();tmpfd!=pHttpConnBuf_.end();)
	{
		close(*tmpfd);
		tmpfd=pHttpConnBuf_.erase(tmpfd);
	}
	if(epollfd_ != -1)
	{
		close(epollfd_);
	}

	delete ptheard_;
	ptheard_ = nullptr;

	delete pThreadPool_;
	pThreadPool_ = nullptr;

	pSqlPool_->destoryPool();
}
