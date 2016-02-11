
/*************************************************************************
 多线程的网络IO，多进程的任务处理服务器。
 
 1,reactor->worker   reactor pipe[0] fd   pthread_create
 2,worker->reactor   worker  pipe[1] fd   worker_create
 3,reactor->client   master  connfd       
 
**********************************************************************/


#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include "../include/ae.h"
#include "../include/anet.h"
#include <errno.h>
#include <sys/socket.h>
#include "../include/aeserver.h"

#include <unistd.h>


void initOnLoopStart(struct aeEventLoop *el)
{
    //printf("initOnLoopStart threadid=%d \n" , pthread_self() );
}

void initThreadOnLoopStart( struct aeEventLoop *el )
{
    //printf("initThreadOnLoopStart threadid=%d \n" , pthread_self() );
}

//异步信号事件
void onSignEvent( aeEventLoop *el, int fd, void *privdata, int mask)
{
    if( fd == servG->sigPipefd[0] )
    {
        int sig,ret,i;
        char signals[1024];
        ret = recv( servG->sigPipefd[0], signals, sizeof( signals ), 0 );
        if( ret == -1 )
        {
            return;
        }
       else if( ret == 0 )
        {
            return;
        }
        else
        {
            for(  i = 0; i < ret; ++i )
            {
                switch( signals[i] )
                {
                    //child process stoped
                    case SIGCHLD:
                    {
                        printf( "Master recv child process stoped signal\n");
                        pid_t pid;
                        int stat,pidx;
                        //WNOHANG
                        while ( ( pid = waitpid( -1, &stat, 0 ) ) > 0 )
                        {
                            for( pidx = 0; pidx < servG->workerNum; pidx++ )
                            {
                                if( servG->workers[pidx].pid == pid )
                                {
                                    //aeDeleteFileEvent( aEvBase.el,aEvBase.worker_process[i].pipefd[0] ,AE_READABLE );
                                    close( servG->workers[pidx].pipefd[0] );
                                    servG->workers[pidx].pid = -1;
                                }
                            }
                        }
                        servG->running = 0;
                        //停止主循环
                        aeStop( servG->mainReactor->eventLoop );
                        
                        break;
                    }
                    case SIGTERM:
                    case SIGQUIT:
                    case SIGINT:
                    {
                        int i;
                        printf( "master kill all the clild now\n" );
                        for( i = 0; i < servG->workerNum; i++ )
                        {
                            int pid = servG->workers[i].pid;
                            //alarm/kill send signal to process,
                            //in child process need install catch signal functions;
                            kill( pid, SIGTERM );
                        }
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
            }
        }
    }
}

//关闭连接，并移除监听事件;
void freeClient( aeConnection* c  )
{
    if (c->fd != -1)
    {
        c->disable = 1;
	//zfree( c->hs );
        int reactor_id = c->fd%servG->reactorNum;
        aeEventLoop* el = servG->reactorThreads[reactor_id].reactor.eventLoop;
        aeDeleteFileEvent( el ,c->fd,AE_READABLE);
        aeDeleteFileEvent( el,c->fd,AE_WRITABLE);
	sdsfree( c->send_buffer );
	sdsfree( c->recv_buffer );	
	servG->connectNum -= 1;
        close(c->fd);
    }
    //zfree(c);
}


void onCloseByClient(  aeEventLoop *el, void *privdata , aeServer* serv , aeConnection* conn  )
{
    if( conn == NULL )
    {
        return;
    }
    aePipeData  data = {0};
    data.type = PIPE_EVENT_CLOSE;
    data.connfd = conn->fd;
    data.len = 0;    
    int worker_id = conn->fd % servG->workerNum;
 
    setPipeWritable( el , privdata , worker_id );	
    pthread_mutex_lock( &servG->workers[worker_id].w_mutex );
    servG->workers[worker_id].send_buffer = sdscatlen( servG->workers[worker_id].send_buffer, &data, PIPE_DATA_HEADER_LENG );
    pthread_mutex_unlock( &servG->workers[worker_id].w_mutex );

    //如果这里施放了，子进程要用共享内存，就没法用了	
    freeClient( conn );
}

//send to client,if send over , delete writable event
void onClientWritable( aeEventLoop *el, int fd, void *privdata, int mask )
{
    ssize_t nwritten;
	if( servG->connlist[fd].disable == 1 )
	{
		return;
	}

	if( sdslen( servG->connlist[fd].send_buffer ) <=0 )
	{
		aeDeleteFileEvent( el, fd , AE_WRITABLE);
		return;
	}

	nwritten = write( fd, servG->connlist[fd].send_buffer, sdslen(servG->connlist[fd].send_buffer));
	if (nwritten <= 0)
	{
		printf( "I/O error writing to client: %s", strerror(errno));
		freeClient( &servG->connlist[fd] );
		return;
	}
	
	//offset
       sdsrange(servG->connlist[fd].send_buffer,nwritten,-1);
  //     sdsclear( servG->connlist[fd].send_buffer );	
	///if send_buffer no data need send, remove writable event
    if (sdslen(servG->connlist[fd].send_buffer) == 0)
    {
        aeDeleteFileEvent( el, fd, AE_WRITABLE);
	if( servG->connlist[fd].disable == 2 || servG->connlist[fd].protoType == HTTP )
	{
		freeClient( &servG->connlist[fd] );
	}
    }
}


void createWorkerTask(  int connfd , char* buffer , int len , int eventType , char* from )
{
	aePipeData  data = {0};
	unsigned int datalen;
	
	data.len = len;
	data.type = eventType;
        data.connfd = connfd;
        if( len > 0 )
	{	
		memcpy( data.data , buffer , len  );
	}
	datalen = PIPE_DATA_HEADER_LENG + data.len;
	
	//获取当前线程的el事件循环句柄
	aeEventLoop* reactor_el = getThreadEventLoop( connfd );
	int worker_id  = connfd % servG->workerNum;
	
	setPipeWritable( reactor_el , worker_id , worker_id  );
	//append
	pthread_mutex_lock( &servG->workers[worker_id].w_mutex );
	servG->workers[worker_id].send_buffer = sdscatlen( servG->workers[worker_id].send_buffer , &data, datalen );
	pthread_mutex_unlock( &servG->workers[worker_id].w_mutex );
	
}


//解析包头，是否需要循环recv,
//http,websocket不完整，才需要继续收，其它直接发送给客户端
//http,websocket 将包体解析出来，只把body发送给客户端
//tcp直接投递给客户端
int parseRequestMessage( int connfd , sds buffer , int len )
{
	//判断服务器开启可接受的协议类型
	//如果服务器设置只接收tcp，那么全部以tcp协议处理，是半包还是粘包，交由逻辑处理
	int ret;
	if( servG->protocolType == PROTOCOL_TYPE_TCP_ONLY )
	{
		servG->connlist[connfd].protoType = TCP;
		createWorkerTask( connfd , buffer , len , PIPE_EVENT_MESSAGE , "PROTOCOL_TYPE_TCP_ONLY" );
		return BREAK_RECV;
	}
	else
	{
		//当前包如果是http协议，或者是websocket协议
		if( isHttpProtocol( buffer  , 8 ) == AE_TRUE  //如果是第一个请求，会走这里
			|| servG->connlist[connfd].protoType == WEBSOCKET  //半包的websocket或第一个握手后的请求会走这里
			|| servG->connlist[connfd].protoType == HTTP  //半包的http走这里，
		 )
		{
			if( servG->connlist[connfd].protoType != WEBSOCKET  )
			{
				servG->connlist[connfd].protoType = HTTP;
				//其实websocket握手的时候，也是做为http处理，因为此时只能根据GET推断出，是http协议
				//在解析的过程中，才可以推断出是websocket,所以握手后的recv的消息就是websocket
				//返回是否需要继续收包
				memset( &servG->connlist[connfd].hh , 0 , sizeof( httpHeader ));
				ret = httpRequestParse(  connfd , buffer , sdslen( buffer ) );
				
			}
			else
			{
				ret = wesocketRequestRarse(  connfd , 
					buffer , len , 
					&servG->connlist[connfd].hh ,  
					&servG->connlist[connfd].hs
				);

			}
			
			return ret;
		}
		//若是tcp直接返回
		else
		{
			 servG->connlist[connfd].protoType = TCP;
                	 createWorkerTask( connfd , buffer , len , PIPE_EVENT_MESSAGE , "PROTOCOL_TYPE_TCP" );
			 return BREAK_RECV;
		}
	}
}


void onClientReadable(aeEventLoop *el, int fd, void *privdata, int mask)
{
	//
	//worker send_buffer
	//sndbuf ->|----c1----|---c2------|--c3-----|-----c2---|---c3-------|
	//c1  	 ->|---header---|--------body------------|
	//client recv buffer
	
	//通常情况下，这个pos应该是0,如果包完整,并copy到send_buffer中后，将其清空
	//如果http,websocket下包没收全，则不要清空
    aeServer* serv = servG;
    ssize_t nread;
    unsigned int readlen, rcvbuflen ,datalen;
    int worker_id = fd % serv->workerNum;
    char buffer[1024];
	
    while(1) {
       // rcvbuflen = sdslen( servG->workers[worker_id].send_buffer );
    	nread = 0;
	memset( &buffer , 0 , sizeof( buffer )  );
        nread = read(fd, &buffer , sizeof( buffer ));

        if (nread == -1 && errno == EAGAIN) return; /* No more data ready. */
        if (nread == 0 )
        {
            onCloseByClient(  el , privdata , serv,  &serv->connlist[fd] );
            return;
        }
        else if( nread > 0 )
        {
		//此处必须的是sdscatlen 保证binnary安全，而不能是sdscat，
		servG->connlist[fd].recv_buffer = sdscatlen( servG->connlist[fd].recv_buffer , &buffer , nread );
		int ret = parseRequestMessage( fd , servG->connlist[fd].recv_buffer  , sdslen( servG->connlist[fd].recv_buffer ) );
			
		if( ret == BREAK_RECV )
		{
			sdsrange( servG->connlist[fd].recv_buffer ,  nread , -1);
			nread = 0;
				//此处只清理一个完整包长度，要考虑粘包情况header->complet_length，
				//TODO::
			        //sdsclear(  servG->connlist[fd].recv_buffer );
				break;
			}
			else if( ret == CONTINUE_RECV )
			{
				printf( "Continue recv...\n");
				continue;
			}
			else
			{
				return;
			}
                        return;
        }
	else
	{
		printf( "Recv Errno=%d,Err=%s \n" , errno, strerror( errno ) );
	}
    }

}

//此函数必须在写入send_buffer前执行
void setPipeWritable( aeEventLoop *el , void *privdata ,  int worker_id  )
{
	if (sdslen( servG->workers[worker_id].send_buffer ) == 0  )
	{
        	aeCreateFileEvent( el, 
			servG->workers[worker_id].pipefd[0],
			AE_WRITABLE,
			onMasterPipeWritable, worker_id );
	}			
}




aeEventLoop* getThreadEventLoop( int connfd )
{
	int reactor_id = connfd % servG->reactorNum;	
	return servG->reactorThreads[reactor_id].reactor.eventLoop;
}



void acceptCommonHandler( aeServer* serv ,int fd,char* client_ip,int client_port, int flags)
{
	if( serv->connectNum >= serv->maxConnect )
	{
		printf( "connect num over limit \n");
		close( fd );
		return;
	}
	
	if( fd <= 0 )
	{
		printf( "error fd is null\n");
		close(fd );
		return;
	}
	
	serv->connlist[fd].client_ip = client_ip;
	serv->connlist[fd].client_port = client_port;
	serv->connlist[fd].flags |= flags;
	serv->connlist[fd].fd = fd;
	serv->connlist[fd].disable = 0;
	serv->connlist[fd].send_buffer = sdsempty();
	serv->connlist[fd].recv_buffer = sdsempty();
	serv->connlist[fd].protoType = 0;
	bzero( & serv->connlist[fd].hs , sizeof( handshake  ) );
		
	int reactor_id = fd % serv->reactorNum;
	int worker_id  = fd % serv->workerNum;

    if (fd != -1)
	{
        anetNonBlock(NULL,fd);
        anetEnableTcpNoDelay(NULL,fd);

		aeEventLoop* el = serv->reactorThreads[reactor_id].reactor.eventLoop;
		
		
        if (aeCreateFileEvent( el ,fd,AE_READABLE, 
				onClientReadable, &fd ) == AE_ERR )
        {
            printf( "CreateFileEvent read error fd =%d,errno=%d,errstr=%s  \n" ,fd  , errno, strerror( errno )  );
            close(fd);
        }
	
    	aePipeData  data = {0};
    	data.type = PIPE_EVENT_CONNECT;
    	data.connfd = fd;
    	data.len = 0;
		serv->connectNum += 1;	
		setPipeWritable( el , NULL , worker_id );
	//	serv->connlist[fd].el = el;

		int sendlen = PIPE_DATA_HEADER_LENG;	
		pthread_mutex_lock( &servG->workers[worker_id].w_mutex );
    	servG->workers[worker_id].send_buffer = sdscatlen( servG->workers[worker_id].send_buffer , &data, sendlen );
    	pthread_mutex_unlock( &servG->workers[worker_id].w_mutex );
	}	
}

void onAcceptEvent( aeEventLoop *el, int fd, void *privdata, int mask)
{
    if( servG->listenfd == fd )
    {
        int client_port, connfd, max = 10;
        char client_ip[46];
        char neterr[1024];
        while(max--)//TODO::
        {
            connfd = anetTcpAccept( neterr, fd , client_ip, sizeof(client_ip), &client_port );
            if ( connfd == -1 ) {
                if (errno != EWOULDBLOCK)
                    printf("Accepting client Error connection: %s \n", neterr);
                return;
            }
            acceptCommonHandler( servG , connfd,client_ip,client_port,0 );
        }
    }
    else
    {
        printf( "onAcceptEvent other fd=%d \n" , fd );
    }
}

void runMainReactor( aeServer* serv )
{
    int res;
    //listenfd event,主进程主线程监听连接事件
    res = aeCreateFileEvent( serv->mainReactor->eventLoop,
                            serv->listenfd,
                            AE_READABLE,
                            onAcceptEvent,
                            NULL
                            );
	printf( "Master Run pid=%d and listen socketfd=%d is ok? [%d]\n",getpid(),serv->listenfd,res==0 );
	printf( "Server start ok ,You can exit program by Ctrl+C !!! \n");
	
    aeMain( serv->mainReactor->eventLoop );
    aeDeleteEventLoop( serv->mainReactor->eventLoop );
}


//此处send是发给了主进程的event_loop，而不是发给子进程的。
void masterSignalHandler( int sig )
{
    printf( "Master Send Sigal to Master Loop...\n");
    int save_errno = errno;
    int msg = sig;
    send( servG->sigPipefd[1], ( char* )&msg, 1, 0 );
    errno = save_errno;
}

void addSignal( int sig, void(*handler)(int), int restart  )
{
    struct sigaction sa;
    memset( &sa, '\0', sizeof( sa ) );
    sa.sa_handler = handler;
    if( restart == 1 )
    {
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset( &sa.sa_mask );
    assert( sigaction( sig, &sa, NULL ) != -1 );
}


//这里的信号是终端执行的中断等操作引起的事件。
//所以此处的addEvent是加入到主进程event_loop中的。
void installMasterSignal( aeServer* serv )
{
    int ret;
    char neterr[1024];
    
    //这个管道不是与worker通信的，而是与主线程的自己通信的
    ret  = socketpair( PF_UNIX, SOCK_STREAM, 0, serv->sigPipefd );
    assert( ret != -1 );
    anetNonBlock( neterr, serv->sigPipefd[1] );
    
    //把信号管道一端加到master event_loop中，使其被epoll关注
    ret = aeCreateFileEvent( serv->mainReactor->eventLoop ,
                            serv->sigPipefd[0],AE_READABLE,onSignEvent,NULL);
    
    //装载信号，指定回调函数,如果用户引发信号事件，则回调。
    addSignal( SIGCHLD, masterSignalHandler , 1 );	//catch child process exit event
    //addSignal( SIGTERM, masterSignalHandler , 1 );  //catch exit event by kill or Ctrl+C ..
    addSignal( SIGINT,  masterSignalHandler , 1 );
    addSignal( SIGPIPE, SIG_IGN , 1 );
}


aeServer* aeServerCreate( char* ip,int port )
{
    aeServer* serv = (aeServer*)zmalloc( sizeof(aeServer ));
    serv->runForever = startServer;
    serv->send =  sendMessageToReactor;
    serv->close = sendCloseEventToReactor;
    serv->sendToClient = anetWrite;
    serv->closeClient = freeClient;
    serv->listen_ip = ip;
    serv->port = port;
    serv->connectNum = 0;
    serv->protocolType = PROTOCOL_TYPE_WEBSOCKET_MIX;
    serv->reactorNum = 2;
    serv->workerNum = 3;
    serv->maxConnect = 1024;
	
    serv->connlist = shm_calloc( serv->maxConnect , sizeof( aeConnection ));
    serv->reactorThreads = zmalloc( serv->reactorNum * sizeof( aeReactorThread  ));
    serv->workers = zmalloc( serv->workerNum * sizeof(aeWorkerProcess));
    serv->mainReactor = zmalloc( sizeof( aeReactor ));
    serv->mainReactor->eventLoop = aeCreateEventLoop( 10 );
    aeSetBeforeSleepProc( serv->mainReactor->eventLoop ,initOnLoopStart );
    
    //安装信号装置
    installMasterSignal( serv  );
    servG = serv;
    
    return serv;
}

//reactor线程,
//创建子线程
//并在每个子线程中创建一个reactor/eventloop,放到全局变量中
void createReactorThreads( aeServer* serv  )
{
    int i,res;
    pthread_t threadid;
    void *thread_result;
    aeReactorThread *thread;
    
    for( i=0;i<serv->reactorNum;i++)
    {
        thread = &(serv->reactorThreads[i]);
        thread->param = zmalloc( sizeof( reactorThreadParam ));
        thread->param->serv = serv;
        thread->param->thid = i;
        
        res = pthread_create(&threadid, NULL, reactorThreadRun , (void *)thread->param );
        if (res != 0)
        {
            perror("Thread creat failed!");
            exit(0);
        }
        thread->thread_id = threadid;
    }
}

aeReactorThread getReactorThread( aeServer* serv, int i )
{
    return (aeReactorThread)(serv->reactorThreads[i]);
}

void readBodyFromPipe(  aeEventLoop *el, int fd , aePipeData data )
{
	int pos = PIPE_DATA_HEADER_LENG;
	int nread = 0;
	int needlen = data.len;
	int bodylen = 0;

	if( data.len <= 0 )
	{
		return;
	}
	
	nread = read( fd , data.data , needlen );
	if( nread < 0 )
	{
	 	if (nread == -1 && errno == EAGAIN) return;
		printf( "readBodyFromPipe error\n");   
	}

	//set writable event to connfd
	if ( sdslen( servG->connlist[data.connfd].send_buffer ) == 0  )
	{
        	aeCreateFileEvent( el, 
			data.connfd,
			AE_WRITABLE,
			onClientWritable,
			NULL 
		);
	}
	servG->connlist[data.connfd].send_buffer = sdscatlen( servG->connlist[data.connfd].send_buffer , data.data, nread );
}

//recv from pipe
void onMasterPipeReadable( aeEventLoop *el, int fd, void *privdata, int mask )
{ 
    int readlen =0;
    aePipeData data;

    //是否要加锁,多个线程同时对一个管道读数据
    readlen = read( fd, &data , PIPE_DATA_HEADER_LENG ); // 
	
    if( readlen == 0 )
    {
        close( fd );
    }
    else if( readlen == PIPE_DATA_HEADER_LENG )
    {
        //message,close，这样做是防止粘包,或半包
        if( data.type == PIPE_EVENT_MESSAGE )
        {
            if( servG->sendToClient )
            {
				readBodyFromPipe( el, fd , data );
            }
        }
        else if( data.type == PIPE_EVENT_CLOSE )
        {
            //close client socket
			printf( "onMasterPipeReadable PIPE_EVENT_CLOSE fd=%d \n" , data.connfd  );
			if( servG->closeClient )
			{
				if( sdslen( servG->connlist[data.connfd].send_buffer ) == 0 )
				{
					servG->closeClient( &servG->connlist[data.connfd] );
				}
				else
				{
					servG->connlist[data.connfd].disable = 2;
				}
			}
        }
        else
        {
            printf( "recvFromPipe recv unkown data.type=%d" , data.type );
        }
    }
    else
    {
        if( errno == EAGAIN )
        {
            return;
        }
        else
        {
            //printf( "Reactor Recv errno=%d,errstr=%s \n" , errno , strerror( errno ));
        }
    }	
}

//write to pipe
void onMasterPipeWritable(  aeEventLoop *el, int pipe_fd, void *privdata, int mask )
{
    ssize_t nwritten;
    int worker_id = (int)( privdata );
 
    pthread_mutex_lock( &servG->workers[worker_id].r_mutex );
    nwritten = write( pipe_fd , servG->workers[worker_id].send_buffer, sdslen(servG->workers[worker_id].send_buffer));
    //pipe fd error...
    if (nwritten <= 0)
    {
	close( pipe_fd );
        return;
    }
   
    //offset
    sdsrange(servG->workers[worker_id].send_buffer,nwritten,-1);
	
    ///if send_buffer no data need send, remove writable event
    if (sdslen(servG->workers[worker_id].send_buffer) == 0)
    {
        aeDeleteFileEvent( el, pipe_fd , AE_WRITABLE);
    }	
    pthread_mutex_unlock( &servG->workers[worker_id].r_mutex );
}

void *reactorThreadRun(void *arg)
{
    reactorThreadParam* param = (reactorThreadParam*)arg;
    aeServer* serv = param->serv;
    int thid = param->thid;
    aeEventLoop* el = aeCreateEventLoop( 1024 );
    serv->reactorThreads[thid].reactor.eventLoop = el;
 
    int ret,i;
	//每个线程都有workerNum个worker pipe
    for(  i = 0; i < serv->workerNum; i++ )
    {
        if ( aeCreateFileEvent( el,serv->workers[i].pipefd[0],
                     AE_READABLE,onMasterPipeReadable, thid  ) == -1 )
        {
            printf( "CreateFileEvent error fd "  );
            close(serv->workers[i].pipefd[0]);
        }
    }
    
    aeSetBeforeSleepProc( el ,initThreadOnLoopStart );
    aeMain( el );
	
    aeDeleteEventLoop( el );
    el = NULL;
}

int socketSetBufferSize(int fd, int buffer_size)
{
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size)) < 0)
    {
        printf("setsockopt(%d, SOL_SOCKET, SO_SNDBUF, %d) failed.", fd, buffer_size);
        return AE_ERR;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size)) < 0)
    {
        printf("setsockopt(%d, SOL_SOCKET, SO_RCVBUF, %d) failed.", fd, buffer_size);
        return AE_ERR;
    }
    return AE_OK;
}

