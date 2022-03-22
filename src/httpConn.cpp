#include "httpConn.h"

/*一些信息头*/
const char* ok_200_title = "HTTP/1.1 200 OK\r\n";
const char* error_400_title = "HTTP/1.1 400 Bad Request\r\n";
const char* error_400_info = "Your request has bad syntax\r\n";
const char* error_403_title = "HTTP/1.1 403 Forbidden\r\n";
const char* error_403_info = "You do not have permission to get file from this server\r\n";
const char* error_404_title = "HTTP/1.1 404 Not Found\r\n";
const char* error_404_info = "The requestd file was not found\r\n";
const char* error_500_title = "HTTP/1.1 500 Internal Error\r\n";
const char* error_500_info = "There was an unusual problem in server\r\n";
const char* linger_keep_alive = "Connection: keep-alive\r\n";
const char* linger_closed = "Connection: close\r\n";
const char* c_content_length = "Content-Length: ";
const char* c_html_format = "Content-Type: text/html\r\n";
const char* c_download_format = "Content-Type: application/octet-stream\r\n";
const char* dc_download_disposiotion="Content-Disposition: attachment; filename=";
const char* c_blank_row = "\r\n";
char* c_index_html = "index.html";

void httpConn::removefd()
{
	epoll_ctl(epollfd_,EPOLL_CTL_DEL,httpfd_,0);
    close(httpfd_);
}

void httpConn::modfd()
{
	epoll_event event;
	event.data.fd = httpfd_;
	event.events = EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLONESHOT;
	epoll_ctl(epollfd_,EPOLL_CTL_MOD,httpfd_,&event);
}

httpConn::httpConn(int fd):httpfd_(fd)
{
	/*获取连接池单例类*/
	sqlPool_ = sqlConnPool::getInstance();	

    init();
}

void httpConn::init()
{
    nlastReadIndex_ = 0;
    nRowStart_ = 0;
    nRowEnd_ = 0;
    readData_ = 0;
    nLastWriteIndex_ = 0;

    check_state_ = CHECK_STATE_REQUESTLINE;
    method_ = GET;

    url_ = 0;
    version_ = 0;
    host_ = 0;
    contentLength_ = 0;
    linger_ = false;

    memset(recvBuf_,'\0',RECV_BUF_SIZE);
    memset(sendBuf_,'\0',SEND_BUF_SIZE);
}

/*解析行内容*/
LINE_STATUS httpConn::parse_line()
{
    char* cRowStart=recvBuf_+nRowEnd_;
    char* cRowEnd=strstr(cRowStart,"\r\n");
    if(!cRowEnd)
    {
        return LINE_CONTINUE;
    }
    *cRowEnd='\0';
    ++cRowEnd;
    *cRowEnd='\0';
    ++cRowEnd;
	//std::cout <<cRowStart<<'\n';
    nRowStart_ =  nRowEnd_;
    nRowEnd_ = cRowEnd-recvBuf_;

    return LINE_COMPLETE;
}

bool httpConn::read()
{
    if(nlastReadIndex_ >= RECV_BUF_SIZE)
    {
        return false;
    }
    while(true)
    {
        readData_ = recv(httpfd_,recvBuf_+nlastReadIndex_,RECV_BUF_SIZE-nlastReadIndex_,0);
        if(readData_==-1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK) {break;}
            else
            {
                LOG_ERROR("HttpResolver recv <%d>client errno :%d \n",httpfd_,errno);
            	removefd();
                httpfd_ = -1;
                return false;
            }
        }
        else if(readData_==0)
        {
        	removefd();
            httpfd_ = -1;
            return false;
        }
        nlastReadIndex_ += readData_;
    }
    return true;
}

