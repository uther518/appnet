
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include "aeserver.h"
#include "http_response.h"
#include <fcntl.h>
#include "ae.h"

void http_response_proc( httpHeader* reqHeader , char* mime_type );
void onRespWritable(  aeEventLoop *el, int connfd, void *privdata, int mask )
{
	httpHeader* reqHeader = ( httpHeader*)privdata;
	//printf( "onRespWritable fd=%d,hfd=%d,mime_type=%s \n" , connfd , reqHeader->connfd,reqHeader->mime_type  );

	http_response_proc( reqHeader , reqHeader->mime_type );
	aeDeleteFileEvent( el, connfd , AE_WRITABLE );
}

void http_response_static( httpHeader* reqHeader  )
{
        int thid = reqHeader->connfd % servG->reactorNum;
        aeEventLoop *el = servG->reactorThreads[thid].reactor.eventLoop;
        if ( aeCreateFileEvent( el, reqHeader->connfd , AE_WRITABLE , onRespWritable, reqHeader ) == -1)
        {
            printf( "aeApiAddEvent Error fd=[%d] \n" , reqHeader->connfd );
            return;
        }
}


void http_response_write( int connfd , char* buff )
{
	int nwritten = write( connfd , buff , strlen( buff ));
	if (nwritten <= 0)
	{
		printf( "I/O error writing to client: %s \n", strerror(errno));
		return;
	}
}

void http_redirect( httpHeader* reqHeader ,  char* uri )
{
        char response[1024] = {0};
        snprintf( response , sizeof( response ) ,
                "%s 301 Moved Permanently\r\nServer: %s\r\nLocation: %s\r\nContent-Length: 0\r\nContent-type: text/html\r\n\r\n" ,
                reqHeader->version,servG->httpHeaderVer,uri
        );
        http_response_write( reqHeader->connfd , response );
}

//redirect 302
void http_response_404(  httpHeader* reqHeader , int redirect )
{
	if( redirect > 0 )
	{
		http_redirect( reqHeader , "404.html" );
		//if 404 page exit, redirect 404.html page
		return;
	}
	char response[1024] = {0};
	char* data = "<html><title>File Not Found</title><body><h1>File Not Found!</h1></body></html>";
	snprintf( response , sizeof( response ) , 
		"%s 404 File Not Found\r\nServer: %s\r\nContent-Length: %d\r\nContent-type: text/html\r\n\r\n%s" , 
		reqHeader->version,servG->httpHeaderVer,strlen( data ),data
	);
	http_response_write( reqHeader->connfd , response );
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

static int resp_append_header_line( header_out_t  header_out , int line , int argi   )
{
	char line[1024] = {0};
	int len;
	
	switch( header_out.curr_line )
	{
		case HEADER_STATUS:
			char* status = get_http_status( argi );
			len = snprintf( line , sizeof( line ) , header_formats[HEADER_STATUS], header_out.reqHeader->version , status );
		break;
		case HEADER_SERVER:
			len = snprintf( line , sizeof( line ) , http_server_full_string );
		break;
		case HEADER_CONTENT_TYPE:
			len = snprintf( line , sizeof( line ) , header_formats[HEADER_CONTENT_TYPE] , header_out.reqHeader->mime_type );
		break;
		case HEADER_CONTENT_LENGTH:
			len = snprintf( line , sizeof( line ) , header_formats[HEADER_CONTENT_LENGTH] , header_out.reqHeader->mime_type );
		break;
		
	}
	
	header_buffer_append( header_out , line , strlen( line ) );
}

void resp_error_page( httpHeader* reqHeader , int err_code , char* err_page )
{
	header_out_t  header_out;
	memset( &header_out , 0 , sizeof( header_out ));
	header_out.req = reqHeader;
	
	resp_append_header_line( header_out , HEADER_STATUS , err_code  );
	resp_append_header_line( header_out , HEADER_SERVER , 0  );
	
	
	http_response_write( header_out.data );
	http_response_write( err_page );
}


//response static resource
void http_response_proc( httpHeader* reqHeader , char* mime_type )
{
	int cllen , ctlen ;
	char path[1024];
	int len;
    get_file_path( reqHeader->uri , path );
	struct stat stat_file;
    int ret =  stat( path , &stat_file );
	
	if( ret < 0 )
	{
		resp_error_page( reqHeader , 404 , http_error_404_page );
	   	//send 404.
		//http_response_404( reqHeader , 0 );
	   	return;
	}

	char response[1024] = {0};
	snprintf( response , sizeof( response ) , 
		"%s 200 OK \r\nServer: %s\r\nContent-Length: %d\r\nContent-Type: %s\r\n\r\n" , 
		reqHeader->version,
		servG->httpHeaderVer,
		stat_file.st_size,mime_type
	);

	
	int nwritten = write( reqHeader->connfd , response , strlen( response ));
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
}


void get_file_path( char* uri , char* path )
{
	char* pos  = strstr( uri , "?" );
	memcpy( path ,  servG->httpDocsRoot , strlen( servG->httpDocsRoot ) );
	if( pos == NULL )
	{
	   memcpy( path + strlen( servG->httpDocsRoot )  ,  uri , strlen( uri)  );
	}
	else
	{
	   memcpy( path + strlen( servG->httpDocsRoot ) ,  uri , pos-uri  );	
	}
}





