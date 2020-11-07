#include <iostream>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sstream>
#include <vector>
#include <hiredis/hiredis.h>
#include <mysql/mysql.h>
 
using namespace std;
 
#define MAXLINE 1024 
#define OPEN_MAX 100
#define LISTENQ 20
#define SERV_PORT 5000
#define INFTIM 1000
 
static redisContext * _context = NULL; 
static MYSQL * _myConn = NULL; 
 
void freeMyConn()
{
	if (NULL != _myConn)
    {
 
		mysql_close(_myConn);
        _myConn = NULL;
	}
}
 
MYSQL* getMyconn()
{
	if (NULL == _myConn)
	{
		const char * myServer = getenv("MYSQL_HOST");
		const char * myUser = getenv("MYSQL_USER");
		const char * myPasswd = getenv("MYSQL_PWD");
		const char * myDB = getenv("MYSQL_DB");
    	
		_myConn = mysql_init(NULL);
    	if (!mysql_real_connect(_myConn, myServer, myUser,  myPasswd, myDB, 0, NULL, 0))
    	{
        	fprintf(stderr, "%s\n", mysql_error(_myConn));
			freeMyConn();	
        	exit(1);
    	}
	}
	
	return _myConn;
} 
 
int string_replase(string &s1, const string &s2, const string &s3)
{	
	string::size_type pos = 0;	
	string::size_type a = s2.size();	
	string::size_type b = s3.size();	
	while ((pos = s1.find(s2,pos)) != string::npos)	
	{		
		s1.replace(pos, a, s3);		
		pos += b;	
	}	
	return 0;
}
 
void split(const std::string &s, std::vector<std::string> &elems, char delim = ' ')
{
    std::stringstream ss;
    ss.str(s);
    std::string item;
    while (std::getline(ss, item, delim)) 
	{
        elems.push_back(item);
    }
}
 
void freeRedisContext()
{
	if (NULL != _context)
    {
		redisFree(_context);
        _context = NULL;
	}
}
 
redisContext* getContext()
{
	if (NULL == _context)
	{
    	const char* r_host = getenv("REDIS_HOST");
    	printf("redis host:%s\n", r_host);
    	_context = redisConnect(r_host, 6379);
    	if (_context->err) 
		{
        	fprintf(stderr, "redis connection faild!\n");
			freeRedisContext();
			exit(1);
		}
    }
 
	return _context;
}
 
int setvalue(const string & key, const string & value)
{
    stringstream ss;
    ss << "SET " << key << " " << value;
    string command;
    getline(ss, command);
	cout << "try redis command:" << command << endl; 
	
    redisReply * reply = (redisReply *) redisCommand(getContext(),  command.c_str());
    if(reply == NULL) 
	{
		cout << "redis command faild! command:" << command << endl;
    	freeReplyObject(reply);
        freeRedisContext();
    	return -1;
    }
 
    if(!((reply->type == REDIS_REPLY_STATUS) && (strcasecmp(reply->str, "OK") == 0)))
	{
		cout << "redis command faild! " << reply->type << ":" <<  reply->str << endl;
    	freeReplyObject(reply);
        freeRedisContext();
        return -1;
    }
 
	freeReplyObject(reply);
   	freeRedisContext();
 
    return 0; 
}
 
int setMyDB(const string & key, const string & value)
{
	char sql[256] = {'\0'};
	sprintf(sql, "select description from user where name = '%s'", key.c_str());
	cout << "try mysql query:" << sql << endl; 
 
    if (0 != mysql_query(getMyconn(), sql))
    {
        fprintf(stderr, "mysql query:%s\n", mysql_error(getMyconn()));
		freeMyConn();
       	return -1; 
    }
 
    MYSQL_RES * res = mysql_store_result(getMyconn());
	if (NULL == res)
	{
        fprintf(stderr, "mysql store result:%s\n", mysql_error(getMyconn()));
		freeMyConn();
       	return -1; 
	}
 
	if (0 == mysql_num_rows(res))
	{
		cout << "insert" << endl;
		// mysql_free_result(res);
		sprintf(sql, "insert into user (name,age,description) values('%s', 11,'%s')", key.c_str(), value.c_str());
    	if (0 != mysql_query(getMyconn(), sql))
		{
			fprintf(stderr, "mysql query:%s\n", mysql_error(getMyconn()));
			freeMyConn();
			return -1; 
		}
	}
	else
	{
		cout << "update" << endl;
		// mysql_free_result(res);
		sprintf(sql, "update user set description = '%s' where name = '%s'", value.c_str(), key.c_str());
    	if (0 != mysql_query(getMyconn(), sql))
		{
			fprintf(stderr, "mysql query:%s\n", mysql_error(getMyconn()));
			freeMyConn();
			return -1; 
		}
 
	}
 
    mysql_free_result(res);
    freeMyConn();
	return 0;
}
 