void createWorkerProcess( aeServer* serv )
{
    char* neterr;
    int ret,i;
    for(  i = 0; i < serv->workerNum; i++ )
    {
		//3缓冲区
		serv->workers[i].send_buffer = sdsempty();
			
		//init mutex
		pthread_mutex_init( &(serv->workers[i].r_mutex) ,NULL);
		pthread_mutex_init( &(serv->workers[i].w_mutex) ,NULL);
			
		ret = socketpair( PF_UNIX, SOCK_STREAM, 0, serv->workers[i].pipefd );
		assert( ret != -1 );

        serv->workers[i].pid = fork();
        if( serv->workers[i].pid < 0 )
        {
            continue;
        }
        else if( serv->workers[i].pid > 0 )
        {
            //父进程
            close( serv->workers[i].pipefd[1] );
            anetNonBlock( neterr , serv->workers[i].pipefd[0] );
            continue;
        }
        else
        {
            //子进程
            close( serv->workers[i].pipefd[0] );
            anetNonBlock( neterr, serv->workers[i].pipefd[1] );
            runWorkerProcess( i , serv->workers[i].pipefd[1]  );
            exit( 0 );
        }
    }
}

void stopReactorThread( aeServer* serv  )
{
    int i;
    for( i=0;i<serv->reactorNum;i++)
    {
        aeStop( serv->reactorThreads[i].reactor.eventLoop );
    }
    
    for( i=0;i<serv->reactorNum;i++)
    {
        usleep( 1000 );
        pthread_cancel( serv->reactorThreads[i].thread_id );
        if( pthread_join( serv->reactorThreads[i].thread_id , NULL ) )
        {
            printf( "pthread join error \n");
        }
    }
	
	//free memory
	for( i=0;i<serv->reactorNum;i++)
    {
		if( serv->reactorThreads[i].param  )
		{
			zfree( serv->reactorThreads[i].param );
		}
		
		if( serv->reactorThreads[i].reactor.eventLoop != NULL )
		{
		   aeDeleteEventLoop( serv->reactorThreads[i].reactor.eventLoop  );
		}
		
	}
}

