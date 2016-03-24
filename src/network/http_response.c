
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include "aeserver.h"
#include "http_response.h"
#include <fcntl.h>
#include "ae.h"

void onRespWritable(  aeEventLoop *el, int connfd, void *privdata, int mask )
{
	httpHeader* reqHeader = ( httpHeader*)privdata;
	//printf( "onRespWritable fd=%d,hfd=%d,mime_type=%s \n" , connfd , reqHeader->connfd,reqHeader->mime_type  );

	http_response_static_proc( reqHeader  );
	aeDeleteFileEvent( el, connfd , AE_WRITABLE );
}

void http_response_static( httpHeader* reqHeader  )
{
	int thid = reqHeader->connfd % servG->reactorNum;
	aeEventLoop *el = servG->reactorThreads[thid].reactor.eventLoop;
	if ( aeCreateFileEvent( el, reqHeader->connfd , AE_WRITABLE , onRespWritable, reqHeader ) == -1)
	{
		return;
	}
}

void http_response_write( int connfd , char* buff , int len )
{
	int nwritten = write( connfd , buff , strlen( buff ));
	if (nwritten <= 0)
	{
		printf( "I/O error writing to client: %s \n", strerror(errno));
		return;
	}
}

header_status_t get_http_status( int status )
{
	header_status_t header_status;
	if( status < http_status_200 || status  > http_status_max )
	{
		return header_status;
	}

	int code_i = status % 100;
	if( status < http_status_300 )
	{
		return http_status_20x[code_i];
	}

	if( status < http_status_400 )
	{
		return http_status_30x[code_i];
	}

	if( status < http_status_500 )
	{
		return http_status_40x[code_i];
	}

	if( status < http_status_max )
	{
		return http_status_50x[code_i];
	}
	return header_status;
}


int is_dir(char *path)
{
	int s;
	struct stat info;
	s = stat(path, &info);
	return (s == 0 && (info.st_mode & S_IFDIR));
}

int is_file(char *path)
{
	int s;
	struct stat info;
	s = stat(path, &info);
	return (s == 0 && (info.st_mode & S_IFREG));
}


int header_buffer_append(  header_out_t* header_out , char* data , int len )
{
	memcpy( header_out->data + header_out->length , data , len  );
	header_out->length += len;
	header_out->count += 1;
}

int resp_append_header( header_out_t*  header_out , int line_type , ...  )
{
	char line[1024] = {0};
	int len = 0;
	char* arg_string;
	int   arg_int;
	va_list ap;
	va_start(ap,line_type);

	switch( line_type )
	{
		case HEADER_STATUS:
			//error_code,
			arg_string = va_arg(ap, char* );
			len = snprintf( line , sizeof( line ) , header_formats[HEADER_STATUS], header_out->req->version , arg_string );
		break;
		case HEADER_SERVER:
			len = snprintf( line , sizeof( line ) , http_server_full_string );
		break;
		case HEADER_CONTENT_TYPE:
			len = snprintf( line , sizeof( line ) , header_formats[HEADER_CONTENT_TYPE] , header_out->req->mime_type );
		break;
		case HEADER_CONTENT_LENGTH:
			//content-length
			arg_int = va_arg( ap, int );
			len = snprintf( line , sizeof( line ) , header_formats[HEADER_CONTENT_LENGTH] , arg_int );
		break;
		case HEADER_LOCATION:
			//content-length
			arg_string = va_arg( ap, char* );
			len = snprintf( line , sizeof( line ) , header_formats[HEADER_LOCATION] , arg_string );
		break;
		case HEADER_KEEP_ALIVE:
			if( header_out->req->keep_alive == 0 )
			{
				len = snprintf( line , sizeof( line ) , header_formats[HEADER_KEEP_ALIVE] , "close" );
			}
			else
			{
				len = snprintf( line , sizeof( line ) , header_formats[HEADER_KEEP_ALIVE] , "keep-alive" );
			}
		break;
		
		case HEADER_END_LINE:
			len = snprintf( line , sizeof( line ) , header_formats[HEADER_END_LINE] );
		break;
		
	}
	header_buffer_append( header_out , line , strlen( line ) );
	return len;
}



//this function only process status=200 page, not include resp_error_page
void create_common_header( header_out_t*  header_out, int status_code   )
{
	header_status_t  error_page = get_http_status( status_code );
	//header append
	resp_append_header( header_out , HEADER_STATUS , error_page.status  );
	resp_append_header( header_out , HEADER_SERVER );
	resp_append_header( header_out , HEADER_CONTENT_TYPE  );
	resp_append_header( header_out , HEADER_KEEP_ALIVE );
}

