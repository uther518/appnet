#include "appnet_event.h"
#include "appnet_server.h"
#include "appnet_dict.h"
#include "appnet_request.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

//======================
void initWorkerOnLoopStart( aeEventLoop *l )
{
	if (servG->worker->start == 0)
	{
		servG->onStart( servG );
		servG->worker->start = 1;
	}
}

int getWorkerPipeIndex( int tofd )
{
	return servG->worker->pidx * servG->reactor_num + tofd % servG->reactor_num;
}

unsigned int dictSdsCaseHash( const void *key )
{
	return dictGenCaseHashFunction( (unsigned char *) key , sdslen( (char *) key ) );
}

int dictSdsKeyCaseCompare( void *privdata , const void *key1 , const void *key2 )
{
	DICT_NOTUSED( privdata );
	return strcasecmp( key1 , key2 ) == 0;
}

void dictSdsDestructor( void *privdata , void *val )
{
	DICT_NOTUSED( privdata );
	sdsfree( val );
}

dictType headerDictType =
{
		dictSdsCaseHash, /* hash function */
		NULL, /* key dup */
		NULL, /* val dup */
		dictSdsKeyCaseCompare, /* key compare */
		dictSdsDestructor, /* key destructor */
		dictSdsDestructor, /* val destructor */
};

int isValidConnfd( int connfd )
{
	if (connfd <= 0)
	{
		return AE_FALSE;
	}
	
	if (servG->connlist[connfd].connfd != connfd)
	{
		return AE_FALSE;
	}
	
	if (servG->connlist[connfd].disable == 1)
	{
		return AE_FALSE;
	}
	return AE_TRUE;
}

void timerAdd( int ms , void *cb , void *params )
{
	aeCreateTimeEvent( servG->worker->el , ms , cb , params , NULL );
}

//异步任务进程回调
int taskCallback( char *task_data , int data_len , int taskid , int to )
{
	appnetPipeData data =
	{0};
	data.type = PIPE_EVENT_TASK;
	
	asyncTask task;
	task.id = taskid;
	task.to = to;
	task.from = servG->worker->pidx;
	
	data.len = 0;
	data.len += sizeof(asyncTask);
	data.len += data_len;
	
	int index;
	int min = servG->worker->pidx * servG->reactor_num;
	index = task.id % servG->reactor_num + min;
	
	if (sdslen( servG->worker->send_buffer ) == 0)
	{
		int ret;
		ret = aeCreateFileEvent( servG->worker->el ,
				servG->worker_pipes[index].pipefd[1] , AE_WRITABLE ,
				onWorkerPipeWritable , NULL );
		if (ret == AE_ERR)
		{
			printf( "Error aeCreateFileEvent..\n" );
		}
	}
	
	appendSendBuffer( &data , PIPE_DATA_HEADER_LENG );
	appendSendBuffer( &task , sizeof(asyncTask) );
	appendSendBuffer( task_data , data_len );
	
	return AE_TRUE;
}

int addAsyncTask( char *params , int task_worker_id )
{
	appnetPipeData data =
	{0};
	data.type = PIPE_EVENT_TASK;
	
	asyncTask task;
	task.id = servG->worker->task_id + 1;
	task.to = task_worker_id;
	task.from = servG->worker->pidx;
	
	data.len = 0;
	data.len += sizeof(asyncTask);
	data.len += strlen( params );
	
	int index;
	int min = servG->worker->pidx * servG->reactor_num;
	index = task.id % servG->reactor_num + min;
	
	if (sdslen( servG->worker->send_buffer ) == 0)
	{
		int ret;
		ret = aeCreateFileEvent( servG->worker->el ,
				servG->worker_pipes[index].pipefd[1] , AE_WRITABLE ,
				onWorkerPipeWritable , NULL );
		if (ret == AE_ERR)
		{
			printf( "Error aeCreateFileEvent..\n" );
		}
	}
	
	appendSendBuffer( &data , PIPE_DATA_HEADER_LENG );
	appendSendBuffer( &task , sizeof(asyncTask) );
	appendSendBuffer( params , strlen( params ) );
	servG->worker->task_id += 1;
	
	return servG->worker->task_id;
}

