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
#include "http_request.h"

//======================
void initWorkerOnLoopStart( aeEventLoop *l)
{
	if ( servG->worker->start == 0 )
	{
		servG->onStart( servG );
		servG->worker->start = 1;
	}
}

int  get_worker_pipe_index( int tofd )
{
	return servG->worker->pidx * servG->reactorNum + tofd%servG->reactorNum;
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

dictType headerDictType =
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
	if ( fd <= 0 )
	{
		return AE_FALSE;
	}
	if ( servG->connlist[fd].fd != fd  )
	{
		return AE_FALSE;
	}
	if ( servG->connlist[fd].disable == 1 )
	{
		return AE_FALSE;
	}
	return AE_TRUE;
}

void timerAdd( int ms , void* cb , void* params  )
{
	aeCreateTimeEvent( servG->worker->el, ms , cb  , params  , NULL );
}


//异步任务进程回调
int taskCallback( char* task_data , int data_len , int taskid , int to  )
{
	aePipeData data = {0};
	data.type = PIPE_EVENT_ATASK;

	asyncTask task;
	task.id = taskid;
	task.to = to;
	task.from = servG->worker->pidx;

	data.len = 0;
	data.len += sizeof( asyncTask );
	data.len += data_len;

	int index;
	int min = servG->worker->pidx * servG->reactorNum;
	index = task.id % servG->reactorNum + min;


	//printf( "TaskWorker Callback pidx=%d,task.id=%d,pipe_index=%d,pipefd=%d,task.to=%d \n" , servG->worker->pidx ,  task.id , index,
	//	servG->worker_pipes[index].pipefd[1],task.to
	// );


	if ( sdslen( servG->worker->send_buffer ) == 0 )
	{
		int ret;
		ret = aeCreateFileEvent( servG->worker->el, servG->worker_pipes[index].pipefd[1] ,
		                         AE_WRITABLE , onWorkerPipeWritable, NULL );
		if ( ret == AE_ERR )
		{
			printf( "Error aeCreateFileEvent..\n");
		}
	}

	appendSendBuffer( &data , PIPE_DATA_HEADER_LENG );
	appendSendBuffer( &task , sizeof( asyncTask ) );
	appendSendBuffer( task_data , data_len );

	return AE_TRUE;
}


