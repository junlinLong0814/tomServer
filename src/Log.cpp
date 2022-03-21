#include "Log.h"

MyLog::MyLog() :fplogFile_(nullptr), nbufLastIdx(0)
{
}

MyLog::~MyLog()
{
	if (fplogFile_)
	{
		//如果退出时缓冲区还有内容 冲出日志文件
		if (nbufLastIdx != 0)
		{
			fputs(ctmpLogFileBuf, fplogFile_);
			fflush(fplogFile_);
		}
		fclose(fplogFile_);
		fplogFile_ = nullptr;
	}
}

std::string MyLog::AddString()
{
	return "\n";
}

void MyLog::AppendBuf()
{
	unsigned int unrealThisLogSize = strlen(clogBuf_) ;

	std::lock_guard<std::mutex> lock(mlogFileBuf);
	if(unrealThisLogSize < sizeof(ctmpLogFileBuf) && unrealThisLogSize >0 )
	{
		{
			std::lock_guard<std::mutex> lock(mufile_);
			memcpy(ctmpLogFileBuf,clogBuf_,sizeof(ctmpLogFileBuf));
			ctmpLogFileBuf[unrealThisLogSize + 1] = '\0';
			fputs(ctmpLogFileBuf, fplogFile_);
			fflush(fplogFile_);
			nbufLastIdx = 0;
		}
	}

	//这一句使得每一次冲出 都不会重复冲出之前缓冲区的内容 
	ctmpLogFileBuf[nbufLastIdx] = '\0';
	//printf("ctmpLogFileBuf:%s\n",ctmpLogFileBuf);
}

bool MyLog::LogInit(int maxBlockSize)
{

	memset(cdirName_, '\0', sizeof(cdirName_));
	memset(cfileName_, '\0', sizeof(cfileName_));
	memset(clogBuf_, '\0', sizeof(clogBuf_));
	memcpy(cdirName_, "server_", 255);

	memset(ctmpLogFileBuf, '\0', sizeof(ctmpLogFileBuf));

	time_t tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	struct tm &tm = *localtime(&tt);
	int nyear = (tm.tm_year + 1900);
	int nmonth = (tm.tm_mon + 1);
	int nday = (tm.tm_mday);
	snprintf(cfileName_, 255, "%s%d_%02d_%02d%s", cdirName_, nyear, nmonth, nday, ".log");
	fplogFile_ = fopen(cfileName_, "a");

	if (!fplogFile_)
	{
		return false;
	}

	nyear_ = nyear;
	nmonth_ = nmonth;
	ntoday_ = nday;
	return true;

}

bool MyLog::newWriteLog(const char *fmt, ...)
{
	time_t tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	struct tm &tm = *localtime(&tt);

	int ntoday = (tm.tm_mday);
	int nmonth = (tm.tm_mon + 1);
	int nyear = (tm.tm_year + 1900);

	if (ntoday != ntoday_
		|| nmonth != nmonth_
		|| nyear != nyear_)
	{
		std::lock_guard<std::mutex> lock(mufile_);
		ntoday_ = ntoday;
		nmonth_ = nmonth;
		nyear_ = nyear;
		memset(cfileName_, '\0', sizeof(cfileName_));
		snprintf(cfileName_, 255, "%s%d_%02d_%02d%s", cdirName_, nyear, nmonth, ntoday, ".log");
		fplogFile_ = fopen(cfileName_, "a");
		if (!fplogFile_)
		{
			return false;
		}
	}

	char s[256];
	memset(s,'\0',sizeof(s));
	va_list ap;
    int n=0;
    va_start(ap,fmt);
    n=vsnprintf(s,sizeof(s),fmt,ap);
    va_end(ap);
	std::string tmp = s;
	{
		std::lock_guard<std::mutex> lock(mlogBuf);
		memset(clogBuf_, '\0', sizeof(clogBuf_));
		snprintf(clogBuf_,sizeof(clogBuf_)-1, "%04d-%02d-%02d  %02d:%02d:%02d  %s\n", nyear, nmonth, ntoday, tm.tm_hour, tm.tm_min, tm.tm_sec, tmp.c_str());
		AppendBuf();
	}

    return n;
}