void timerRemove( int tid )
{
	aeDeleteTimeEvent( servG->worker->el , tid );
}

int appendSendBuffer( const char *buff , size_t len )
{
	servG->worker->send_buffer = sdscatlen( servG->worker->send_buffer , buff , len );
	if (servG->worker->send_buffer == NULL)
	{
		printf( "appendSendBuffer Out Mem\n" );
		return AE_ERR;
	}
	return AE_OK;
}

int httpRedirectUrl( char *uri , int code )
{
	if (code != 301)
		code = 302;
	return httpRespCode( code , uri );
}

int httpRespCode( int status , char *val )
{
	headerOut header_out;
	memset( &header_out , 0 , sizeof( header_out ) );
	header_out.req = &servG->worker->req_header;
	memcpy( header_out.req->mime_type , "text/plain" ,
			sizeof( header_out.req->mime_type ) );
	
	createCommonHeader( &header_out , status );
	if (status == 301 || status == 302)
	{
		respAppendHeader( &header_out , HEADER_LOCATION , val );
	}
	respAppendHeader( &header_out , HEADER_CONTENT_LENGTH , 0 );
	respAppendHeader( &header_out , HEADER_END_LINE );
	
	int connfd = servG->worker->req_header.connfd;
	appnetPipeData data;
	data.type = PIPE_EVENT_MESSAGE;
	data.len = header_out.length;
	data.connfd = connfd;
	
	if (sdslen( servG->worker->send_buffer ) == 0)
	{
		int ret;
		int index = getWorkerPipeIndex( connfd );
		ret = aeCreateFileEvent( servG->worker->el ,
				servG->worker_pipes[index].pipefd[1] , AE_WRITABLE ,
				onWorkerPipeWritable , NULL );
		if (ret == AE_ERR)
		{
			printf( "Error aeCreateFileEvent..\n" );
		}
	}
	appendSendBuffer( &data , PIPE_DATA_HEADER_LENG );
	appendSendBuffer( header_out.data , header_out.length );
	return AE_TRUE;
}

#define BUF_LEN 0xFFFF
int createResponse( int connfd , char *buff , int len , char proto_type ,
		sds response )
{
	
	if (proto_type == HTTP)
	{
		char content_length[64];
		int clen;
		clen = sprintf( content_length , "Content-Length: %d\r\n" , len );
		response = sdscat( response , "HTTP/1.1 200 OK \r\n" );
		response = sdscat( response , "Server: appnet/1.1.0\r\n" );
		response = sdscatlen( response , content_length , clen );
		response = getRespHeaderString( response );
		response = sdscat( response , "\r\n" );
		servG->worker->response = sdscatlen( response , buff , len );
		resetRespHeader( servG->worker->resp_header );
		return sdslen( servG->worker->response );
	}
	else
	{
		size_t outlen = BUF_LEN;
		uint8_t res[BUF_LEN];
		wsMakeFrame( buff , len , res , &outlen , WS_TEXT_FRAME );
		sdscatlen( servG->worker->response , res , outlen );
		return outlen;
	}
}