//这个callbackfunc地址要记好
int addAsyncTask(  char* params ,  int task_worker_id  )
{
	aePipeData data = {0};
	data.type = PIPE_EVENT_ATASK;

	asyncTask task;
	task.id = servG->worker->task_id + 1;
	task.to = task_worker_id;
	task.from = servG->worker->pidx;

	data.len = 0;
	data.len += sizeof( asyncTask );
	data.len += strlen( params );

	int index;
	int min = servG->worker->pidx * servG->reactorNum;
	index = task.id % servG->reactorNum + min;

	/*
	printf( "data=%s,task.id=%d,pipe_index=%d,pipefd=%d,task.to=%d \n" ,params , task.id , index,
		servG->worker_pipes[index].pipefd[1],task.to
	 );
	*/
	if ( sdslen( servG->worker->send_buffer ) == 0 )
	{
		int ret;
		ret = aeCreateFileEvent( servG->worker->el, servG->worker_pipes[index].pipefd[1] ,
		                         AE_WRITABLE , onWorkerPipeWritable, NULL );
		if ( ret == AE_ERR )
		{
			printf( "Error aeCreateFileEvent..\n");
		}
	}

	appendSendBuffer( &data , PIPE_DATA_HEADER_LENG );
	appendSendBuffer( &task , sizeof( asyncTask ) );
	appendSendBuffer( params , strlen( params ) );
	servG->worker->task_id += 1;

	return servG->worker->task_id;
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

#define BUF_LEN 0xFFFF
int createResponse( int connfd , char* buff , int len , char prototype , sds response )
{
	if ( prototype == HTTP )
	{
		char content_length[64];
		int clen;
		clen = sprintf( content_length , "Content-Length: %d\r\n" , len );
		response = sdscat( response , "HTTP/1.1 200 OK \r\n" );
		response = sdscat( response , "Server: appnet/1.1.0\r\n" );
		response = sdscatlen( response , content_length , clen );
		response = getRespHeaderString( response );
		response = sdscat( response , "\r\n" );
		servG->worker->response = sdscatlen( response ,  buff , len );
		resetRespHeader( servG->worker->resp_header );
		return sdslen( servG->worker->response );
	}
	else
	{
		size_t outlen = BUF_LEN;
		uint8_t res[BUF_LEN];
		wsMakeFrame( buff ,  len , res , &outlen , WS_TEXT_FRAME );
		sdscatlen( servG->worker->response , res , outlen );
		return outlen;
	}
}

//如果不是tcp协议，要单独处理
int sendMessageToReactor( int connfd , char* buff , int len )
{
	if ( isValidConnfd( connfd ) == AE_FALSE )
	{
		return -1;
	}
	aePipeData data;
	data.type = PIPE_EVENT_MESSAGE;
	data.len = len;
	data.connfd = connfd;
	//printf( "sendMessage connfd=%d \n" , connfd );
	int writeable = 0;
	if (sdslen( servG->worker->send_buffer ) == 0  )
	{
		writeable = 1;
	}

	//append data
	if ( len >= 0 )
	{
		//if proto is http or websocket, need compose response data ,else direct send origin data
		if (  servG->worker->proto != TCP )
		{
			char prototype = servG->worker->proto;
			int retlen;
			//buffer...
			retlen = createResponse(  connfd , buff , len , prototype , servG->worker->response  );
			if ( retlen < 0 )
			{
				printf( "CreateResponse Error \n");
				return;
			}

			if ( sdslen( servG->worker->response ) != retlen )
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


		if ( writeable > 0 ||  data.len + PIPE_DATA_HEADER_LENG  == sdslen( servG->worker->send_buffer ) )
		{
			int ret;
			int index = get_worker_pipe_index( connfd );
			ret = aeCreateFileEvent( servG->worker->el,servG->worker_pipes[index].pipefd[1] , AE_WRITABLE ,onWorkerPipeWritable,NULL );
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
	if ( servG->onRecv )
	{
		//将真实数据返回，去除包头
		//如果一个请求是多个包发送过来的，会有问题吗
		servG->onRecv( servG , &servG->connlist[connfd] , buffer , length  );
	}
}

void readTaskFromPipe( int pipe_fd , aePipeData data )
{
	int readlen;
	if ( data.len > 0 )
	{
		char buff[TMP_BUFFER_LENGTH] = {0};
		while ( ( readlen = read( pipe_fd , buff , data.len ) ) > 0 )
		{
			if ( readlen == data.len )
			{
				break;
			}
		}
		asyncTask task;
		char param[1024] = {0};
		memcpy( &task , buff , sizeof( asyncTask ) );
		memcpy( &param, buff + sizeof( asyncTask ) , data.len - sizeof( asyncTask ) );

		//printf( "readTaskFromPipe in task_worker_id=%d,task.id=%d,data.len=%d  \n" , servG->worker->pidx , task.id , data.len  );
		if ( servG->worker->pidx >= servG->workerNum  )
		{
			//in task_worker,onTask
			servG->onTask( param , data.len - sizeof( asyncTask ) , task.id  , task.from  );
		}
		else
		{
			//in worker,onTaskCallback
			//printf( "readTaskFromPipe Callback data=%s \n" , param );
			servG->onTaskCallback( param , data.len - sizeof( asyncTask ) , task.id  , task.from  );
		}
	}
}

void readWorkerBodyFromPipe( int pipe_fd , aePipeData data )
{
	int pos = PIPE_DATA_HEADER_LENG;
	int readlen = 0;
	int needlen = ( data.len > PIPE_DATA_LENG ) ? PIPE_DATA_LENG : data.len;
	int bodylen = 0;
	int readTotal = 0;
	int ret;
	if ( data.len > 0 )
	{
		data.data = sdsempty();
		char buff[TMP_BUFFER_LENGTH];
		while ( ( readlen = read( pipe_fd , buff , data.len-bodylen ) ) > 0  )
		{
			data.data = sdscatlen( data.data , buff , readlen  );
			bodylen += readlen;
			if( bodylen == data.len )break;
		}
	}

	//printf( "data.proto=%d,conn proto=%d \n" , data.proto , servG->connlist[data.connfd].protoType );
	servG->worker->proto = data.proto;
	if ( data.proto == HTTP )
	{
		//printf( "header_len=%d,len=%d \n" , data.header_len , data.len );
		httpHeader* header =  &servG->worker->req_header;
		memset( header , 0 , sizeof( header ));
		header->connfd = data.connfd;

		ret = httpHeaderParse( header , data.data , data.header_len );
		if ( ret < AE_OK )
		{
			printf( "Worker httpHeaderParse Error \n");
		}
		//getHeaderParams( header , " " );
		callUserRecvFunc( data.connfd , data.data + data.header_len , data.len - data.header_len );
	}
	else
	{
		callUserRecvFunc( data.connfd , data.data , bodylen );
	}
}

void onWorkerPipeWritable( aeEventLoop *el, int fd, void *privdata, int mask )
{
	ssize_t nwritten;
	nwritten = write( fd, servG->worker->send_buffer, sdslen(servG->worker->send_buffer));
	if (nwritten <= 0)
	{
		printf( "Worker I/O error writing to worker: %s \n", strerror(errno));
		//退出吗，自杀..
		return;
	}
	//offset
	sdsrange(servG->worker->send_buffer, nwritten, -1);
	///if send_buffer no data need send, remove writable event
	if (sdslen(servG->worker->send_buffer) == 0)
	{
		//printf( "Writing Length=%d....\n" , nwritten );
		aeDeleteFileEvent( el, fd, AE_WRITABLE);
	}
	else
	{
		printf( "Writing Continue....\n");
	}
}

void onWorkerPipeReadable( aeEventLoop *el, int fd, void *privdata, int mask )
{
	aePipeData data;
	int readlen = 0;
	//此处如果要把数据读取到大于包长的缓冲区中，不要用anetRead，否则就掉坑里了
	readlen = read(fd, &data , PIPE_DATA_HEADER_LENG );
	if ( readlen == 0 )
	{
		close( fd );
	}
	else if ( readlen == PIPE_DATA_HEADER_LENG )
	{
		servG->worker->connfd = data.connfd;
		//connect,read,close
		if ( data.type == PIPE_EVENT_CONNECT )
		{
			if ( servG->onConnect )
			{
				servG->onConnect( servG , data.connfd );
			}
		}
		else if ( data.type == PIPE_EVENT_MESSAGE )
		{
			readWorkerBodyFromPipe(  fd , data );
		}
		else if ( data.type == PIPE_EVENT_ATASK )
		{
			readTaskFromPipe( fd , data );
		}
		else if ( data.type == PIPE_EVENT_CLOSE )
		{
			if ( servG->onClose )
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
	int index = get_worker_pipe_index( connfd );
	if (sdslen( servG->worker->send_buffer ) == 0  )
	{
		int ret = aeCreateFileEvent( servG->worker->el,
		                             servG->worker_pipes[index].pipefd[1] , AE_WRITABLE,
		                             onWorkerPipeWritable, NULL );
		if ( ret == AE_ERR )
		{
			printf( "setPipeWritable_error %s:%d \n" , __FILE__ , __LINE__ );
		}
	}
	else
	{
		printf( "send_buffer=%d,len=%d\n", sdslen( servG->worker->send_buffer ) , data.len  );
	}
	servG->worker->send_buffer = sdscatlen( servG->worker->send_buffer , &data, PIPE_DATA_HEADER_LENG );
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
	int index = get_worker_pipe_index( connfd );
	if (sdslen( servG->worker->send_buffer ) == 0  )
	{
		int ret = aeCreateFileEvent( servG->worker->el,
		                             servG->worker_pipes[index].pipefd[1] , AE_WRITABLE,
		                             onWorkerPipeWritable, NULL );
		if ( ret == AE_ERR )
		{
			printf( "setPipeWritable_error %s:%d \n" , __FILE__ , __LINE__ );
		}
	}
	else
	{
		printf( "send_buffer=%d,len=%d\n", sdslen( servG->worker->send_buffer ) , data.len  );
	}
	servG->worker->send_buffer = sdscatlen( servG->worker->send_buffer , &data, PIPE_DATA_HEADER_LENG );
}

int timerCallback(struct aeEventLoop *l, long long id, void *data)
{
	printf("I'm time_cb,here [EventLoop: %p],[id : %lld],[data: %s],pid=%d \n", l, id, data, getpid());
	return 5000;
}
void finalCallback(struct aeEventLoop *l, void *data)
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
	return setRespHeader( key , value );
}



int setRespHeader( char* key , char* val )
{
	if ( strlen( key ) < 0 || strlen( val ) < 0 )
	{
		printf( "Header Error key=%s,val=%s \n" , key , val );
		return AE_FALSE;
	}

	if ( strcasecmp( key , "Server" ) == 0 || strcasecmp( key , "Content-Length" ) == 0 )
	{
		printf( "Header Key Cannot Modify By User \n");
		return AE_FALSE;
	}

	sds skey = sdsnew( key );
	sds sval = sdsnew( val );
	int ret = dictReplace( servG->worker->resp_header, skey,  sval );
	return AE_TRUE;
}

sds getRespHeaderString( sds header )
{
	dictIterator *it = dictGetIterator( servG->worker->resp_header );
	dictEntry *de;
	while ((de = dictNext(it)) != NULL)
	{
		header = sdscatprintf( header , "%s:%s\r\n" , dictGetKey( de ) , dictGetVal( de ) );
	}
	dictReleaseIterator( it );
	return header;
}

void resetRespHeader( dict* resp_header )
{
	dictEmpty( resp_header , NULL );
	//set default header options
	setRespHeader( "Content-Type" , "text/html" );
}

/**
 * process event types:
 * 1,parent process send readable event
 */
void runWorkerProcess( int pidx  )
{
	//servG->worker have private memory space in every process
	aeWorker* worker = zmalloc( sizeof( aeWorker ));
	worker->pid = getpid();
	worker->maxEvent = MAX_EVENT;
	worker->pidx = pidx;
	//worker->pipefd = pipefd;
	worker->running = 1;
	worker->start = 0;
	worker->task_id = 0;
	worker->send_buffer = sdsnewlen( NULL , SEND_BUFFER_LENGTH  );
	worker->recv_buffer = sdsnewlen( NULL , RECV_BUFFER_LENGTH  );
	worker->response = sdsnewlen( NULL , SEND_BUFFER_LENGTH );

	worker->resp_header =  dictCreate(&headerDictType, NULL);
	servG->worker = worker;
	resetRespHeader( worker->resp_header );


	sdsclear( servG->worker->send_buffer );
	sdsclear( servG->worker->recv_buffer );
	sdsclear( servG->worker->response );

	addSignal( SIGTERM, childTermHandler, 0 );
	signal(SIGINT , childTermHandler );
	signal(SIGPIPE, SIG_IGN);

	worker->el = aeCreateEventLoop( worker->maxEvent );
	aeSetBeforeSleepProc( worker->el, initWorkerOnLoopStart );
	int res, index, thid;

	//listen pipe event expect reactor message come in;
	//此处要监听reactor M个pipefd[1]
	for ( thid = 0 ; thid < servG->reactorNum ; thid++ )
	{
		index = pidx * servG->reactorNum + thid;
		res = aeCreateFileEvent( worker->el,
		servG->worker_pipes[index].pipefd[1],
			AE_READABLE,
			onWorkerPipeReadable,NULL
		);
		printf("Worker Run pid=%d, index=%d, pidx=%d and listen pipefd=%d is ok? [%d]\n", 
				getpid() , index, pidx ,servG->worker_pipes[index].pipefd[1],res==0 );
	}

	//res = aeCreateTimeEvent( worker->el, 1000 , timerCallback , NULL , NULL );

	aeMain(worker->el);
	aeDeleteEventLoop(worker->el);
	servG->onFinal( servG );

	//printf( "Worker pid=%d exit...\n" , worker->pid );
	sdsfree( worker->send_buffer );
	sdsfree( worker->recv_buffer );
	sdsfree( worker->response );
	dictRelease( worker->resp_header  );
	zfree( worker );
	shm_free( servG->connlist , 0 );
	//close( pipefd );
	exit( 0 );
}
