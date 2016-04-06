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

unsigned int dictSdsCaseHash(const void *key)
{
    return dictGenCaseHashFunction((unsigned char*)key, sdslen((char*)key));
}

int dictSdsKeyCaseCompare(void *privdata, const void *key1, const void *key2)
{
    DICT_NOTUSED(privdata);
    return strcasecmp(key1, key2) == 0;
}

void dictSdsDestructor(void *privdata, void *val)
{
    DICT_NOTUSED(privdata);
    sdsfree(val);
}

dictType httpTableDictType =
{
    dictSdsCaseHash,           /* hash function */
    NULL,                      /* key dup */
    NULL,                      /* val dup */
    dictSdsKeyCaseCompare,     /* key compare */
    dictSdsDestructor,         /* key destructor */
    dictSdsDestructor,         /* val destructor */
};


int isValidConnfd( int fd )
{
    if( fd <= 0 )
    {
        return AE_FALSE;
    }
    if( servG->connlist[fd].fd != fd  )
    {
        return AE_FALSE;
    }
    if( servG->connlist[fd].disable == 1 )
    {
        return AE_FALSE;
    }
    return AE_TRUE;
}

void timerAdd( int ms , void* cb , void* params  )
{
   aeCreateTimeEvent( servG->worker->el, ms , cb  , params  , NULL );
}

void timerRemove( int tid  )
{
    aeDeleteTimeEvent( servG->worker->el, tid );
}

int appendSendBuffer( const char *buff , size_t len)
{
    servG->worker->send_buffer = sdscatlen( servG->worker->send_buffer , buff , len );
    if ( servG->worker->send_buffer == NULL)
    {
        printf( "appendSendBuffer Out Mem\n");
        return AE_ERR;
    }
    return AE_OK;
}

int createResponse( int connfd , char* buff , int len , char prototype , sds response )
{
    if( prototype == HTTP )
    {
		char content_length[64];
		int clen;
		clen = sprintf( content_length , "Content-Length: %d\r\n" , len );	

		response = sdscat( response , "HTTP/1.1 200 OK \r\n" );
		response = sdscat( response , "Server: appnet/1.0.0\r\n" );
		response = sdscatlen( response , content_length , clen );
		response = sdscat( response , "Content-Type: text/html\r\n" );
		//response = sdscat( response , "Content-Type: image/png\r\n");
		if( sdslen( servG->worker->header ) > 0 )
		{
			response = sdscat( response , servG->worker->header );
		}
		
		response = sdscat( response , "\r\n" );
		//printf( "Response:[%d][%s] \n" , sdslen( response ) , response );
		servG->worker->response = sdscatlen( response ,  buff , len );
		
		sdsclear(  servG->worker->header );
		return sdslen( servG->worker->response );
    }
    else
    {
        int outlen = 0;
        char res[len+256];
        wsMakeFrame( buff ,  len , &res , &outlen , WS_TEXT_FRAME );
        sdscatlen( servG->worker->response , res , outlen );
        return outlen;
    }
}

//如果不是tcp协议，要单独处理
int sendMessageToReactor( int connfd , char* buff , int len )
{
    if( isValidConnfd( connfd ) == AE_FALSE )
    {
        return -1;
    }
    printf( "sendMessageToReactor fd=%d \n" , connfd );
    aePipeData data;
    data.type = PIPE_EVENT_MESSAGE;
    data.len = len;
    data.connfd = connfd;
    int writeable = 0;
    if (sdslen( servG->worker->send_buffer ) == 0  )
    {
		writeable = 1;
    }
    //append data
    if( len >= 0 )
    {
		//if proto is http or websocket, need compose response data ,else direct send origin data
		if( servG->connlist[connfd].protoType != TCP )
		{
			char prototype = servG->connlist[connfd].protoType;
			int retlen;
			//buffer...
			retlen = createResponse(  connfd , buff , len , prototype , servG->worker->response  );
			printf( "CreateResponse Done \n");
			if( retlen < 0 )
			{
			   printf( "CreateResponse Error \n");
			   return;
			}
			
			if( sdslen( servG->worker->response ) != retlen )
			{
			   printf( "createResponse Error sdslen=%d \n" , sdslen( servG->worker->response ) );
			   return;
			}

			data.len = retlen;
			appendSendBuffer( &data , PIPE_DATA_HEADER_LENG );
			appendSendBuffer(  servG->worker->response , retlen );
			sdsclear( servG->worker->response );
		}
		else
		{
			appendSendBuffer( &data , PIPE_DATA_HEADER_LENG );
			appendSendBuffer(  buff , len );
		}

		if( data.len+PIPE_DATA_HEADER_LENG  == sdslen( servG->worker->send_buffer ) )
		{
			int ret;
			ret = aeCreateFileEvent( servG->worker->el,servG->worker->pipefd , AE_WRITABLE ,onWorkerPipeWritable,NULL );
			if( ret == AE_ERR )
			{
			   printf( "Error aeCreateFileEvent..\n");
			}
		}
    }
    return len;
}
void callUserRecvFunc( int connfd , sds buffer , int length )
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
    int readTotal = 0;
    if( data.len > 0 )
    {
        data.data = sdsempty();
        char buff[TMP_BUFFER_LENGTH];
        while( ( readlen = read( pipe_fd , buff , sizeof( buff ) ) ) > 0 )
        {
            data.data = sdscatlen( data.data , buff , readlen  );
            bodylen += readlen;
        }
    }
    callUserRecvFunc( data.connfd , data.data , bodylen );
}

