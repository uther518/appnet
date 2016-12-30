#ifndef __APPNET_SERVER_H__
#define __APPNET_SERVER_H__

#include "appnet_event.h"
#include "appnet_dict.h"
#include "appnet_request.h"
#include "appnet_response.h"
#include "appnet_sds.h"
#include "share_memory.h"
#include "appnet_websocket.h"
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#define AE_TRUE 1
#define AE_FALSE 0

#define OPT_WORKER_NUM "opt_worker_num"
#define OPT_TASK_WORKER_NUM "opt_task_worker_num"
#define OPT_REACTOR_NUM "opt_reactor_num"
#define OPT_MAX_CONNECTION "opt_max_connection"
#define OPT_PROTOCOL_TYPE "opt_protocol_type"
#define OPT_DAEMON "opt_daemon"

#define APPNET_HTTP_DOCS_ROOT "http_docs_root"
#define APPNET_HTTP_404_PAGE "http_404_page"
#define APPNET_HTTP_50X_PAGE "http_50x_page"
#define APPNET_HTTP_UPLOAD_DIR "http_upload_dir"
#define APPNET_HTTP_KEEP_ALIVE "http_keep_alive"

#define DEFAULT_HTTP_DOCS_ROOT "/home/www"
#define DEFALUT_HTTP_404_PAGE "/404.html"
#define DEFALUT_HTTP_50X_PAGE "/50x.html"

#define MAXFD 1024
#define MAX_EVENT 4096
#define WORKER_PROCESS_COUNT 3
#define REACTOR_THREAD_COUNT 2

#define TMP_BUFFER_LENGTH 65535
#define RECV_BUFFER_LENGTH 65535
#define SEND_BUFFER_LENGTH 65535

#define SOCKET_SND_BUF_SIZE 1024 * 1024
#define SOCKET_RCV_BUF_SIZE 1024 * 1024

#define __SLEEP_WAIT__ usleep(10000)
struct aeEventLoop;

typedef struct
{
    char disable;
    char proto_type;
    uint16_t listen_port;
    uint16_t listen_fd;
    char *client_ip;
    uint16_t client_port;
    uint16_t worker_id;
    uint8_t reactor_id;
    sds send_buffer;
    sds recv_buffer;
    int connfd;
    char wshs; //websocket handleshake
} appnetConnection;

typedef struct _appnetServer appnetServer;
typedef struct _reactorThreadParam reactorThreadParam;

typedef struct
{
    int epfd;
    int id;
    int event_num;
    int max_event_num;
    int running :1;
    void *object;
    void *ptr;
    aeEventLoop *event_loop;
} appnetReactor;

typedef struct
{
    pid_t pid;
    int pipefd[2];
    pthread_mutex_t w_mutex;
    pthread_mutex_t r_mutex;
    sds send_buffer; /* worker send to pipe */
    sds recv_buffer; /* worker recv from pipe */
} appnetWorkerProcess;

typedef struct _appnetWorkerPipes
{
    int pipefd[2];
} appnetWorkerPipes;

typedef struct
{
    char proto; /* current request protocol type */
    int pidx; /* process index */
    pid_t pid;
    int pipefd;
    int running;
    int max_event;
    int connfd;
    int start;
    int task_id;
    aeEventLoop *el;
    sds send_buffer; /* master send to pipe */
    sds recv_buffer; /* master recv from pipe */
    sds response;
    dict *resp_header;
    httpHeader req_header;
} appnetWorker;

typedef struct
{
    pthread_t thread_id;
    appnetReactor reactor;
    reactorThreadParam *param;
    handshake *hs;
    httpHeader *hh; /* http/websocket header */
} appnetReactorThread;

typedef enum
{
    PIPE_EVENT_CONNECT = 1,
    PIPE_EVENT_MESSAGE,
    PIPE_EVENT_CLOSE,
    PIPE_EVENT_TASK
} PipeEventType;

typedef enum
{
    LISTEN_TYPE_TCP = 0,
    LISTEN_TYPE_HTTP,
    LISTEN_TYPE_WS,
    LISTEN_TYPE_HTTPS,
    LISTEN_TYPE_MAX
} ListenType;

