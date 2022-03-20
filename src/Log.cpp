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
	//当前缓冲区容纳不下本次的日志字符串，需要先把原本的日志写到日志文件
	if (nbufLastIdx + unrealThisLogSize >= LOG_BUF_SIZE )
	{
		{
			std::lock_guard<std::mutex> lock(mufile_);
			ctmpLogFileBuf[LOG_BUF_SIZE - 1] = '\0';
			fputs(ctmpLogFileBuf, fplogFile_);
			fflush(fplogFile_);
			nbufLastIdx = 0;
		}
	}
	memcpy(ctmpLogFileBuf + nbufLastIdx, clogBuf_, unrealThisLogSize);
	nbufLastIdx += unrealThisLogSize;

	//这一句使得每一次冲出 都不会重复冲出之前缓冲区的内容 
	ctmpLogFileBuf[nbufLastIdx] = '\0';
}

bool MyLog::LogInit(int maxBlockSize)
{

	memset(cdirName_, '\0', sizeof(cdirName_));
	memset(cfileName_, '\0', sizeof(cfileName_));
	memset(clogBuf_, '\0', sizeof(clogBuf_));
	memcpy(cdirName_, "asd", 255);

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

