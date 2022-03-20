#include "httpServer.h"
#include "Log.h"
void addsig(int sig,void(handler)(int),bool restart=true)
{
	struct sigaction sa;
	memset(&sa,'\0',sizeof(sa));
	sa.sa_handler = handler;
	if(restart)
	{
		sa.sa_flags |= SA_RESTART;
	}
	sigfillset(&sa.sa_mask);
	if(sigaction(sig,&sa,NULL)==-1)
	{
		printf("sigaction error : %d!",errno);
	}
}

int main()
{
	addsig(SIGPIPE,SIG_IGN);
	MyLog::GetLogInstance()->LogInit();
	LOG_INFO("Starting...");
	httpServer* pTomdog = new httpServer();
	pTomdog->startServer();
	delete pTomdog;
		
	return 0;
}
