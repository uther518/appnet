
#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include "ae.h"
#include <errno.h>
#include <sys/socket.h>
#include "aeserver.h"

//======================
void initWorkerOnLoopStart( aeEventLoop *l)
{
    //puts("worker Event Loop Init!!! \n");
}

int sendMessageToReactor( int connfd , char* buff , int len )
{
    	aePipeData data = {0};
    	data.type = PIPE_EVENT_MESSAGE;
    	data.connfd = connfd;
    	data.len = len;
	
	if (sdslen( servG->worker->send_buffer ) == 0 && len != 0 )
	{
		aeCreateFileEvent( servG->worker->el,
				servG->worker->pipefd , AE_WRITABLE,
				onWorkerPipeWritable,NULL );
	}
	
	//append header
    	servG->worker->send_buffer = sdscatlen( servG->worker->send_buffer , &data, PIPE_DATA_HEADER_LENG );
	//append data
	servG->worker->send_buffer = sdscatlen( servG->worker->send_buffer ,buff, len );
}

void readWorkerBodyFromPipe( int pipe_fd , aePipeData data )
{
	int pos = PIPE_DATA_HEADER_LENG;
	int readlen = 0;
	int needlen = ( data.len > PIPE_DATA_LENG ) ? PIPE_DATA_LENG : data.len;
	int bodylen = 0;

	if( data.len <= 0 )
	{
	    return;
	}
	
  	memset( data.data ,0,sizeof( data.data )); 
	readlen = read( pipe_fd , data.data  , needlen );
	//printf( "readWorkerBodyFromPipe pipefd=%d, needlen=%d, len=%d , data=%s \n" , pipe_fd, needlen , readlen , data.data  );
	if( servG->onRecv )
	{
		//将真实数据返回，去除包头
		//如果一个请求是多个包发送过来的，会有问题吗
		servG->onRecv( servG , &servG->connlist[data.connfd] , data.data , readlen  );
	}
}


void onWorkerPipeWritable( aeEventLoop *el, int fd, void *privdata, int mask )
{
    ssize_t nwritten;
    nwritten = write( fd, servG->worker->send_buffer, sdslen(servG->worker->send_buffer));
    if (nwritten <= 0) {
        printf( "Worker I/O error writing to worker: %s", strerror(errno));
		//退出吗，自杀..
        return;
    }

	//offset
    sdsrange(servG->worker->send_buffer,nwritten,-1);
	
	///if send_buffer no data need send, remove writable event
    if (sdslen(servG->worker->send_buffer) == 0)
    {
        aeDeleteFileEvent( el, fd, AE_WRITABLE);
    }
	
}

//读取后要写到接收缓冲区吗,不需要。。,
void onWorkerPipeReadable( aeEventLoop *el, int fd, void *privdata, int mask )
{
    aePipeData data;
    int readlen =0;

    //此处如果要把数据读取到大于包长的缓冲区中，不要用anetRead，否则就掉坑里了
    //readlen = anetRead( fd, data, PIPE_DATA_HEADER_LENG  );
    readlen = read(fd, &data , PIPE_DATA_HEADER_LENG );
    if( readlen == 0 )
    {
        close( fd );
    }
    else if( readlen == PIPE_DATA_HEADER_LENG )
    {
    	//printf( "onWorkerPipeReadable len=%d \n" , readlen );
	    //connect,read,close
        if( data.type == PIPE_EVENT_CONNECT )
        {
            if( servG->onConnect )
            {
                servG->onConnect( servG , data.connfd );
            }
        }
        else if( data.type == PIPE_EVENT_MESSAGE )
        {
	    readWorkerBodyFromPipe(  fd ,data );
        }
        else if( data.type == PIPE_EVENT_CLOSE )
        {
            if( servG->onClose )
            {
                servG->onClose( servG , &servG->connlist[data.connfd] );
            }
        }
        else
        {
            printf( "recvFromPipe recv unkown data.type=%d" , data.type );
        }
    }
}

int sendCloseEventToReactor( int connfd  )
{
    aePipeData data;
    data.type = PIPE_EVENT_CLOSE;
    data.connfd = connfd;
    data.len = 0;
    return send2ReactorThread( connfd , data );
}

int socketWrite(int __fd, void *__data, int __len)
{
    int n = 0;
    int written = 0;
    
    while (written < __len)
    {
        n = write(__fd, __data + written, __len - written);
        if (n < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            else if (errno == EAGAIN)
            {
                continue;
            }
            else
            {
                printf("write %d bytes failed.", __len);
                return AE_ERR;
            }
        }
        written += n;
    }
    return written;
}


int send2ReactorThread( int connfd , aePipeData data )
{
	if (sdslen( servG->worker->send_buffer ) == 0 && data.len != 0 )
		aeCreateFileEvent( servG->worker->el,
				servG->worker->pipefd , AE_WRITABLE,
				onWorkerPipeWritable,NULL );
					
    servG->worker->send_buffer = sdscatlen( servG->worker->send_buffer ,&data, PIPE_DATA_HEADER_LENG );
}



int timerCallback(struct aeEventLoop *l,long long id,void *data)
{
    printf("I'm time_cb,here [EventLoop: %p],[id : %lld],[data: %p] \n",l,id,data);
    return 5*1000;
}

void finalCallback(struct aeEventLoop *l,void *data)
{
    puts("call the unknow final function \n");
}

void childTermHandler( int sig )
{
    //printf( "Worker Recv Int Signal...\n");
    aeStop( servG->worker->el );
}

void childChildHandler( int sig )
{
    //printf( "Worker Recv Child Signal...\n");
    pid_t pid;
    int stat;
    while ( ( pid = waitpid( -1, &stat, WNOHANG ) ) > 0 )
    {
        continue;
    }
}

/**
 * process event types:
 * 1,parent process send readable event
 */
void runWorkerProcess( int pidx ,int pipefd )
{
    //每个进程私有的。
    aeWorker* worker = zmalloc( sizeof( aeWorker ));
    worker->pid = getpid();
    worker->maxEvent = 1024;
    worker->pidx = pidx;
    worker->pipefd = pipefd;
    worker->running = 1;
    servG->worker = worker;
    
    //这里要安装信号接收器..
    addSignal( SIGTERM, childTermHandler, 0 );
    addSignal( SIGCHLD, childChildHandler , 1 );
    
    worker->el = aeCreateEventLoop( worker->maxEvent );
    aeSetBeforeSleepProc( worker->el,initWorkerOnLoopStart );
    int res;
    
	worker->send_buffer = sdsempty();

    //监听父进程管道事件
    res = aeCreateFileEvent( worker->el,
                             worker->pipefd,
                             AE_READABLE,
                             onWorkerPipeReadable,NULL
                            );
	printf("Worker Run pid=%d and listen pipefd=%d is ok? [%d]\n",worker->pid,pipefd,res==0 );
	
	
    //定时器
    //res = aeCreateTimeEvent(el,5*1000,timerCallback,NULL,finalCallback);
    //printf("create time event is ok? [%d]\n",!res);
    aeMain(worker->el);
    aeDeleteEventLoop(worker->el);
    close( pipefd );
	
	printf( "Worker pid=%d exit...\n" , worker->pid );
    sdsfree( worker->send_buffer );
	zfree( worker );   
    shm_free( servG->connlist , 0 );
	exit( 0 );
   
}
