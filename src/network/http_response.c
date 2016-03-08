

#include "http_response.h"

//response static resource
void http_response_static( httpHeader* reqHeader , char* mime_type )
{

	int cllen , ctlen ;
	char path[1024];
	get_file_path( reqHeader->uri , path );
	
	printf( "http_response_static path=%s \n" , path );
	return;
	
	commonResponse  resp;
	sprintf( resp.hRespCode , 	"%s 200 OK \r\n", 			reqHeader->version );
	sprintf( resp.hServerName , 	"Server: appnet/1.0.0\r\n"  );
	sprintf( resp.hContentLength , 	"Content-Length: %d\r\n", 	len );	
	sprintf( resp.hContentType , 	"Content-Type: %s\r\n", 	mime_type );
	
	
	char response[1024] = {0};
	memcpy( response , resp.hRespCode , 	strlen( resp.hRespCode ));
	memcpy( response , resp.hServerName , 	strlen( resp.hServerName ));
	memcpy( response , resp.hContentType , 	strlen( resp.hContentType ));
	memcpy( response , resp.hContentLength , strlen( resp.hContentLength ));
	
	
	response = sdscat( response , "\r\n" );
	
	//send header 
	
	
	//sendfile
	
	
	
}


void get_file_path( char* uri , char* path )
{
	char* pos  = strstr( uri , "?" );
	memcpy( path ,  servG->httpDocsRoot , strlen( servG->httpDocsRoot ) );
	memcpy( path+strlen( servG->httpDocsRoot ) ,  uri , pos-uri  );
	
}




int get_current_path( char* current_path , int len )
{
	char* ret = getcwd( current_path , len );
	if( ret == NULL )
	{
		printf( "Current Path Too Long\n"  );
		return 0;
	}
	return 1;
}


//get file pointer by uri
void get_file_info( char* uri , char* fp , int len )
{
	
}


void sendfile()
{
	
}