/*分析请求行*/
HTTP_CODE httpConn::parse_requestLine()
{
    char* requestLine = recvBuf_+nRowStart_;
    //std::cout <<"rqLIne:"<<requestLine<<'\n';

    char* method = strstr(requestLine,"GET");
    if(!method)
    {
    	method = strstr(requestLine,"POST");
    	if(!method)
    	{
    		//printf("Receive a undefine method!\n");
            LOG_WARNING("Receive a undefine method!");
        	return BAD_REQUEST;
    	}
        method_ = POST ;
        LOG_INFO("Receive a POST method!")
        //printf("Receive a http request:\nMethod:POST\n");
    }
    else
    {
        method_ = GET;
        LOG_INFO("Receive a GET method!")
    }

    char* version = strstr(requestLine,"HTTP/1.1");
    if(!version)
    {
        LOG_WARNING("Receive other HTTP version!");
        //printf("Receive other HTTP version!\n");
        return BAD_REQUEST;
    }
    else
    {
        --version;
        *version = '\0'; 
        version_ = "HTTP/1.1";
        //printf("Version:HTTP/1.1\n");
    }

    char* url = strstr(requestLine,"/");
    if(!url)
    {
        //printf("Receive empty url\n");
        return BAD_REQUEST;
    }
    else
    {
        url_ = url;
        /*首次访问 跳转登录页面*/
        if(strlen(url_)==1)
        {
            url_ = "/login.html";
        }
        //printf("Url: %s (%d)\n",url_,strlen(url_));
    }

    /*请求行处理完毕,将状态设为处理请求头*/
    check_state_=CHECK_STATE_REQUESTHEADER;
    return INCOMPLETE_REQUEST;
    //return GET_REQUEST;
}

/*分析请求头*/
HTTP_CODE httpConn::parse_requestHeader()
{
    char* requestHeader = recvBuf_+nRowStart_;
    //std::cout << "header:"<<requestHeader<<'\n';

    char* tmpInfo=nullptr;
    /*读到空行,代表消息头部解析完毕*/
    if(requestHeader[0]=='\0')
    {
        /*若contentLength不为0,代表有该大小的消息体 或者 请求为POST 状态机转移状态到CONTENT*/
        if(contentLength_ !=0 || method_ == POST)
        {
            //printf("status = Content\n");
            check_state_ = CHECK_STATE_CONTENT;
            return INCOMPLETE_REQUEST;
        }
        /*否则已经督导一个完整的HTTP请求*/
        return GET_REQUEST;
    }
    /*处理Connection字段*/
    else if( (tmpInfo=strstr(requestHeader,"Connection:") )!=nullptr)
    {
        if(strstr(tmpInfo,"keep-alive")!=nullptr || strstr(tmpInfo,"Keep-Alive")!=nullptr)
        {
            linger_ = true;
            //printf("Connection: keep-laive: true\n");
        }
        else
        {
            linger_ = false;
            //printf("Connection: keep-alive:false\n");
        }
    }
    /*处理Content-Length字段*/
    else if( (tmpInfo=strstr(requestHeader,"Content-Length:") )!=nullptr)
    {
        tmpInfo += 15;
        tmpInfo += strspn(tmpInfo," \t");
        contentLength_ = atol(tmpInfo);
        //std::cout << "Content-Length:" << contentLength_<<'\n';
        //printf("Content-Length: %d\n:"contentLength_);
    }
    /*处理Host字段*/
    else if( (tmpInfo=strstr(requestHeader,"Host:") )!=nullptr)
    {
        tmpInfo += 5;
        tmpInfo += strspn(tmpInfo," \t");
        host_ = tmpInfo;
        //printf("Host: %s\n",tmpInfo);
    }
    else
    {
        //printf("Unknow header info : :%s!\n",requestHeader);
    }
    return INCOMPLETE_REQUEST;
}