int getMyDB(const string & key, string & value)
{
    stringstream ss;
	ss << "select description from user where name = '" << key << "'";
    string query;
    getline(ss, query);
	cout << "try mysql query:" << query << endl; 
 
    if (0 != mysql_query(getMyconn(), query.c_str()))
    {
        fprintf(stderr, "%s\n", mysql_error(getMyconn()));
		freeMyConn();
       	return -1; 
    }
 
    MYSQL_RES * res = mysql_use_result(getMyconn());
	MYSQL_ROW row; 
	int i = 0;
    while ((row = mysql_fetch_row(res)) != NULL)
    {
//        printf("%s \n", row[0]);
		value += row[i++];
    }
 
    mysql_free_result(res);
    freeMyConn();
	return 0;
}
 
int getvalue(const string & key, string & value)
{
    stringstream ss;
    ss << "GET " << key;
    string command;
    getline(ss, command);
	cout << "try redis command:" << command << endl; 
 
    redisReply * reply = (redisReply *) redisCommand(getContext(),  command.c_str());
    if(NULL == reply) 
	{
		cout << "redis command faild! command:" << command << endl;
    	freeReplyObject(reply);
        freeRedisContext();
    	return -1;
    }
 
    if(reply->type == REDIS_REPLY_STRING)
    {
        value = reply->str;
	}
 
    freeReplyObject(reply);
	freeRedisContext(); 
    return 0; 
}
 
void setnonblocking(int sock)//将套接字设置为非阻塞
{
    int opts;
    opts=fcntl(sock,F_GETFL);
    if(opts<0)
    {
        perror("fcntl(sock,GETFL)");
        exit(1);
    }
    opts = opts|O_NONBLOCK;
    if(fcntl(sock,F_SETFL,opts)<0)
    {
        perror("fcntl(sock,SETFL,opts)");
        exit(1);
    }
}
 
/* usage:
set cindy 18age;living in Guilin;favorite in cookie
get cindy
*/
int onReciveMsg(const string & msg, string & reply)
{
	char buf[MAXLINE] = {'\0'};
	vector<string> strs;
    split(msg, strs);
	if (strs.empty()) 
	{
		return 0;
	}
 
	if (0 == strcasecmp(strs[0].c_str(), "GET") && 2 == strs.size())
	{
		// find data from redis cache first
		if (0 != getvalue(strs[1], reply))
        {
        	cout << "get redis value failed!" << endl;
			return -1;
        }
 
		// data not exist in redis cache
		if (0 == strlen(reply.c_str()))
		{
			// find data from mysql db
			if (0 != getMyDB(strs[1], reply))
			{
				cout << "query mysql db failed!" << endl;
				return -1;
			}
 
			// cache to redis cache if data exist in mysql db
			if (strlen(reply.c_str()) > 0)
			{
				string_replase(reply, " ", "_");
				if (0 != setvalue(strs[1], reply))
				{
					cout << "set redis value failed!" << endl;
					return -1; 
				}
			}
			else
			{
				sprintf(buf, "no more info about [%s]", strs[1].c_str());
				cout << buf << endl;
				reply = buf;
			}
		}
	} 	
	else if (0 == strcasecmp(strs[0].c_str(), "SET"))
	{
		string value;
		int i =0;
		for(vector<string>::iterator it = strs.begin(); it != strs.end(); it++)
		{
			if (i++ < 2) continue;	// 0-set 1-key 2more-value
			value += *it + "_";
		}
		
		if (0 != setvalue(strs[1], value))
        {
        	cout << "set redis value failed!" << endl;
			return -1;
        }
 
		if (0 != setMyDB(strs[1], value))
        {
        	cout << "set mysql db value failed!" << endl;
			return -1;
		}
	}
	
	return 0; 
}
 
