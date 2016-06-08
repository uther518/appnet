
#ifndef __AE_SERVER_H__
#define __AE_SERVER_H__
#include <time.h>
#include <signal.h> 
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "ae.h"
#include "share_memory.h"
#include "sds.h"
#include "dict.h"
#include "http_request.h"
#include "websocket.h"
#include "http_response.h"

#define AE_TRUE        1
#define AE_FALSE       0

#define OPT_WORKER_NUM  "opt_worker_num"
#define OPT_ATASK_WORKER_NUM "opt_atask_worker_num"
#define OPT_REACTOR_NUM "opt_reactor_num"
#define OPT_MAX_CONNECTION "opt_max_connection"
#define OPT_PROTOCOL_TYPE  "opt_protocol_type"

#define APPNET_HTTP_DOCS_ROOT  "http_docs_root"
#define APPNET_HTTP_404_PAGE   "http_404_page"
#define APPNET_HTTP_50X_PAGE   "http_50x_page"
#define APPNET_HTTP_UPLOAD_DIR  "http_upload_dir"
#define APPNET_HTTP_KEEP_ALIVE  "http_keep_alive"

#define DEFAULT_HTTP_DOCS_ROOT "/home/www"
#define DEFALUT_HTTP_404_PAGE "/404.html"
#define DEFALUT_HTTP_50X_PAGE "/50x.html"

#define MAXFD 1024
#define MAX_EVENT 4096
#define WORKER_PROCESS_COUNT 3
#define REACTOR_THREAD_COUNT 2

#define TMP_BUFFER_LENGTH  65535
#define RECV_BUFFER_LENGTH  65535
#define SEND_BUFFER_LENGTH  65535

#define SOCKET_SND_BUF_SIZE 1024*1024
#define SOCKET_RCV_BUF_SIZE 1024*1024

#define __SLEEP_WAIT__	usleep( 10000 )
struct aeEventLoop;

typedef struct _aeConnection
{
  int flags;
  int fd;
  char disable; 
  char* client_ip;
  int client_port;
  sds send_buffer; //send to client
  sds recv_buffer; //recv from client
  char protoType;
}aeConnection;

typedef struct _aeServer aeServer;
typedef struct _aeReactor aeReactor;
typedef struct _reactorThreadParam reactorThreadParam;

//reactor结构体
struct _aeReactor
{
	int epfd;
	int id;
	int event_num;
	int max_event_num;
	int running :1;
	void *object;
    	void *ptr;
	aeEventLoop *eventLoop;
	
};

//主进程中的worker数组
//这个数组被N个线程共享访问
typedef struct _aeWorkerProcess
{
    pid_t pid;
    int pipefd[2];
    
    //这里是自旋锁好，还是mutex好
    pthread_mutex_t w_mutex;
    pthread_mutex_t r_mutex;
    sds send_buffer;	//send to worker pipe
    sds recv_buffer;    //recv from worker pipe
	
}aeWorkerProcess;

typedef struct _aeWorkerPipes
{
    int pipefd[2];
}aeWorkerPipes;

typedef struct _aeWorker
{
        char proto; //current request protocol type
	int pidx; //主进程中分的编号0-x
	pid_t pid;
	int pipefd;
	int running;
	int maxEvent;
	int connfd;
	int start;
	int task_id;
	aeEventLoop *el;
	sds send_buffer;	//send to master pipe
	sds recv_buffer;	//recv from master pipe
	sds response;		//response
	dict* resp_header;
	httpHeader req_header;
}aeWorker;

//reactor thread info..
typedef struct _aeReactorThread
{
    pthread_t thread_id;
    aeReactor reactor;
    reactorThreadParam* param;
    handshake* hs; //websocket握手数据
    httpHeader* hh; //http/websocket header
} aeReactorThread;

typedef enum
{
	PIPE_EVENT_CONNECT = 1,
	PIPE_EVENT_MESSAGE,
	PIPE_EVENT_CLOSE,
	PIPE_EVENT_ATASK
}PipeEventType;	


typedef enum
{
	PROTOCOL_TYPE_TCP_ONLY = 0,
	PROTOCOL_TYPE_HTTP_ONLY,
	PROTOCOL_TYPE_WEBSOCKET_ONLY,
	PROTOCOL_TYPE_HTTP_MIX,
	PROTOCOL_TYPE_WEBSOCKET_MIX,
}ProtocolType;	

struct _aeServer
{
   char* listen_ip;
   int   listenfd;
   int   port;
   int   running;
   void *ptr2;
   int reactorNum;
   int workerNum;
   int ataskWorkerNum;
   int maxConnect;
   int connectNum;
   int exit_code;
   int protocolType;
   int  http_keep_alive;
   char http_404_page[64];
   char http_50x_page[64];
   char http_docs_root[255];
   char http_upload_dir[255];
 