int sendMessageToReactor( int connfd , char *buff , int len )
{
	if (isValidConnfd( connfd ) == AE_FALSE)
	{
		return -1;
	}
	
	appnetPipeData data;
	data.type = PIPE_EVENT_MESSAGE;
	data.len = len;
	data.connfd = connfd;
	
	int writeable = 0;
	if (sdslen( servG->worker->send_buffer ) == 0)
	{
		writeable = 1;
	}
	
	if (len >= 0)
	{
		
		/* if proto is http or websocket, need compose response data ,else direct
		 send origin data */
		if (servG->worker->proto != TCP)
		{
			char proto_type = servG->worker->proto;
			int retlen;
			
			retlen = createResponse( connfd , buff , len , proto_type ,
					servG->worker->response );
			if (retlen < 0)
			{
				printf( "CreateResponse Error \n" );
				return;
			}
			
			if (sdslen( servG->worker->response ) != retlen)
			{
				printf( "createResponse Error sdslen=%d \n" ,
						sdslen( servG->worker->response ) );
				return;
			}
			
			data.len = retlen;
			appendSendBuffer( &data , PIPE_DATA_HEADER_LENG );
			appendSendBuffer( servG->worker->response , retlen );
			sdsclear( servG->worker->response );
			
		}
		else
		{
			appendSendBuffer( &data , PIPE_DATA_HEADER_LENG );
			appendSendBuffer( buff , len );
		}
		
		if (writeable > 0 ||
				data.len + PIPE_DATA_HEADER_LENG ==
						sdslen( servG->worker->send_buffer ))
		{
			
			int ret;
			int index = getWorkerPipeIndex( connfd );
			ret = aeCreateFileEvent( servG->worker->el ,
					servG->worker_pipes[index].pipefd[1] , AE_WRITABLE ,
					onWorkerPipeWritable , NULL );
			
			if (ret == AE_ERR)
			{
				printf( "Error aeCreateFileEvent..\n" );
			}
		}
	}
	return len;
}

void callUserRecvFunc( int connfd , sds buffer , int length )
{
	if (servG->onRecv)
	{
		servG->onRecv( servG , &servG->connlist[connfd] , buffer , length );
	}
}

void readTaskFromPipe( int pipe_fd , appnetPipeData data )
{
	int readlen;
	if (data.len > 0)
	{
		char buff[TMP_BUFFER_LENGTH] =
		{0};
		
		while (( readlen = read( pipe_fd , buff , data.len ) ) > 0)
		{
			if (readlen == data.len)
			{
				break;
			}
		}
		
		asyncTask task;
		char param[1024] =
		{0};
		memcpy( &task , buff , sizeof(asyncTask) );
		memcpy( &param , buff + sizeof(asyncTask) , data.len - sizeof(asyncTask) );
		
		if (servG->worker->pidx >= servG->worker_num)
		{
			
			servG->onTask( param , data.len - sizeof(asyncTask) , task.id , task.from );
		}
		else
		{
			servG->onTaskCallback( param , data.len - sizeof(asyncTask) , task.id ,
					task.from );
		}
	}
}

void readWorkerBodyFromPipe( int pipe_fd , appnetPipeData data )
{
	int pos = PIPE_DATA_HEADER_LENG;
	int readlen = 0;
	int bodylen = 0;
	int ret;
	
	if (data.len > 0)
	{
		data.data = sdsempty();
		char buff[TMP_BUFFER_LENGTH];
		
		while (( readlen = read( pipe_fd , buff , data.len - bodylen ) ) > 0)
		{
			data.data = sdscatlen( data.data , buff , readlen );
			bodylen += readlen;
			if (bodylen == data.len)
				break;
		}
	}
	
	servG->worker->proto = data.proto;
	if (data.proto == HTTP)
	{
		
		httpHeader *header = &servG->worker->req_header;
		memset( header , 0 , sizeof( header ) );
		header->connfd = data.connfd;
		
		ret = httpHeaderParse( header , data.data , data.header_len );
		if (ret < AE_OK)
		{
			printf( "Worker httpHeaderParse Error \n" );
		}
		
		callUserRecvFunc( data.connfd , data.data + data.header_len ,
				data.len - data.header_len );
	}
	else
	{
		
		callUserRecvFunc( data.connfd , data.data , bodylen );
	}
}

void onWorkerPipeWritable( aeEventLoop *el , int fd , void *privdata , int mask )
{
	
	ssize_t nwritten;
	nwritten =
			write( fd , servG->worker->send_buffer , sdslen( servG->worker->send_buffer ) );
	
	if (nwritten <= 0)
	{
		printf( "Worker I/O error writing to worker: %s \n" , strerror( errno ) );
		return;
	}
	
	sdsrange( servG->worker->send_buffer , nwritten , -1 );
	
	/* if send_buffer no data need send, remove writable event */
	if (sdslen( servG->worker->send_buffer ) == 0)
	{
		aeDeleteFileEvent( el , fd , AE_WRITABLE );
	}
	else
	{
		printf( "Writing Continue....\n" );
	}
}