int main(int argc, char* argv[])
{
    int i, maxi, listenfd, connfd, sockfd,epfd,nfds, portnumber;
    ssize_t n;
    char rcvbuf[MAXLINE];
    char sndbuf[MAXLINE];
    socklen_t clilen;
    if ( 2 == argc )
    {
        if( (portnumber = atoi(argv[1])) < 0 )
        {
            fprintf(stderr,"Usage:%s port:number\a\n",argv[0]);
            return 1;
        }
    }
    else
    {
        fprintf(stderr,"Usage:%s port:number\a\n",argv[0]);
        return 1;
    }
 
    struct epoll_event ev,events[20]; 	//声明epoll_event结构体的变量,ev用于注册事件,数组用于回传要处理的事件
    epfd=epoll_create(256); 			//生成用于处理accept的epoll专用的文件描述符
    struct sockaddr_in clientaddr;
    struct sockaddr_in serveraddr;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    setnonblocking(listenfd); 			//把socket设置为非阻塞方式
    ev.data.fd=listenfd; 				//设置与要处理的事件相关的文件描述符
    ev.events=EPOLLIN|EPOLLET;  		//设置要处理的事件类型    
 
    epoll_ctl(epfd,EPOLL_CTL_ADD,listenfd,&ev); //注册epoll事件
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    const char *local_addr = "127.0.0.1";
    inet_aton(local_addr, &(serveraddr.sin_addr)); 
    serveraddr.sin_port = htons(portnumber);
    bind(listenfd,(sockaddr *)&serveraddr, sizeof(serveraddr));
    listen(listenfd, LISTENQ);
    maxi = 0;
	bzero(rcvbuf, sizeof(rcvbuf));
	bzero(sndbuf, sizeof(sndbuf));
    for ( ; ; ) 
	{
        nfds = epoll_wait(epfd,events,20,500); 		//等待epoll事件的发生
        for(i = 0; i < nfds; ++i) 					//处理所发生的所有事件
        {
            if(events[i].data.fd == listenfd)		//如果新监测到一个SOCKET用户连接到了绑定的SOCKET端口，建立新的连接。
            {
                cout << "new connection arrival" << endl;
                connfd = accept(listenfd,(sockaddr *)&clientaddr, &clilen);
                if (connfd < 0) 
				{
                    perror("accept");
                    exit(1);
                }
                char *str = inet_ntoa(clientaddr.sin_addr);
                cout << "accapt a connection from " << str << endl;
				cout << "--------------------------------" << endl;
                ev.data.fd = connfd; 				//设置用于读操作的文件描述符
                ev.events = EPOLLIN|EPOLLET; 		//设置用于注测的读操作事件
                epoll_ctl(epfd, EPOLL_CTL_ADD, connfd,&ev); //注册ev
            }
            else if(events[i].events & EPOLLIN)		//如果是已经连接的用户，并且收到数据，那么进行读入。
            {
                if ( (sockfd = events[i].data.fd) < 0) continue;
 
    			bzero(rcvbuf, sizeof(rcvbuf));
                if ( (n = read(sockfd, rcvbuf, MAXLINE)) < 0) 
				{
                    if (errno == ECONNRESET) 
					{
                        close(sockfd);
                        events[i].data.fd = -1;
                    }
					else
                        cout << "readline error" << endl;
                } 
				else if (n == 0) 
				{
                    close(sockfd);
                    events[i].data.fd = -1;
                }
                // rcvbuf[n] = '\0'; 
                // cout << "read:" << rcvbuf << endl;
 
				// handle redis
				string msg(rcvbuf);
				string reply;
				onReciveMsg(msg, reply);
				sprintf(sndbuf, "%s\n", reply.c_str());
 
                ev.data.fd = sockfd;  			//设置用于写操作的文件描述符
                ev.events = EPOLLOUT|EPOLLET; 	//设置用于注测的写操作事件
                epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev); //修改sockfd上要处理的事件为EPOLLOUT 
            }
            else if(events[i].events&EPOLLOUT) // 如果有数据发送
            {
				if (strlen(sndbuf) > 0)
				{ 
                	sockfd = events[i].data.fd;
                	write(sockfd, sndbuf, strlen(sndbuf));
                	ev.data.fd = sockfd; //设置用于读操作的文件描述符
                	ev.events = EPOLLIN|EPOLLET; //设置用于注测的读操作事件
                	epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);  //修改sockfd上要处理的事件为EPOLIN
				}
            }
        }
    }
    return 0;
}