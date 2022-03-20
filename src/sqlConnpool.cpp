#include "sqlConnpool.h"

sqlConnPool* sqlConnPool::getInstance()
{
	static sqlConnPool connPool;
	return &connPool;
}

sqlConnPool::sqlConnPool():maxConn_(0),currConn_(0)
{

}

void sqlConnPool::init(std::string url,std::string user,std::string pwd,std::string dbName,int port,int maxConn,int close_log)
{
	url_ = url;
	user_ = user;
	port_ = port;
	pwd_ = pwd;
	dbName_ = dbName;
	close_log_ = close_log;

	for(int i=0;i<maxConn;++i)
	{
		MYSQL* con = nullptr;
		con = mysql_init(con);

		if(!con)
		{
			LOG_ERROR("mysql_init error!\n");
			return ;
		}

		
		/*调试无法连接数据库时用*/
		// MYSQL mysql;
		// mysql_init(&mysql);
		// if(!mysql_real_connect(&mysql,url_.c_str(),user_.c_str(),pwd_.c_str(),dbName_.c_str(),port_,NULL,0))
		// {
		// 	fprintf(stderr, "Failed to connect to database: Error: %s\n",mysql_error(&mysql));
		// 	return ;
		// }
		
		con = mysql_real_connect(con,url_.c_str(),user_.c_str(),pwd_.c_str(),dbName_.c_str(),port_,NULL,0);
		if(!con)
		{
			LOG_ERROR("mysql_real_connect error!\n");
			return ;
		}

		connPool_.push_back(con);
		++freeConn_;
	}

	LOG_INFO("inited mysqlPool !\n");
	mySem_.setCount(freeConn_);

	maxConn_ = freeConn_;
}

MYSQL* sqlConnPool::getConnection()
{
	MYSQL* con = nullptr;
	if(connPool_.empty()) { return nullptr;}

	mySem_.semWait();
	mutex_.lock();

	con = connPool_.front();
	connPool_.pop_front();

	--freeConn_;
	++currConn_;

	mutex_.unlock();
	return con;
}

bool sqlConnPool::releaseConnection(MYSQL* con)
{
	if(!con) { return false;}

	mutex_.lock();

	connPool_.push_back(con);
	++freeConn_;
	--currConn_;

	mutex_.unlock();
	mySem_.semSignal();
	return true;
}

void sqlConnPool::destoryPool()
{
	mutex_.lock();

	if(!connPool_.empty())
	{
		for(auto it=connPool_.begin();it!=connPool_.end();)
		{
			mysql_close(*it);
			it = connPool_.erase(it);
		}
		currConn_ = 0;
		freeConn_ = 0;
		connPool_.clear();
	}

	mutex_.unlock();
}

sqlConnPool::~sqlConnPool()
{
	destoryPool();
}

sqlConnRAII::sqlConnRAII(MYSQL** con,sqlConnPool* connPool)
{
	/*用户传进来 &con 的地址 ,相当于间接寻址 ,白话一点就是传进来的是指针的引用*/
	*con = connPool->getConnection();

	conRAII = *con;	
	poolRAII = connPool;
}
sqlConnRAII::~sqlConnRAII()
{
	poolRAII->releaseConnection(conRAII);
}