void onWorkerPipeWritable( aeEventLoop *el, int fd, void *privdata, int mask )
{
    ssize_t nwritten;
    nwritten = write( fd, servG->worker->send_buffer, sdslen(servG->worker->send_buffer));
    //printf( "onWorkerPipeWritable[%d] \n" , nwritten );
    if (nwritten <= 0)
    {
        printf( "Worker I/O error writing to worker: %s \n", strerror(errno));
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

void onWorkerPipeReadable( aeEventLoop *el, int fd, void *privdata, int mask )
{
    aePipeData data;
    int readlen =0;
    //此处如果要把数据读取到大于包长的缓冲区中，不要用anetRead，否则就掉坑里了
   
    readlen = read(fd, &data , PIPE_DATA_HEADER_LENG );
    if( readlen == 0 )
    {
        close( fd );
    }
    else if( readlen == PIPE_DATA_HEADER_LENG )
    {
        servG->worker->connfd = data.connfd;
  
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
    return sdslen( servG->worker->send_buffer );
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
    aeStop( servG->worker->el );
}


void childChildHandler( int sig )
{
  
}
/*
int setHeader( char* headerLine )
{
	servG->worker->header= sdscatprintf(  servG->worker->header , "%s%s" , headerLine , "\r\n" );
	return AE_TRUE;
}
*/
static int http_header_is_valid_value(const char *value)
{
	const char *p = value;
	while ((p = strpbrk(p, "\r\n")) != NULL)
	{
		/* we really expect only one new line */
		p += strspn(p, "\r\n");
		/* we expect a space or tab for continuation */
		if (*p != ' ' && *p != '\t')
			return (0);
	}
	return (1);
}

int setHeader( char* key , char* value )
{
	if (strchr(key, '\r') != NULL || strchr(key, '\n') != NULL)
	{
		/* drop illegal headers */
		printf( "%s: dropping illegal header key\n", __func__ );
		return (-1);
	}

	if (!http_header_is_valid_value(value))
	{
		printf("%s: dropping illegal header value\n", __func__ );
		return (-1);
	}

	servG->worker->header= sdscatprintf(  servG->worker->header , "%s:%s%s" , key , value , "\r\n" );
	return AE_TRUE;
}


/**
 * process event types:
 * 1,parent process send readable event
 */
void runWorkerProcess( int pidx ,int pipefd )
{
    //servG->worker have private memory space in every process
    aeWorker* worker = zmalloc( sizeof( aeWorker ));
    worker->pid = getpid();
    worker->maxEvent = MAX_EVENT;
    worker->pidx = pidx;
    worker->pipefd = pipefd;
    worker->running = 1;
    worker->start = 0;
    worker->send_buffer = sdsnewlen( NULL , SEND_BUFFER_LENGTH  );
    worker->recv_buffer = sdsnewlen( NULL , RECV_BUFFER_LENGTH  );
    worker->response = sdsnewlen( NULL , SEND_BUFFER_LENGTH );
    worker->header = sdsempty();
	
    servG->worker = worker;
    sdsclear( servG->worker->send_buffer );
    sdsclear( servG->worker->recv_buffer );
    sdsclear( servG->worker->response );
 
    addSignal( SIGTERM, childTermHandler, 0 );
    signal(SIGINT , childTermHandler ); 
   //addSignal( SIGCHLD, childChildHandler , 1 );

    worker->el = aeCreateEventLoop( worker->maxEvent );
    aeSetBeforeSleepProc( worker->el,initWorkerOnLoopStart );
    int res;

    //listen pipe event expect reactor message come in;
    res = aeCreateFileEvent( worker->el,
		 worker->pipefd,
		 AE_READABLE,
		 onWorkerPipeReadable,NULL
	);
    printf("Worker Run pid=%d and listen pipefd=%d is ok? [%d]\n",worker->pid,pipefd,res==0 );

    aeMain(worker->el);
    aeDeleteEventLoop(worker->el);
    servG->onFinal( servG );
   
    //printf( "Worker pid=%d exit...\n" , worker->pid );
    sdsfree( worker->send_buffer );
    sdsfree( worker->recv_buffer );
    sdsfree( worker->response );
    sdsfree( worker->header );
	
    zfree( worker );
    shm_free( servG->connlist , 0 );
    close( pipefd );
    exit( 0 );
}
