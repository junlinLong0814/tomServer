#ifndef _LOGGING_H_
#define _LOGGING_H_

#include <chrono>
#include <string.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <mutex>

#ifndef LOG_BUF_SIZE
#define LOG_BUF_SIZE 100		
#endif 

class MyLog
{
public:
	static MyLog* GetLogInstance()
	{
		static MyLog myLog;
		return &myLog;
	}

	/*日志初始化*/
	bool LogInit(int maxBlockSize = 1);

	/*写日志操作*/
	template<class Level, class... Args>
	bool WriteLog(Level level, Args... args);

	/*不定参构成日志字符串*/
	template<class Level, class... Args>
	std::string AddString(Level level, Args... args);

	std::string AddString();
private:
	/*将一条日志语句加到缓冲区*/
	void AppendBuf();

	MyLog();
	~MyLog();

private:
	char 											ctmpLogFileBuf[LOG_BUF_SIZE];	//待写入日志文件的缓冲区
	char 											cdirName_[256];					//路径名
	char 											cfileName_[256];				//Log文件名
	char 											clogBuf_[256];					//Log日志内容
	
	std::mutex 										mufile_;						//互斥访问文件指针		
	std::mutex 										mlogBuf;						//互斥访问clogBuf
	std::mutex 										mlogFileBuf;					//互斥访问ctmpLogFileBuf	
	
	int 											ntoday_;						//记录天数
	int 											nmonth_;						//记录月份
	int 											nyear_;							//记录年份
	int 											nbufLastIdx;					//指向缓冲区结尾位置
	FILE 											*fplogFile_;					//指向日志文件指针
	
	//bool 										bcanWriteFile;								//指示线程是否可以进行文件的写
	//std::shared_ptr<std::thread>	pWriteLogThread;					//异步写入日志文件
	//std::condition_variable 			cv;												//唤醒线程写文件
};

template<class Level, class... Args>
std::string MyLog::AddString(Level level, Args... args)
{
	std::stringstream ss;
	ss << level;
	std::string slog = ss.str();
	slog += AddString(args...);
	return slog;
}

template<class Level, class... Args>
bool MyLog::WriteLog(Level level, Args... args)
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

	{
		std::lock_guard<std::mutex> lock(mlogBuf);
		memset(clogBuf_, '\0', sizeof(clogBuf_));
		std::string tmplogBuf = AddString(level, args...);
		snprintf(clogBuf_, 255, "%04d-%02d-%02d  %02d:%02d:%02d  %s", nyear, nmonth, ntoday, tm.tm_hour, tm.tm_min, tm.tm_sec, tmplogBuf.c_str());
		AppendBuf();
	}


	return true;
}


#define LOG_DEBUG(...) {MyLog::GetLogInstance()->WriteLog("[Debug]:", ##__VA_ARGS__);}
#define LOG_INFO(...) {MyLog::GetLogInstance()->WriteLog("[Info]:", ##__VA_ARGS__);}
#define LOG_WARNING(...) {MyLog::GetLogInstance()->WriteLog("[Warning]:", ##__VA_ARGS__);}
#define LOG_ERROR(...) {MyLog::GetLogInstance()->WriteLog("[Error]:", ##__VA_ARGS__);}

#endif