void http_redirect( httpHeader* reqHeader ,  char* uri )
{
	header_out_t  header_out;
	memset( &header_out , 0 , sizeof( header_out ));
	header_out.req = reqHeader;
    header_status_t  error_page = get_http_status( 301 );        //header append

	create_common_header(  &header_out , 301 );
	resp_append_header( &header_out , HEADER_LOCATION , uri );
	resp_append_header( &header_out , HEADER_CONTENT_LENGTH , 0  );
	resp_append_header( &header_out , HEADER_END_LINE );
	
	http_response_write( reqHeader->connfd , header_out.data , header_out.length );
}

int page_is_defined( char* page )
{
	char path[128] = {0};
	get_file_path( page , path );
	if( is_file( path ) )
	{
	  return 1;
	}
	printf( "%s Is Not Defined Page \n" , path );
	return 0;
}

//
int resp_defined_error_page( header_out_t*  header_out , int err_code )
{	
	switch( err_code )
	{
		case 404:
			if( page_is_defined( servG->http_404_page ) == 0 )return 0;
			http_redirect( header_out->req , servG->http_404_page );
			break;
		case 500:
		case 501:
		case 502:
		case 503:
		case 504:
		case 505:
			if( page_is_defined( servG->http_50x_page ) == 0 )return 0;
			http_redirect( header_out->req , servG->http_50x_page );
			break;
		default:
			return 0;
		
	}
	return 1;
}

//如果内容是固定长度的，推荐用这种方式,如果是很大块的内容也可以用trucked方式
void header_append_length(  header_out_t*  header_out , int len )
{
	resp_append_header( header_out , HEADER_CONTENT_LENGTH  , len );
}

//如果内容是动态生成的数据流,则需要分段返回给客户端
void header_append_chunked(  header_out_t  header_out )
{
	//resp_append_header( header_out , HEADER_CONTENT_TYPE  );
	//resp_append_header( header_out , HEADER_CONTENT_LENGTH  , datalen );
}


//404
void resp_error_page( header_out_t*  header_out, int status_code )
{
	if( status_code >= 400 && status_code <= 507 )
	{
		int ret = resp_defined_error_page(  header_out , status_code );
		if( ret == 1 )
		{
			return;
		}
	}
	
	//get header info
	header_status_t  error_page = get_http_status( status_code );
	int datalen = strlen( error_page.data );
	
	//header append
	//resp_append_header( header_out , HEADER_STATUS , error_page.status  );
	//resp_append_header( header_out , HEADER_SERVER );
	
	create_common_header( header_out , error_page.status );
	header_append_length( header_out , datalen );
	resp_append_header( header_out , HEADER_END_LINE );
	
	//send
	http_response_write( header_out->req->connfd, header_out->data , header_out->length );
	http_response_write( header_out->req->connfd, error_page.data , datalen );
}


//response static resource
void http_response_static_proc( httpHeader* reqHeader )
{
	int len, cllen , ctlen ;
	char path[1024];
	header_out_t  header_out;
	memset( &header_out , 0 , sizeof( header_out ));
	header_out.req = reqHeader;
	
	
    get_file_path( reqHeader->uri , path );
	struct stat stat_file;
    int ret =  stat( path , &stat_file );
	

	if( ret < 0 )
	{
		resp_error_page( &header_out , 404 );
	   	return;
	}

	create_common_header( &header_out , 200 );
	header_append_length( &header_out , stat_file.st_size );
	resp_append_header( &header_out , HEADER_END_LINE );
	
	int nwritten = write( reqHeader->connfd , header_out.data , header_out.length );
	if (nwritten <= 0)
	{
		printf( "I/O error writing to client: %s \n", strerror(errno));
		return;
	}

    int fd = open( path , O_RDONLY );
	if( fd < 0 )
	{
	 	printf( "Open file Error:%s,errno=%d \n" , strerror(errno) , errno );
		return;
	}

	off_t offset = 0;
	while( offset < stat_file.st_size  )
	{	
		int sendn = sendfile( reqHeader->connfd , fd , &offset , stat_file.st_size - offset );	
		if( sendn < 0 )
		{
		   if(errno == EAGAIN)continue;
		   if(errno == EINTR)break;
		}
	}
	close( fd );
}


void get_file_path( char* uri , char* path )
{
	char* pos  = strstr( uri , "?" );
	memcpy( path ,  servG->http_docs_root , strlen( servG->http_docs_root ) );
	if( pos == NULL )
	{
	   memcpy( path + strlen( servG->http_docs_root )  ,  uri , strlen( uri)  );
	}
	else
	{
	   memcpy( path + strlen( servG->http_docs_root ) ,  uri , pos-uri  );	
	}
}