void onWorkerPipeReadable( aeEventLoop *el , int fd , void *privdata , int mask )
{
	appnetPipeData data;
	int readlen = 0;
	
	readlen = read( fd , &data , PIPE_DATA_HEADER_LENG );
	if (readlen == 0)
	{
		close( fd );
	}
	else if (readlen == PIPE_DATA_HEADER_LENG)
	{
		
		servG->worker->connfd = data.connfd;
		// connect,read,close
		if (data.type == PIPE_EVENT_CONNECT)
		{
			
			if (servG->onConnect)
			{
				servG->onConnect( servG , data.connfd );
			}
			
		}
		else if (data.type == PIPE_EVENT_MESSAGE)
		{
			
			readWorkerBodyFromPipe( fd , data );
		}
		else if (data.type == PIPE_EVENT_TASK)
		{
			
			readTaskFromPipe( fd , data );
		}
		else if (data.type == PIPE_EVENT_CLOSE)
		{
			
			if (servG->onClose)
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

int sendCloseEventToReactor( int connfd )
{
	appnetPipeData data;
	data.type = PIPE_EVENT_CLOSE;
	data.connfd = connfd;
	data.len = 0;
	int index = getWorkerPipeIndex( connfd );
	if (sdslen( servG->worker->send_buffer ) == 0)
	{
		int ret = aeCreateFileEvent( servG->worker->el ,
				servG->worker_pipes[index].pipefd[1] ,
				AE_WRITABLE , onWorkerPipeWritable , NULL );
		
		if (ret == AE_ERR)
		{
			printf( "setPipeWritable_error %s:%d \n" , __FILE__ , __LINE__ );
		}
	}
	else
	{
		printf( "send_buffer=%d,len=%d\n" , sdslen( servG->worker->send_buffer ) ,
				data.len );
	}
	
	servG->worker->send_buffer =
			sdscatlen( servG->worker->send_buffer , &data , PIPE_DATA_HEADER_LENG );
	
	return sdslen( servG->worker->send_buffer );
}

int socketWrite( int __fd , void *__data , int __len )
{
	int n = 0;
	int written = 0;
	while (written < __len)
	{
		n = write( __fd , __data + written , __len - written );
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
				printf( "write %d bytes failed." , __len );
				return AE_ERR;
			}
		}
		written += n;
	}
	return written;
}

int send2ReactorThread( int connfd , appnetPipeData data )
{
	int index = getWorkerPipeIndex( connfd );
	if (sdslen( servG->worker->send_buffer ) == 0)
	{
		int ret = aeCreateFileEvent( servG->worker->el ,
				servG->worker_pipes[index].pipefd[1] ,
				AE_WRITABLE , onWorkerPipeWritable , NULL );
		if (ret == AE_ERR)
		{
			printf( "setPipeWritable_error %s:%d \n" , __FILE__ , __LINE__ );
		}
	}
	else
	{
		printf( "send_buffer=%d,len=%d\n" , sdslen( servG->worker->send_buffer ) ,
				data.len );
	}
	servG->worker->send_buffer =
			sdscatlen( servG->worker->send_buffer , &data , PIPE_DATA_HEADER_LENG );
}

int timerCallback( struct aeEventLoop *l , long long id , void *data )
{
	printf( "I'm time_cb,here [EventLoop: %p],[id : %lld],[data: %s],pid=%d \n" , l ,
			id , data , getpid() );
	return 5000;
}

void finalCallback( struct aeEventLoop *l , void *data )
{
	puts( "call the unknow final function \n" );
}

void childTermHandler( int sig )
{
	aeStop( servG->worker->el );
}

void childChildHandler( int sig )
{
}

static int httpHeaderIsValidValue( const char *value )
{
	const char *p = value;
	while (( p = strpbrk( p , "\r\n" ) ) != NULL)
	{
		
		/* we really expect only one new line */
		p += strspn( p , "\r\n" );
		
		/* we expect a space or tab for continuation */
		if (*p != ' ' && *p != '\t')
			return ( 0 );
	}
	return ( 1 );
}

int setHeader( char *key , char *value )
{
	if (strchr( key , '\r' ) != NULL || strchr( key , '\n' ) != NULL)
	{
		/* drop illegal headers */
		printf( "%s: dropping illegal header key\n" , __func__ );
		return ( -1 );
	}
	
	if (!httpHeaderIsValidValue( value ))
	{
		printf( "%s: dropping illegal header value\n" , __func__ );
		return ( -1 );
	}
	return setRespHeader( key , value );
}

int setRespHeader( char *key , char *val )
{
	if (strlen( key ) < 0 || strlen( val ) < 0)
	{
		printf( "Header Error key=%s,val=%s \n" , key , val );
		return AE_FALSE;
	}
	
	if (strcasecmp( key , "Server" ) == 0 ||
			strcasecmp( key , "Content-Length" ) == 0)
	{
		printf( "Header Key Cannot Modify By User \n" );
		return AE_FALSE;
	}
	
	sds skey = sdsnew( key );
	sds sval = sdsnew( val );
	int ret = dictReplace( servG->worker->resp_header , skey , sval );
	return AE_TRUE;
}

sds getRespHeaderString( sds header )
{
	dictIterator *it = dictGetIterator( servG->worker->resp_header );
	dictEntry *de;
	while (( de = dictNext( it ) ) != NULL)
	{
		header = sdscatprintf( header , "%s:%s\r\n" , dictGetKey(de) , dictGetVal(de) );
	}
	dictReleaseIterator( it );
	return header;
}

void resetRespHeader( dict *resp_header )
{
	dictEmpty( resp_header , NULL );
	// set default header options
	setRespHeader( "Content-Type" , "text/html" );
}

/**
 * process event types:
 * 1,parent process send readable event
 */
void runWorkerProcess( int pidx )
{
	// servG->worker have private memory space in every process
	appnetWorker *worker = zmalloc( sizeof(appnetWorker) );
	worker->pid = getpid();
	worker->max_event = MAX_EVENT;
	worker->pidx = pidx;
	// worker->pipefd = pipefd;
	worker->running = 1;
	worker->start = 0;
	worker->task_id = 0;
	worker->send_buffer = sdsnewlen( NULL , SEND_BUFFER_LENGTH );
	worker->recv_buffer = sdsnewlen( NULL , RECV_BUFFER_LENGTH );
	worker->response = sdsnewlen( NULL , SEND_BUFFER_LENGTH );
	
	worker->resp_header = dictCreate( &headerDictType , NULL );
	servG->worker = worker;
	resetRespHeader( worker->resp_header );
	
	sdsclear( servG->worker->send_buffer );
	sdsclear( servG->worker->recv_buffer );
	sdsclear( servG->worker->response );
	
	addSignal( SIGTERM , childTermHandler , 0 );
	signal( SIGINT , childTermHandler );
	signal( SIGPIPE , SIG_IGN );
	
	worker->el = aeCreateEventLoop( worker->max_event );
	aeSetBeforeSleepProc( worker->el , initWorkerOnLoopStart );
	int res,index,thid;
	
	/* listen pipe event expect reactor message come in; */
	for (thid = 0; thid < servG->reactor_num; thid++)
	{
		index = pidx * servG->reactor_num + thid;
		res = aeCreateFileEvent( worker->el , servG->worker_pipes[index].pipefd[1] ,
				AE_READABLE , onWorkerPipeReadable , NULL );
		printf( "Worker Run pid=%d, index=%d, pidx=%d and listen pipefd=%d is ok? "
				"[%d]\n" ,
				getpid() , index , pidx , servG->worker_pipes[index].pipefd[1] ,
				res == 0 );
	}
	
	// res = aeCreateTimeEvent( worker->el, 1000 , timerCallback , NULL , NULL );
	
	aeMain( worker->el );
	aeDeleteEventLoop( worker->el );
	servG->onFinal( servG );
	
	sdsfree( worker->send_buffer );
	sdsfree( worker->recv_buffer );
	sdsfree( worker->response );
	dictRelease( worker->resp_header );
	zfree( worker );
	shm_free( servG->connlist , 0 );
	// close( pipefd );
	exit( 0 );
}
