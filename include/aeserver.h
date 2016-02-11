
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


#define AE_TRUE        1
#define AE_FALSE       0

#define MAXFD 1024
#define WORKER_PROCESS_COUNT 3
#define REACTOR_THREAD_COUNT 2

#define TMP_BUFFER_LENGTH  65535
#define RECV_BUFFER_LENGTH  65535
#define SEND_BUFFER_LENGTH  65535

#define __SLEEP_WAIT__	usleep( 10000 )
struct aeEventLoop;

typedef struct _aeConnection
{
  int flags;
  int fd;
  char disable; 
  char* client_ip;
  int client_port;
  handshake hs; //websocket握手数据
  httpHeader hh; //http/websocket header
  sds send_buffer; //发送给客户端的缓冲区
  sds recv_buffer; //接收缓冲区
  
//  aeEventLoop *el;
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
    void *ptr;  //reserve
	aeEventLoop *eventLoop;
};

typedef struct _aeWorkerProcess
{
    pid_t pid;
    int pipefd[2];
	//这里是自旋锁好，还是mutex好
	pthread_mutex_t w_mutex;
	pthread_mutex_t r_mutex;
	sds send_buffer;
	
}aeWorkerProcess;

typedef struct _aeWorker
{
	int pidx; //主进程中分的编号0-x
	pid_t pid;
	int pipefd;
	int running;
	int maxEvent;
        int connfd;
        int start;
	aeEventLoop *el;
	sds send_buffer;
	sds recv_buffer;

}aeWorker;

typedef struct _aeReactorThread
{
    pthread_t thread_id;
    aeReactor reactor;
    reactorThreadParam* param;
    //swLock lock;
} aeReactorThread;

typedef enum
{
	PIPE_EVENT_CONNECT = 1,
	PIPE_EVENT_MESSAGE,
	PIPE_EVENT_CLOSE,
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
   int listenfd;
   int   port;
   int   running;
   void *ptr2;
   int reactorNum;
   int workerNum;
   int maxConnect;
   int connectNum;
   int protocolType;
   
   aeReactor* mainReactor;
   aeConnection* connlist;
   aeReactorThread *reactorThreads;
   //pthread_barrier_t barrier;
   aeWorkerProcess *workers; //主进程中保存的worker相关信息数组。
   aeWorker* worker;	//子进程中的全局变量,子进程是独立空间，所以只要一个标识当前进程
   int sigPipefd[2];
   //reactor->client
   int  (*sendToClient)(  int fd, char* data , int len );
   void (*closeClient)( aeConnection *c  );
   //worker->reactor
   int  (*send)(  int fd, char* data , int len );
   int  (*close)( int fd );
   
   void (*onConnect)( aeServer* serv ,int fd );
   void (*onRecv)( aeServer *serv, aeConnection* c , char* buff , int len );
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

#define PIPE_DATA_LENG 1024
#define PIPE_DATA_HEADER_LENG 1+2*sizeof(int)


#define  CONTINUE_RECV 1
#define  BREAK_RECV  2
#define  CLOSE_CONNECT  3



#pragma pack(1)
typedef struct _aePipeData
{
	char type;
	int len;
	int connfd;
	char data[PIPE_DATA_LENG];
}aePipeData;


void initOnLoopStart(struct aeEventLoop *el);
void initThreadOnLoopStart( struct aeEventLoop *el );
void onSignEvent( aeEventLoop *el, int fd, void *privdata, int mask);
void freeClient( aeConnection* c  );
void onCloseByClient(  aeEventLoop *el, void *privdata ,
		aeServer* serv , aeConnection* conn  );
void onClientWritable( aeEventLoop *el, int fd, void *privdata, int mask );
void onClientReadable( aeEventLoop *el, int fd, void *privdata, int mask);
void setPipeWritable( aeEventLoop *el , void *privdata ,  int worker_id  );
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
void runWorkerProcess( int pidx ,int pipefd );
void createWorkerTask(  int connfd , char* buffer , int len , int eventType , char* from );
aeEventLoop* getThreadEventLoop( int connfd );

void timerAdd( int ms , void* cb , void* params  );

aeServer*  servG;
#endif