HTTP_CODE httpConn::parse_content()
{
    //printf("Do not process content!\n");
    nRowStart_ = nRowEnd_ ; 
    char* tmpContent =recvBuf_+nRowStart_;
    nRowEnd_ = strlen(tmpContent);
    //TODO:说明还未收到一个完整的报文
    // if(nRowEnd_ < contentLength_ )
    // {
    //     return INCOMPLETE_REQUEST;
    // }
    /*post请求*/
    if(method_ == POST)
    {
        //printf("GET HTTP_POST content: %s\n",tmpContent);

    	/*目前仅有登录注册功能*/
    	/*注册*/
    	MYSQL* tmpSqlConn = nullptr;
    	sqlConnRAII(&tmpSqlConn,sqlPool_);
        if(!tmpSqlConn)
        {
            LOG_ERROR("Mysql conn error!\n");
        }

        /*提取post正文中的 name 以及 pwd*/
        char* name = strstr(tmpContent,"name=");
	    name+=5;
	    char* pwd = strstr(tmpContent,"&pwd=");
	    *pwd = '\0';
	    pwd+=5;
	    std::string s_name = name;
	    std::string s_pwd = pwd;

	    /*查询数据库有无该用户*/
	    std::string sqlSelect = "SELECT passwd from accounts where name = '" + s_name + "'";
        int ret = mysql_query(tmpSqlConn,sqlSelect.c_str());
        if(ret!=0)
        {
        	LOG_ERROR("sql select error!\n");
        	/*返回服务器错误码*/
       		return INTERNAL_ERRNO;
       	}
       	MYSQL_RES* resultSet = mysql_store_result(tmpSqlConn);
       	if(resultSet)
        {
        	int name2pwd = mysql_num_rows(resultSet);

        	/*当前是注册页面*/
        	if(strstr(url_,"register"))
	        {
	        	/*原用户已经注册*/
		        if(name2pwd != 0) 
		        {
		        	/*设置即将跳转的url*/
		        	url_ = "/repeatRegister.html";
                    LOG_WARNING("had regsistered!");
		        	//printf("had registered!\n");
                    return GET_REQUEST;
		        }
		    	std::string sqlInsert = "INSERT into accounts(name,passwd)values('" + s_name +"','"+s_pwd+"')";
		    	int ret = mysql_query(tmpSqlConn,sqlInsert.c_str());
		    	if(ret!=0)
		    	{
		    		//printf("sql insert error \n");
                    LOG_ERROR("SQL Insert Error");
		    		/*返回服务器错误码*/
		    		return INTERNAL_ERRNO;
		    	}
		    	url_ = "/registerSucceed.html";
                LOG_INFO("register !");
		    	return GET_REQUEST;
	        }
	        /*当前是登录页面*/
	        else if(strstr(url_,"login"))
	        {
	        	/*该用户还未注册*/
	       		if(name2pwd == 0)
	        	{
	        		//url_ = "=notRegister";
                    LOG_WARNING("not register login !\n");
	        		return GET_REQUEST;
	        	}
	        	else
	        	{
	        		MYSQL_ROW row = mysql_fetch_row(resultSet);
	        		std::string store_pwd = row[0];
	        		/*登录成功*/
	        		if(store_pwd == s_pwd)
	        		{
                        //printf("login !\n");
	        			url_ = "/index.html";
	        		}
	        		/*密码错误*/
	        		else
	        		{
                        //printf("login bad!\n");
	        			url_ = "/loginError.html";
	        		}
	        		return GET_REQUEST;
	        	}	
	        }
	        /*服务器不具备客户端访问的页面*/
	        else
	        {
	        	return NO_RESOURCE;
	        }		
        }	
    }
    /*GET请求 的正文内容只是打印出来就行*/
    else 
    {
    	//printf("GET HTTP_GET content: %s\n",tmpContent);
    }

    //printf("content: %s\n",tmpContent);
    return GET_REQUEST;
}

