/*************************************************************************

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


//master event loop
void initOnLoopStart(struct aeEventLoop *el)
{
}

//thread event loop
void initThreadOnLoopStart( struct aeEventLoop *el )
{
}
//异步信号事件
void onSignEvent( aeEventLoop *el, int fd, void *privdata, int mask)
{
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
    freeClient( conn );
}

//send to client,if send over , delete writable event
//if cannot send over in one write(), this function will be called at next times
//else if send over in one write(), writable event will be remove
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
    sdsrange(servG->connlist[fd].send_buffer,nwritten,-1);
    
    if (sdslen(servG->connlist[fd].send_buffer) == 0)
    {
        aeDeleteFileEvent( el, fd, AE_WRITABLE);
        if( servG->connlist[fd].disable == 2  || servG->connlist[fd].protoType == HTTP  )
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
    datalen = PIPE_DATA_HEADER_LENG + data.len;
    aeEventLoop* reactor_el = getThreadEventLoop( connfd );
    int worker_id  = connfd % servG->workerNum;
    setPipeWritable( reactor_el , worker_id , worker_id  );
    //append
    pthread_mutex_lock( &servG->workers[worker_id].w_mutex );
    servG->workers[worker_id].send_buffer = sdscatlen( servG->workers[worker_id].send_buffer , &data, PIPE_DATA_HEADER_LENG );
    if( len > 0 )
    {
        servG->workers[worker_id].send_buffer = sdscatlen( servG->workers[worker_id].send_buffer , buffer , len );
    }
    pthread_mutex_unlock( &servG->workers[worker_id].w_mutex );
}


int parseRequestMessage( int connfd , sds buffer , int len )
{
    int ret;
    if( servG->protocolType == PROTOCOL_TYPE_TCP_ONLY )
    {
        servG->connlist[connfd].protoType = TCP;
        createWorkerTask( connfd , buffer , len , PIPE_EVENT_MESSAGE , "PROTOCOL_TYPE_TCP_ONLY" );
        return BREAK_RECV;
    }
    else
    {
        if( isHttpProtocol( buffer  , 8 ) == AE_TRUE 
            || servG->connlist[connfd].protoType == WEBSOCKET                 
	    || servG->connlist[connfd].protoType == HTTP 
          )
        {
            if( servG->connlist[connfd].protoType != WEBSOCKET  )
            {
                servG->connlist[connfd].protoType = HTTP;
                memset( &servG->connlist[connfd].hh , 0 , sizeof( httpHeader ));
                ret = httpRequestParse(  connfd , buffer , sdslen( buffer ) );
            }
            else
            {
                ret = wesocketRequestRarse(  connfd ,buffer , len ,&servG->connlist[connfd].hh ,&servG->connlist[connfd].hs );
            }
            return ret;
        }
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
    //c1     ->|---header---|--------body------------|
    //client recv buffer
    aeServer* serv = servG;
    ssize_t nread;
    unsigned int readlen, rcvbuflen ,datalen;
    int worker_id = fd % serv->workerNum;
    char buffer[TMP_BUFFER_LENGTH];
    while(1)
    {
        nread = 0;
        memset( &buffer , 0 , sizeof( buffer )  );
        nread = read(fd, &buffer , sizeof( buffer ));
        if (nread == -1 && errno == EAGAIN)
        {
            return;    /* No more data ready. */
        }
        if (nread == 0 )
        {
            onCloseByClient(  el , privdata , serv,  &serv->connlist[fd] );
            return;
        }
        else if( nread > 0 )
        {
			//printf( "Recv From Client:[%d][%d][%s] \n" ,fd , nread, buffer );
            servG->connlist[fd].recv_buffer = sdscatlen( servG->connlist[fd].recv_buffer , &buffer , nread );
            int ret = parseRequestMessage( fd , servG->connlist[fd].recv_buffer  , sdslen( servG->connlist[fd].recv_buffer ) );
            if( ret == BREAK_RECV )
            {
                int complete_length = servG->connlist[fd].hh.complete_length;
                if( complete_length > 0 )
                {
                    sdsrange( servG->connlist[fd].recv_buffer ,  complete_length  , -1);
                }
                else
                {
                    sdsclear( servG->connlist[fd].recv_buffer );
                }
                break;
            }
            else if( ret == CONTINUE_RECV )
            {
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
        //serv->connlist[fd].el = el;
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
            if ( connfd == -1 )
            {
                if (errno != EWOULDBLOCK)
                {
                    printf("Accepting client Error connection: %s \n", neterr);
                }
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


void masterKillHandler( int sig )
{
    kill( 0, SIGTERM);
    pid_t pid;
    int stat,pidx;
    //WNOHANG
    while ( ( pid = waitpid( -1, &stat, 0 ) ) > 0 )
    {
        for( pidx = 0; pidx < servG->workerNum; pidx++ )
        {
            if( servG->workers[pidx].pid == pid )
            {
                close( servG->workers[pidx].pipefd[0] );
                servG->workers[pidx].pid = -1;
            }
        }
    }
    aeStop( servG->mainReactor->eventLoop );
    servG->running = 0;
    //destroyServer( servG );
    //printf( "Master Stoped pid=%d..\n", getpid() );
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


void installMasterSignal( aeServer* serv )
{
    //printf( "installMasterSignal...pid=%d \n" , getpid() );
    signal(SIGPIPE, SIG_IGN);
    addSignal (SIGINT, masterKillHandler , 1  );
//  signal (SIGKILL, masterSignalHandler );
//  signal (SIGQUIT, masterSignalHandler );
//  signal (SIGTERM, masterSignalHandler );
//  signal (SIGHUP, masterSignalHandler );
//  signal(SIGSEGV, masterSignalHandler );
}
void testsds( char* str )
{
    printf( "===========testsds=====================%s\n" , str );
    char* p1 = "xxxxxxxxxxxxx";
    sds snd = sdsempty();
    snd = sdscatlen( snd , p1 ,strlen( p1 ));
    snd = sdscatlen( snd , p1 ,strlen( p1 ));
    snd = sdscatlen( snd , p1 ,strlen( p1 ));
    snd = sdscatlen( snd , p1 ,strlen( p1 ));
    printf( "snd len=%d,buff=%s \n" , sdslen( snd ) , snd );
}


void createReactorThreads( aeServer* serv  )
{
    int i,res;
    pthread_t threadid;
    void *thread_result;
    aeReactorThread *thread;
    for( i=0; i<serv->reactorNum; i++)
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


//read data body from pipe,
void readBodyFromPipe(  aeEventLoop *el, int fd , aePipeData data )
{
    int pos = PIPE_DATA_HEADER_LENG;
    int nread = 0;
    int bodylen = 0;
    if( data.len <= 0 )
    {
        return;
    }
	

	char read_buff[TMP_BUFFER_LENGTH];
	while( 1 )
	{
		nread  = read( fd , read_buff  , TMP_BUFFER_LENGTH );
		if( nread > 0 )
		{
			bodylen += nread;
			//strcatlen function can extend space when current space not enough
			if ( sdslen( servG->connlist[data.connfd].send_buffer) == 0 )
        		{
				aeCreateFileEvent( el, data.connfd , AE_WRITABLE, onClientWritable, NULL );      
        		}
			servG->connlist[data.connfd].send_buffer = sdscatlen( servG->connlist[data.connfd].send_buffer , read_buff  , nread );
	 
			//printf( "RecvFromPipe Need:[%d][%d]\n" , needlen , bodylen );
			if( bodylen == data.len )
			{
				//printf( "RecvFromPipe Break:[%d]=[%d]\n" , bodylen , data.len  );
				break;
			}

		}
	}
	
    if( bodylen <= 0 )
    {
        if (nread == -1 && errno == EAGAIN)
        {
            return;
        }
        printf( "readBodyFromPipe error\n");
        return;
    }
  
   
}


//recv from pipe
void onMasterPipeReadable( aeEventLoop *el, int fd, void *privdata, int mask )
{
    int readlen =0;
    aePipeData data;
   
    while(  ( readlen = read( fd, &data , PIPE_DATA_HEADER_LENG ) ) > 0 )
    {
        if( readlen == 0 )
        {
            close( fd );
        }
        else if( readlen == PIPE_DATA_HEADER_LENG )
        {
            //message,close
            if( data.type == PIPE_EVENT_MESSAGE )
            {
                if( servG->sendToClient )
                {
                    readBodyFromPipe( el, fd , data );
                }
            }
            else if( data.type == PIPE_EVENT_CLOSE )
            {
                char prototype = servG->connlist[data.connfd].protoType;
                if( prototype == WEBSOCKET )
                {
                    int outlen = 0;
                    char close_buff[1024];
                    char* reason = "normal_close";
                    wsMakeFrame( reason , strlen( reason ) , &close_buff , &outlen , WS_CLOSING_FRAME );
                    if ( sdslen( servG->connlist[data.connfd].send_buffer) == 0 )
                    {
                        aeCreateFileEvent( el,
						   data.connfd,
						   AE_WRITABLE,
						   onClientWritable,
						   NULL
                        );
                    }
                    servG->connlist[data.connfd].send_buffer = sdscatlen( servG->connlist[data.connfd].send_buffer , close_buff , outlen );
                }
                //close client socket
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
                //printf( "recvFromPipe recv unkown data.type=%d" , data.type );
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
    ///if send_buffer empty, remove writable event
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
    aeEventLoop* el = aeCreateEventLoop( MAX_EVENT );
    serv->reactorThreads[thid].reactor.eventLoop = el;
    int ret,i;
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
        return AE_ERR;
    }
    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size)) < 0)
    {
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
            //parent
            close( serv->workers[i].pipefd[1] );
	    anetSetSendBuffer( neterr , serv->workers[i].pipefd[0] , SOCKET_SND_BUF_SIZE );
            anetNonBlock( neterr , serv->workers[i].pipefd[0] );
            continue;
        }
        else
        {
            //child
            close( serv->workers[i].pipefd[0] );
            anetNonBlock( neterr, serv->workers[i].pipefd[1] );
	    anetSetSendBuffer( neterr , serv->workers[i].pipefd[1] , SOCKET_SND_BUF_SIZE );
            runWorkerProcess( i , serv->workers[i].pipefd[1]  );
            exit( 0 );
        }
    }
}
void stopReactorThread( aeServer* serv  )
{
    int i;
    for( i=0; i<serv->reactorNum; i++)
    {
        aeStop( serv->reactorThreads[i].reactor.eventLoop );
    }
    for( i=0; i<serv->reactorNum; i++)
    {
        usleep( 1000 );
        pthread_cancel( serv->reactorThreads[i].thread_id );
        if( pthread_join( serv->reactorThreads[i].thread_id , NULL ) )
        {
            printf( "pthread join error \n");
        }
    }
    //free memory
    for( i=0; i<serv->reactorNum; i++)
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
    int minfd = 3;//TODO::
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
    printf( "destroyServer...\n");
    stopReactorThread( serv );
   
    freeConnectBuffers( serv );
   
    shm_free( serv->connlist,1 );
    if( serv->reactorThreads )
    {
        zfree( serv->reactorThreads );
    }
  
    freeWorkerBuffer( serv );
    if( serv->workers )
    {
        zfree( serv->workers );
    }
    if( serv->mainReactor )
    {
        zfree( serv->mainReactor );
    }
    if( serv != NULL )
    {
        zfree( serv );
    }
    puts("Master Exit ,Everything is ok !!!\n");
}

int setOption( char* key , char* val )
{
	if( strcmp( key , OPT_WORKER_NUM ) == 0 )
	{
	    if( atoi( val ) <= 0 )
	    {
			return AE_FALSE;
	    }	   
	    servG->workerNum = atoi( val );
	}
	else if( strcmp( key , OPT_REACTOR_NUM ) == 0 )
	{
	        if( atoi( val ) <= 0 )
		{
			return AE_FALSE;
		}
	    	servG->reactorNum = atoi( val ); 
	}
	else if( strcmp( key , OPT_MAX_CONNECTION  ) == 0 )
	{
		if( atoi( val ) <= 0 )
		{
			return AE_FALSE;
		}
		servG->maxConnect = atoi( val ); 
	}
	else if( strcmp( key , OPT_PROTOCOL_TYPE  ) == 0 )
	{
		int type =  atoi( val );
		if( type < 0 || type > PROTOCOL_TYPE_WEBSOCKET_MIX )
		{
			return AE_FALSE;
		}
		servG->protocolType = type; 
	}
	else if( strcmp( key , APPNET_HTTP_DOCS_ROOT  ) == 0 )
	{
		memset( servG->http_docs_root ,  0 , sizeof(  servG->http_docs_root ) );
		memcpy( servG->http_docs_root , val , strlen( val ) );
	}
	else if( strcmp( key , APPNET_HTTP_404_PAGE  ) == 0 )
	{
		memset( servG->http_404_page ,  0 , sizeof(  servG->http_404_page ) );
		memcpy( servG->http_404_page , val , strlen( val ) );
	}
	else if( strcmp( key , APPNET_HTTP_50X_PAGE  ) == 0 )
	{
		memset( servG->http_50x_page ,  0 , sizeof(  servG->http_50x_page ) );
		memcpy( servG->http_50x_page , val , strlen( val ) );
	}
	else
	{
	    printf( "Unkown Option\n" );
	    return AE_FALSE;
	}

	if( servG->reactorNum > servG->workerNum )
	{
	   servG->reactorNum = servG->workerNum;
	}
	return AE_TRUE;
}

void initServer(  aeServer* serv )
{
    serv->connlist = shm_calloc( serv->maxConnect , sizeof( aeConnection ));
    serv->reactorThreads = zmalloc( serv->reactorNum * sizeof( aeReactorThread  ));
    serv->workers = zmalloc( serv->workerNum * sizeof(aeWorkerProcess));
    serv->mainReactor = zmalloc( sizeof( aeReactor ));
    serv->mainReactor->eventLoop = aeCreateEventLoop( 10 );
    aeSetBeforeSleepProc( serv->mainReactor->eventLoop ,initOnLoopStart );
    installMasterSignal( serv  );
}

int startServer( aeServer* serv )
{
    int sockfd[2];
    int sock_count = 0;
	
	//memory alloc
    initServer( serv );

    listenToPort( serv->listen_ip, serv->port , sockfd , &sock_count );

    serv->listenfd = sockfd[0];

    createWorkerProcess( serv );

    createReactorThreads( serv );

    __SLEEP_WAIT__;
	
    runMainReactor( serv );

    destroyServer( serv );
    return 0;
}

aeServer* aeServerCreate( char* ip,int port )
{
    aeServer* serv = (aeServer*)zmalloc( sizeof(aeServer ));
    serv->listen_ip = ip;
    serv->port = port;
    serv->connectNum = 0;
    serv->reactorNum = 1;
    serv->workerNum = 1;
    serv->maxConnect = 1024;
    serv->protocolType = PROTOCOL_TYPE_WEBSOCKET_MIX;
    serv->runForever = startServer;
    serv->send =  sendMessageToReactor;
    serv->close = sendCloseEventToReactor;
    serv->setOption = setOption;
    serv->sendToClient = anetWrite;
    serv->closeClient = freeClient;

    memset( serv->http_docs_root , 0 , sizeof( serv->http_docs_root ) );
    memset( serv->http_404_page , 0 , sizeof( serv->http_404_page ) );
    memset( serv->http_50x_page , 0 ,  sizeof( serv->http_50x_page ) );

    memcpy( serv->http_docs_root , DEFAULT_HTTP_DOCS_ROOT , strlen( DEFAULT_HTTP_DOCS_ROOT ) );
    memcpy( serv->http_404_page  , DEFALUT_HTTP_404_PAGE , strlen( DEFALUT_HTTP_404_PAGE ) );
    memcpy( serv->http_50x_page  , DEFALUT_HTTP_50X_PAGE , strlen( DEFALUT_HTTP_50X_PAGE ) );


    servG = serv;
    return serv;
}
