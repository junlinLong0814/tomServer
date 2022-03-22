#include "httpServer.h"



httpServer::httpServer(int port):port_(port),listenfd_(-1),clientfd_(-1),epollfd_(-1)
{
	bzero(&address_,sizeof(address_));
	bzero(&client_address_,sizeof(client_address_));
}

int httpServer::run()
{
	bzero(&address_,sizeof(address_));
	address_.sin_family = AF_INET;
	address_.sin_port = htons(port_);
	address_.sin_addr.s_addr = htonl(INADDR_ANY);

	listenfd_ = socket(PF_INET,SOCK_STREAM,0);
	if(listenfd_<=0)
	{
		LOG_ERROR("Open listenfd errno : %d\n",errno);
		return -1;
	}
	struct linger tmpLinger = {1,0};
	setsockopt(listenfd_,SOL_SOCKET,SO_LINGER,&tmpLinger,sizeof(tmpLinger));

	int ret=bind(listenfd_,(struct sockaddr*)&address_,sizeof(address_));
	if(ret == -1)
	{
		LOG_ERROR("Bind errno : %d\n",errno);
		return -1;
	}

	ret=listen(listenfd_,SOMAXCONN);
	if(ret==-1)
	{
		LOG_ERROR("Listen errno : %d\n",errno);
		return -1;
	}
	LOG_INFO("Port:%d,Listening...\n",port_);
	return 0;
}

#define WORKER_NUM 1
int httpServer::startWorker()
{
	for(int i=0;i<WORKER_NUM;++i)
	{
		workServer* worker = new workServer();
		workers_.push_back(worker);
		worker->startWorkerServer();
	}
	return 0;
}

void httpServer::startServer()
{
	int ret = run();
	if(ret != 0)
	{
		return ;
	}

	ret = startWorker();
	if(ret != 0 )
	{
		return ;
	}
	epollfd_ = epoll_create(MAX_EVENT_NUMBER);
	if(epollfd_==-1)
	{
		LOG_ERROR("Epollfd errno : %d\n",errno);
		return ;
	}

	addfd(listenfd_,false);

	while(1)
	{
		int acceptClient = epoll_wait(epollfd_,events_,MAX_EVENT_NUMBER,-1);
		if(acceptClient == -1)
		{
			LOG_ERROR("Epoll_wait(accept) errno : %d\n",errno);
			break;
		}

		client_addrlength_ = sizeof(client_address_);
		clientfd_ = accept(listenfd_,(struct sockaddr*)&client_address_,&client_addrlength_);
		if(clientfd_<0)
		{
			LOG_ERROR("Clientfd assigned errno : %d\n",errno);
			break;
		}
		LOG_INFO("%s connected...\n",inet_ntoa(client_address_.sin_addr));
		//std::shared_ptr<Client> pClient = std::make_shared<Client>(clientfd_);	
		
		assignClient(clientfd_);

	}

	close(epollfd_);
	epollfd_ = -1;
	close(listenfd_);
	listenfd_ = -1;
}

void httpServer::assignClient(int fd)
{
	if(workers_.size()>0)
	{
		auto idleWorker = workers_[0];
		for(auto worker : workers_)
		{
			if(worker->load()<idleWorker->load())
			{
				idleWorker = worker;
			}
		}
		idleWorker->addClient(fd);
	}
}

int httpServer::setNonBlock(int fd)
{
	int old_option = fcntl(fd,F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd,F_SETFL,new_option);
	return old_option;
}

void httpServer::addfd(int fd,bool et_enable)
{
	epoll_event event;
	event.data.fd=fd;
	event.events = EPOLLIN;
	if(et_enable)
	{
		event.events |= EPOLLET;
	}
	epoll_ctl(epollfd_,EPOLL_CTL_ADD,fd,&event);
	setNonBlock(fd);
}

httpServer::~httpServer()
{
	if(epollfd_ != -1)
	{
		close(epollfd_);
	}
	if(listenfd_ != -1)
	{
		close(listenfd_);
	}
	for(auto iter=workers_.begin();iter!=workers_.end();++iter)
	{
		delete (*iter);
		(*iter) = nullptr;
	}
}