/*分析入口*/
HTTP_CODE httpConn::process_read()
{
    LINE_STATUS line_status = LINE_COMPLETE;
    HTTP_CODE ret = INCOMPLETE_REQUEST;
    /*完整的行*/
    while( ( (check_state_ == CHECK_STATE_CONTENT)&&(line_status == LINE_COMPLETE) ) 
            || (line_status=parse_line() ) == LINE_COMPLETE )
    {
        switch(check_state_)
        {
            /*请求行*/
            case CHECK_STATE_REQUESTLINE:
            {
                ret= parse_requestLine();
                if(ret==BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                break;
            }
            /*请求头*/
            case CHECK_STATE_REQUESTHEADER:
            {
                ret=parse_requestHeader();
                if(ret==GET_REQUEST)
                {
                    return GET_REQUEST;
                }
                else if(ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_CONTENT:
            {
                ret = parse_content();
                if(ret == GET_REQUEST)
                {
                    return GET_REQUEST;
                }
                line_status = LINE_CONTINUE;
                break;
            }
            default:
            {
                LOG_WARNING("Undefined CHECK_STATE!");
                return INTERNAL_ERRNO;
            }
        }
    }
    if(line_status==LINE_CONTINUE)
    {
        return INCOMPLETE_REQUEST;
    }
    else 
    {
        return BAD_REQUEST;
    }
}


bool httpConn::responseByRet(HTTP_CODE ret)
{
    int filefd = -1, fileLength = -1;
    switch(ret)
    {
        /*主状态机状态出错*/
        case INTERNAL_ERRNO:
        {
            std::string sinfo=c_content_length+std::to_string(strlen(error_500_info))+c_blank_row;
            strcat(sendBuf_,error_500_title);
            strcat(sendBuf_,c_html_format);
            strcat(sendBuf_,sinfo.c_str());
            if(linger_) strcat(sendBuf_,linger_keep_alive);
            else strcat(sendBuf_,linger_closed);
            strcat(sendBuf_,c_blank_row);
            strcat(sendBuf_,error_500_info);
            break;
        }
        /*客户端错误*/
        case BAD_REQUEST:
        {
            std::string sinfo=c_content_length+std::to_string(strlen(error_400_info))+c_blank_row;
            strcat(sendBuf_,error_400_title);
            strcat(sendBuf_,c_html_format);
            strcat(sendBuf_,sinfo.c_str());
            if(linger_) strcat(sendBuf_,linger_keep_alive);
            else strcat(sendBuf_,linger_closed);
            strcat(sendBuf_,c_blank_row);
            strcat(sendBuf_,error_400_info);
            break;
        }
        /*客户端请求的文件不存在*/
        case NO_RESOURCE:
        {
            std::string sinfo=c_content_length+std::to_string(strlen(error_404_info))+c_blank_row;
            strcat(sendBuf_,error_404_title);
            strcat(sendBuf_,c_html_format);
            strcat(sendBuf_,sinfo.c_str());
            if(linger_) strcat(sendBuf_,linger_keep_alive);
            else strcat(sendBuf_,linger_closed);
            strcat(sendBuf_,c_blank_row);
            strcat(sendBuf_,error_404_info);
            break;
        }
        /*无权访问该文件*/
        case FORBIDDEN_REQUEST:
        {
            std::string sinfo=c_content_length+std::to_string(strlen(error_403_info))+c_blank_row;
            strcat(sendBuf_,error_403_title);
            strcat(sendBuf_,c_html_format);
            strcat(sendBuf_,sinfo.c_str());
            if(linger_) strcat(sendBuf_,linger_keep_alive);
            else strcat(sendBuf_,linger_closed);
            strcat(sendBuf_,c_blank_row);
            strcat(sendBuf_,error_403_info);
            break;
        }
        case GET_REQUEST:
        {
            strcat(sendBuf_,ok_200_title);
            std::string s_fileName = "";
            ++url_;
            s_fileName = s_fileName + url_;
            if(strstr(url_,"html"))
            {
                strcat(sendBuf_,c_html_format);
            }
            else
            {
                strcat(sendBuf_,c_download_format);
                std::string strDownload_disposiotion = dc_download_disposiotion+s_fileName+c_blank_row;
                strcat(sendBuf_,strDownload_disposiotion.c_str());
            }    
            if(s_fileName!="favicon.ico" && s_fileName.size() >= 1)
            {
                filefd = open(url_,O_RDONLY);
                if(filefd <= 0)
                {
                    return responseByRet(NO_RESOURCE);
                }
                struct stat stat_buf;
                fstat(filefd,&stat_buf); 

                fileLength = stat_buf.st_size;
                if(fileLength <=0 )
                {
                    return fileLength==0?responseByRet(NO_RESOURCE):responseByRet(FORBIDDEN_REQUEST);
                }

                std::string sinfo=c_content_length + std::to_string(fileLength) + c_blank_row;
                strcat(sendBuf_,sinfo.c_str());

                // int fileLength = addFile(s_fileName.c_str());
                // if(fileLength <=0 )
                // {
                //     return fileLength==0?responseByRet(NO_RESOURCE):responseByRet(FORBIDDEN_REQUEST);
                // }
                // std::string sinfo=c_content_length+std::to_string(fileLength)+c_blank_row;
                //strcat(sendBuf_,sinfo.c_str());
            }

            if(linger_) strcat(sendBuf_,linger_keep_alive);
            else strcat(sendBuf_,linger_closed);
            strcat(sendBuf_,c_blank_row);
            
            break;
        }
        default:
        {
            return false;
        }
    }
    int sndret=send(httpfd_,sendBuf_,strlen(sendBuf_),0);
    if(sndret <= 0)
    {
        LOG_ERROR("HttpResolver data:%d send errno:%d\n",sndret,errno);
        return false;
    }
    if(filefd != -1)
    {
        sendfile(httpfd_,filefd,NULL,fileLength);
        close(filefd);
    }
    //printf("Response Succeed!\n ");
    return true;
}

/*测试函数*/
bool httpConn::testAPI(int epollfd,int fd)
{
    httpfd_ = fd;
    init();
    epollfd_ = epollfd;
    bool flag = false;
    if(read())
    {
        HTTP_CODE ret= process_read();
        //keep-alive && 响应成功，不断连接
        //if(responseByRet(ret))
        if(responseByRet(ret) && linger_) 
        {
        	modfd();
        }
        //其余情况，断开连接且移除在epoll的监听
        else 
        {
        	removefd();
            httpfd_ = -1;
        }
    }
    init();
    return flag;
}

httpConn::~httpConn()
{
    if(httpfd_!=-1)
    {
        close(httpfd_);
    }
}