struct _appnetServer
{
    char *listen_ip;
    uint16_t listenfd;
    uint16_t port;
    uint16_t listen_fds[4];
    uint16_t ports[4];
    uint8_t running;
    uint8_t daemon;
    void *ptr2;
    uint16_t reactor_num;
    uint16_t worker_num;
    uint16_t task_worker_num;
    uint32_t connect_max;
    uint32_t connect_num;
    uint8_t exit_code;
    uint8_t proto_type;
    uint8_t http_keep_alive;
    char http_404_page[64];
    char http_50x_page[64];
    char http_docs_root[255];
    char http_upload_dir[255];
    appnetReactor *main_reactor;
    appnetConnection *connlist;
    appnetReactorThread *reactor_threads;
    appnetWorkerProcess *workers;
    appnetWorkerPipes *worker_pipes;
    appnetWorker *worker;
    int sigPipefd[2];
    int (*sendToClient)( int fd, char *data, int len );
    void (*closeClient)( appnetConnection *conn );
    int (*send)( int fd, char *data, int len );
    int (*close)( int fd );
    int (*setOption)( char *key, char *val );
    int (*setHeader)( char *key, char *val );
    void (*onConnect)( appnetServer *serv, int fd );
    void (*onRecv)( appnetServer *serv, appnetConnection *c, char *buff, int len );
    void (*onTask)( char *data, int len, int id, int from );
    void (*onTaskCallback)( char *data, int len, int id, int from );
    void (*onClose)( appnetServer *serv, appnetConnection *c );
    void (*onStart)( appnetServer *serv );
    void (*onFinal)( appnetServer *serv );
    void (*runForever)( appnetServer *serv );
};

struct _reactorThreadParam
{
    int thid;
    appnetServer *serv;
};

/*receive status*/
#define CONTINUE_RECV 1
#define BREAK_RECV 2
#define CLOSE_CONNECT 3

#pragma pack(1)
typedef struct
{
    char type;
    char proto;
    int header_len;
    int len;
    int connfd;
    sds data;
} appnetPipeData;
/*define pipe data header length*/
#define PIPE_DATA_HEADER_LENG 2+3*sizeof(int)

typedef struct
{
    int id;
    int to;
    int from;
} asyncTask;

void initOnLoopStart( struct aeEventLoop *el );
void initThreadOnLoopStart( struct aeEventLoop *el );
void onSignEvent( aeEventLoop *el, int fd, void *privdata, int mask );
void freeClient( appnetConnection *c );
void onCloseByClient( aeEventLoop *el, void *privdata, appnetServer *serv, appnetConnection *conn );
void onClientWritable( aeEventLoop *el, int fd, void *privdata, int mask );
void onClientReadable( aeEventLoop *el, int fd, void *privdata, int mask );
int setPipeWritable( aeEventLoop *el, void *privdata, int worker_id );
/*accept connection from client*/
void acceptCommonHandler( appnetServer *serv, int listenfd, int connfd, char *client_ip, int client_port );
void onAcceptEvent( aeEventLoop *el, int fd, void *privdata, int mask );
void runMainReactor( appnetServer *serv );
void masterSignalHandler( int sig );
void addSignal( int sig, void (*handler)( int ), int restart );
void installMasterSignal( appnetServer *serv );
appnetServer *aeServerCreate( char *ip, int port );
void createReactorThreads( appnetServer *serv );
appnetReactorThread getReactorThread( appnetServer *serv, int i );
void readBodyFromPipe( aeEventLoop *el, int fd, appnetPipeData data );
void onMasterPipeReadable( aeEventLoop *el, int fd, void *privdata, int mask );
void onMasterPipeWritable( aeEventLoop *el, int pipe_fd, void *privdata, int mask );
void *reactorThreadRun( void *arg );
int socketSetBufferSize( int fd, int buffer_size );
void createWorkerProcess( appnetServer *serv );
void stopReactorThread( appnetServer *serv );
void freeWorkerBuffer( appnetServer *serv );
int freeConnectBuffers( appnetServer *serv );
void destroyServer( appnetServer *serv );
int startServer( appnetServer *serv );
void initWorkerOnLoopStart( aeEventLoop *l );
int sendMessageToReactor( int connfd, char *buff, int len );
void readWorkerBodyFromPipe( int pipe_fd, appnetPipeData data );
void onWorkerPipeWritable( aeEventLoop *el, int fd, void *privdata, int mask );
void onWorkerPipeReadable( aeEventLoop *el, int fd, void *privdata, int mask );
int sendCloseEventToReactor( int connfd );
int socketWrite( int __fd, void *__data, int __len );
int send2ReactorThread( int connfd, appnetPipeData data );
int timerCallback( struct aeEventLoop *l, long long id, void *data );
void finalCallback( struct aeEventLoop *l, void *data );
void childTermHandler( int sig );
void childChildHandler( int sig );
void runWorkerProcess( int pidx );
void appendWorkerData( int connfd, char *buffer, int len, int event_type, char *from );
void appendHttpData( int connfd, char *header, int header_len, char *body, int body_len, int event_type, char *from );
aeEventLoop *getThreadEventLoop( int connfd );
int setHeader( char *key, char *val );
int setOption( char *key, char *val );
void timerAdd( int ms, void *cb, void *params );
void resetRespHeader( dict *resp_header );
sds getRespHeaderString( sds header );
int setRespHeader( char *key, char *val );
void appendToClientSendBuffer( int connfd, char *buffer, int len );
int getPipeIndex( int connfd );

appnetServer *servG;
#endif