   aeReactor* mainReactor;
   aeConnection* connlist;
   aeReactorThread *reactorThreads;
   //pthread_barrier_t barrier;
   aeWorkerProcess *workers; //处理客户端请求
   aeWorkerPipes *worker_pipes;//每个worker对应各线程均有一个pipe

   aeWorker* worker;	//子进程中的全局变量,子进程是独立空间，所以只要一个标识当前进程
   int sigPipefd[2];
   //reactor->client
   int  (*sendToClient)(  int fd, char* data , int len );
   void (*closeClient)( aeConnection *c  );
   //worker->reactor
   int  (*send)(  int fd, char* data , int len );
   int  (*close)( int fd );
   int  (*setOption)( char* key , char* val );
   int  (*setHeader)( char* key , char* val );
   
   void (*onConnect)( aeServer* serv ,int fd );
   void (*onRecv)( aeServer *serv, aeConnection* c , char* buff , int len );
   void (*onTask)( char* data , int len , int id , int from );
   void (*onTaskCallback)( char* data , int len , int id , int from );
   void (*onClose)( aeServer *serv , aeConnection *c );
   void (*onStart)( aeServer *serv  );
   void (*onFinal)( aeServer *serv  );

   void (*runForever )( aeServer* serv );
};

struct _reactorThreadParam
{
	int thid;
	aeServer* serv;
};

#define PIPE_DATA_LENG 90
#define PIPE_DATA_HEADER_LENG 2+3*sizeof(int)

#define  CONTINUE_RECV 1
#define  BREAK_RECV  2
#define  CLOSE_CONNECT  3

#pragma pack(1)
typedef struct _aePipeData
{
	char type;
	char proto;
	int header_len;
	int len;
	int connfd;
	sds data;
}aePipeData;


typedef struct
{
	int id;
	int to;
	int from;
}asyncTask;


void initOnLoopStart(struct aeEventLoop *el);
void initThreadOnLoopStart( struct aeEventLoop *el );
void onSignEvent( aeEventLoop *el, int fd, void *privdata, int mask);
void freeClient( aeConnection* c  );
void onCloseByClient(  aeEventLoop *el, void *privdata ,
		aeServer* serv , aeConnection* conn  );
void onClientWritable( aeEventLoop *el, int fd, void *privdata, int mask );
void onClientReadable( aeEventLoop *el, int fd, void *privdata, int mask);
int setPipeWritable( aeEventLoop *el , void *privdata ,  int worker_id  );
void acceptCommonHandler( aeServer* serv ,int fd,
		char* client_ip,int client_port, int flags);
void onAcceptEvent( aeEventLoop *el, int fd, void *privdata, int mask);
void runMainReactor( aeServer* serv );
void masterSignalHandler( int sig );
void addSignal( int sig, void(*handler)(int), int restart  );
void installMasterSignal( aeServer* serv );
aeServer* aeServerCreate( char* ip,int port );
void createReactorThreads( aeServer* serv  );
aeReactorThread getReactorThread( aeServer* serv, int i );
void readBodyFromPipe(  aeEventLoop *el, int fd , aePipeData data );
void onMasterPipeReadable( aeEventLoop *el, int fd, void *privdata, int mask );
void onMasterPipeWritable(  aeEventLoop *el, int pipe_fd, void *privdata, int mask );
void *reactorThreadRun(void *arg);
int socketSetBufferSize(int fd, int buffer_size);
void createWorkerProcess( aeServer* serv );
void stopReactorThread( aeServer* serv  );
void freeWorkerBuffer( aeServer* serv );
int freeConnectBuffers( aeServer* serv );
void destroyServer( aeServer* serv );
int startServer( aeServer* serv );
void initWorkerOnLoopStart( aeEventLoop *l);
int sendMessageToReactor( int connfd , char* buff , int len );
void readWorkerBodyFromPipe( int pipe_fd , aePipeData data );
void onWorkerPipeWritable( aeEventLoop *el, int fd, void *privdata, int mask );
void onWorkerPipeReadable( aeEventLoop *el, int fd, void *privdata, int mask );
int sendCloseEventToReactor( int connfd  );
int socketWrite(int __fd, void *__data, int __len);
int send2ReactorThread( int connfd , aePipeData data );
int timerCallback(struct aeEventLoop *l,long long id,void *data);
void finalCallback(struct aeEventLoop *l,void *data);
void childTermHandler( int sig );
void childChildHandler( int sig );
void runWorkerProcess( int pidx );
void createWorkerTask(  int connfd , char* buffer , int len , int eventType , char* from );
void createHttpTask(  int connfd , char* header ,  int header_len , char* body,  int body_len , int eventType , char* from );
aeEventLoop* getThreadEventLoop( int connfd );
int setHeader( char* key , char* val );
int setOption( char* key , char* val );
void timerAdd( int ms , void* cb , void* params  );
void resetRespHeader( dict* resp_header );
sds getRespHeaderString( sds header );
int setRespHeader( char* key , char* val );

aeServer*  servG;
int getPipeIndex( int connfd );

#endif