void freeWorkerBuffer( aeServer* serv )
{
	int i;
	for(  i = 0; i < serv->workerNum; i++ )
	{
		sdsfree( serv->workers[i].send_buffer  );
	}
}

int freeConnectBuffers( aeServer* serv )
{
	int i;
	int count = 0;
	int minfd = 3;//TODO::可以精确赋值
	
	if( serv->connectNum == 0 )
	{
		return 0;
	}
	
	for( i = minfd; i < serv->maxConnect ; i++ )
	{
		if( serv->connlist[i].disable == 0 )
		{
			sdsfree( serv->connlist[i].send_buffer );
			sdsfree( serv->connlist[i].recv_buffer );
			//zfree( serv->connlist[i].hs );
			count++;
		}
		if( count ==  serv->connectNum )
		{
			break;
		}	
	}
	return count;
}


void destroyServer( aeServer* serv )
{
    //1,停止,释放线程
    stopReactorThread( serv );
    
    //释放收发缓冲区
    freeConnectBuffers( serv );
	
    //2,释放共享内存
    shm_free( serv->connlist,1 );
    
    //3,释放由zmalloc分配的内存
    if( serv->reactorThreads )
    {
        zfree( serv->reactorThreads );
    }
    //4,释放N个woker缓冲区
    freeWorkerBuffer( serv );
	
    if( serv->workers )
    {
        zfree( serv->workers );
    }		
    
    if( serv->mainReactor )
    {
        zfree( serv->mainReactor );
    }
    
    //4,最后释放这个全局大变量
    if( serv != NULL )
    {
        zfree( serv );
    }
    puts("Master Exit ,Everything is ok !!!\n");
}

int startServer( aeServer* serv )
{	
    int sockfd[2];
    int sock_count = 0;          
    
    //监听TCP端口，这个接口其实可以同时监听多个端口的。
    listenToPort( serv->listen_ip, serv->port , sockfd , &sock_count );
    serv->listenfd = sockfd[0];

    //创建进程要先于线程，否则，会连线程一起fork了,好像会这样。。。
    createWorkerProcess( serv );	
    
    //创建子线程,每个线程都监听所有worker管道
    createReactorThreads( serv );
    
	__SLEEP_WAIT__;
	
    //运行主reactor
    runMainReactor( serv );
    
    //退出
    destroyServer( serv );
    return 0;
}
