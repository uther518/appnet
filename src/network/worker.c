
	#include <stdio.h>
	#include <stddef.h>
	#include <time.h>
	#include <assert.h>
	#include <unistd.h>
	#include "ae.h"
	#include <errno.h>
	#include <sys/socket.h>
	#include "aeserver.h"
	#include "dict.h"
	//======================
	void initWorkerOnLoopStart( aeEventLoop *l)
	{
	     if( servG->worker->start == 0 )
	     {
		servG->onStart( servG );
		servG->worker->start = 1;
	     }
	}

	unsigned int dictSdsCaseHash(const void *key) {
	    return dictGenCaseHashFunction((unsigned char*)key, sdslen((char*)key));
	}

	int dictSdsKeyCaseCompare(void *privdata, const void *key1,
		const void *key2)
	{
	    DICT_NOTUSED(privdata);
	    return strcasecmp(key1, key2) == 0;
	}

	void dictSdsDestructor(void *privdata, void *val)
	{
	    DICT_NOTUSED(privdata);
	    sdsfree(val);
	}

	dictType httpTableDictType = {
	    dictSdsCaseHash,           /* hash function */
	    NULL,                      /* key dup */
	    NULL,                      /* val dup */
	    dictSdsKeyCaseCompare,     /* key compare */
	    dictSdsDestructor,         /* key destructor */
	    dictSdsDestructor,         /* val destructor */
	};

	int createResponse( int connfd , char* buff , int len , char prototype , char* response )
        {
		if( prototype == HTTP )
		{
			strcpy ( response , "HTTP/1.1 200 OK \r\n" );
			strcat ( response , "Server: appnet/1.0.0\r\n" );
			strcat ( response , "Content-Type: text/html\r\n" );
			strcat ( response , "\r\n" );
			strcat ( response ,  buff );
			return strlen( response );
		}
		else
		{
			int outlen = 0;
			wsMakeFrame( buff ,  len , response , &outlen , WS_TEXT_FRAME );	
			//WS_CLOSING_FRAME
		//	wsMakeFrame( buff ,  len , response , &outlen , WS_CLOSING_FRAME  );
	        //	printf( "wsMakeFrame len=%d,buff=%s \n" , outlen , response );
			return outlen;
		}
	}


	void timerAdd( int ms , void* cb , void* params  )
	{
		aeCreateTimeEvent( servG->worker->el, ms , cb  , params  , NULL );
	}


	void timerRemove( int tid  )
        {
                aeDeleteTimeEvent( servG->worker->el, tid );
        }

	//如果不是tcp协议，要单独处理
	int sendMessageToReactor( int connfd , char* buff , int len )
	{
		aePipeData data = {0};
		data.type = PIPE_EVENT_MESSAGE;
		data.connfd = connfd;
		if (sdslen( servG->worker->send_buffer ) == 0  )
		{
			aeCreateFileEvent( servG->worker->el,
					servG->worker->pipefd , AE_WRITABLE,
					onWorkerPipeWritable,NULL );
		}
		

		if( servG->connlist[connfd].protoType != TCP )
		{
			char prototype = servG->connlist[connfd].protoType;
			data.len = createResponse(  connfd , buff , len , prototype , &data.data  );		
		}
		else
		{
			//append header
			data.len = len;
		}
		
		servG->worker->send_buffer = sdscatlen( servG->worker->send_buffer , &data, PIPE_DATA_HEADER_LENG );
		//append data
		if( len >= 0 )
		{
			if( servG->connlist[connfd].protoType == TCP  )
			{
				servG->worker->send_buffer = sdscatlen( servG->worker->send_buffer ,buff, len );
			}
			else
			{
				servG->worker->send_buffer = sdscatlen( servG->worker->send_buffer , data.data , data.len );
			}
		}

		return data.len;
	}



	void callUserRecvFunc( int connfd , char* buffer , int length )
	{
		if( servG->onRecv )
		{
			//将真实数据返回，去除包头
			//如果一个请求是多个包发送过来的，会有问题吗
			servG->onRecv( servG , &servG->connlist[connfd] , buffer , length  );
		}
	}

	void readWorkerBodyFromPipe( int pipe_fd , aePipeData data )
	{
		int pos = PIPE_DATA_HEADER_LENG;
		int readlen = 0;
		int needlen = ( data.len > PIPE_DATA_LENG ) ? PIPE_DATA_LENG : data.len;
		int bodylen = 0;

		if( data.len > 0 )
		{
			memset( data.data ,0,sizeof( data.data )); 
			readlen = read( pipe_fd , data.data  , needlen );
		}
		callUserRecvFunc( data.connfd , data.data , readlen );
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
		servG->worker->connfd = data.connfd;
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
			printf( "onClose\n");
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
	    
	    char prototype = servG->connlist[connfd].protoType;
	    if( prototype == WEBSOCKET )
	    {   
		int outlen = 0;
		char* response;
		char* reason = "normal_close";
                wsMakeFrame( reason , strlen( reason ) , &data.data , &outlen , WS_CLOSING_FRAME ); 
	   	data.len = outlen;
	    }
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
		if (sdslen( servG->worker->send_buffer ) == 0  )
		{
			aeCreateFileEvent( servG->worker->el,
					servG->worker->pipefd , AE_WRITABLE,
					onWorkerPipeWritable,NULL );
		}
		else
		{
			printf( "send_buffer=%d,len=%d\n", sdslen( servG->worker->send_buffer ) , data.len  );
		}				
		servG->worker->send_buffer = sdscatlen( servG->worker->send_buffer ,&data, PIPE_DATA_HEADER_LENG );
	}



	int timerCallback(struct aeEventLoop *l,long long id,void *data)
	{
	    printf("I'm time_cb,here [EventLoop: %p],[id : %lld],[data: %s] \n",l,id,data);
	    return 1;
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
	    worker->start = 0;
	    servG->worker = worker;
	    
	    //这里要安装信号接收器..
	    addSignal( SIGTERM, childTermHandler, 0 );
	    addSignal( SIGCHLD, childChildHandler , 1 );
	    
	    worker->el = aeCreateEventLoop( worker->maxEvent );
	    aeSetBeforeSleepProc( worker->el,initWorkerOnLoopStart );
	    int res;
	    
	    worker->send_buffer = sdsempty();
	    worker->recv_buffer = sdsempty();
    //监听父进程管道事件
    res = aeCreateFileEvent( worker->el,
                             worker->pipefd,
                             AE_READABLE,
                             onWorkerPipeReadable,NULL
                            );
    printf("Worker Run pid=%d and listen pipefd=%d is ok? [%d]\n",worker->pid,pipefd,res==0 );
	
    //定时器
    //res = aeCreateTimeEvent( worker->el, 10 , onWorkerStart , NULL , NULL );
  
    aeMain(worker->el);
    aeDeleteEventLoop(worker->el);
    servG->onFinal( servG );
    
    printf( "Worker pid=%d exit...\n" , worker->pid );
    sdsfree( worker->send_buffer );
    sdsfree( worker->recv_buffer );
    zfree( worker );   
    shm_free( servG->connlist , 0 );
    close( pipefd );
    exit( 0 );
   
}
