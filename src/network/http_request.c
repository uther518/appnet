
#include "aeserver.h"
#include "zmalloc.h"
#include "http_request.h"
#include "mime_types.h"
#include <string.h>
#include <sys/time.h>
#include "dict.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/********
    [Protocol] => HTTP
    [Method] => GET
    [Uri] => /domain
    [Version] => HTTP/1.1
    [Host] => 192.168.171.129:3011
    [User-Agent] => Mozilla/5.0 (Windows NT 6.1; WOW64; rv:43.0) Gecko/20100101 Firefox/43.0
    [Accept] => text/html,application/xhtml+xml,application/xml;q=0.9;q=0.8
    [Accept-Language] => zh-CN,zh;q=0.8,en-US;q=0.5,en;q=0.3
    [Accept-Encoding] => gzip, deflate
    [Sec-WebSocket-Version] => 13
    [Origin] => null
    [Sec-WebSocket-Extensions] => permessage-deflate
    [Sec-WebSocket-Key] => kzxlJW/Ny8o2BF6R6konUA==
    [Cookie] => _ga=GA1.1.394174278.1454081401
    [Connection] => keep-alive, Upgrade
    [Pragma] => no-cache
    [Cache-Control] => no-cache
    [Upgrade] => websocket
----------------------------------------------

	

**********/

static char* findEolChar( const char* s , int len )
{
    char *s_end, *cr , *lf;
    s_end = s + len;
    while( s < s_end )
    {
        //每次循环查找128bytes
        size_t chunk = ( s + CHUNK_SZ < s_end ) ? CHUNK_SZ : ( s_end - s );
        cr = memchr( s , '\r' , chunk );
        lf = memchr( s , '\n' , chunk );
        if( cr )
        {
            if( lf && lf < cr )
            {
                return lf;  //xxxxx\n\rcccccc           lf:xxxxx
            }
            return cr;      //xxxxx\r\ncccccc\r\n       cr:xxxxx
        }
        else if( lf )
        {
            return lf;      //xxxxx\ncccccccccc\r\n     lf:xxxxx
        }
        s += CHUNK_SZ;
    }
    return NULL;
}


//获取字符串左边"\r","\n"," "个数。
static int getLeftEolLength( const char* s )
{
    int i,pos=0;
    for( i = 0; i<strlen( s ); i++ )
    {
        if( memcmp( "\r" , s+i , 1 ) == 0 ||  memcmp( "\n" , s+i , 1 )==0 ||   memcmp( " " , s+i , 1 )==0 )
        {
            pos++;
        }
        else
        {
            return pos;
        }
    }
    return pos;
}


//返回的是在buffer中的偏移量
int bufferLineSearchEOL( httpHeader* header , const char* buffer , int len , char* eol_style )
{
    //先清空左边空格
    //header->buffer_pos += getLeftEolLength( buffer );
    char* cp = findEolChar( buffer , len );
    int offset = cp - buffer;
    if( cp && offset > 0 )
    {
        //header->buffer_pos += offset;
        return offset;
    }
    return AE_ERR;
}


//从上次取到的位置，在剩余的buffer长度中查找，以\r\n结尾读取一行
int bufferReadln( httpHeader* header , const char* buffer , int len , char* eol_style )
{
    int read_len,offset,eol;
    eol = 0;
    eol= getLeftEolLength( buffer+header->buffer_pos  );
    if( eol >= strlen( AE_HEADER_END ) )
    {
        //header end..
        if( memcmp( AE_HEADER_END , buffer+header->buffer_pos, strlen( AE_HEADER_END )  ) == 0)
        {
            return AE_OK;
        }
    }
    header->buffer_pos += eol;
    read_len = len - header->buffer_pos;
    offset = bufferLineSearchEOL( header , buffer+header->buffer_pos , read_len , eol_style );
    if( offset < 0 )
    {
        return AE_ERR;
    }
    //表示buffer的起始位置
    return offset;
}

char* findChar(  char sp_char , const char* dest , int len );
//return char* point to first space position in s;
//在s中查找，最多找len个长度s
char* findSpace(  const char* s , int len )
{
    return findChar( AE_SPACE , s , len );
}

char* findChar(  char sp_char , const char* dest , int len )
{
    char *s_end, *sp;
    s_end = dest + len;
    while( dest < s_end )
    {
        //每次循环查找128bytes
        size_t chunk = ( dest + CHUNK_SZ < s_end ) ? CHUNK_SZ : ( s_end - dest );
        sp = memchr( dest , sp_char , chunk );
        if( sp )
        {
            return sp;      //xxxxx\r\ncccccc\r\n       cr:xxxxx
        }
        dest += CHUNK_SZ;
    }
    return NULL;
}

/*
by RFC2616
http://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html#sec5.1
The Request-Line begins with a method token, followed by the Request-URI and the protocol version, and ending with CRLF.
The elements are separated by SP characters. No CR or LF is allowed except in the final CRLF sequence.
Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
*/
static int parseFirstLine( httpHeader* header , const char* buffer , int len )
{
    int offset;
    offset = bufferReadln( header , buffer , len , AE_EOL_CRLF );
    if( offset < AE_OK )
    {
        //error means header uncomplate, or a wrong header.
        return AE_ERR;
    }
    //first line length except CRLF
    header->buffer_pos += offset;
    //find SPACE pos in ( buffer , buffer + header->buffer_pos );
    char* space;
    int find_count = 0;
    int pre_length = 0;
    int section = HEADER_METHOD;
    bzero( header->method , sizeof( header->method ) );
    bzero( header->uri,sizeof( header->uri ) );
    bzero( header->version ,sizeof( header->version ) );
    //因为三段只有两个空格啊，所以找两次就可以了。
    while( section < HEADER_VERSION )
    {
        //因为是第一行，从开头到行末。
        space = findSpace( buffer + find_count , offset );
        if( space == NULL )
        {
            return AE_ERR;
        }
        pre_length += find_count;
        //本次找到了几个字符，这次找到在buffer中的位置-上次找到的位置
        find_count = space-(buffer + find_count);
        if( section == HEADER_METHOD )
        {
            memcpy( header->method , buffer , find_count );
        }
        else if( section == HEADER_URI )
        {
            memcpy( header->uri, buffer+pre_length , find_count );
            break;
        }
        section++;
        find_count++;//加1是因为要去除掉一个空格的位置
    }
    memcpy( header->version, space+1 , header->buffer_pos -( space-buffer)-1 );
    return AE_OK;
}

static int  readingHeaderFirstLine( httpHeader* header , const char* buffer , int len )
{
    return  parseFirstLine(  header , buffer , len );
}


static int readingHeaders( httpHeader* header , const char* buffer , int len )
{
    int ret,end,offset;
    headerFiled filed;
    end = 0;
    while( end == 0 )
    {
        offset = bufferReadln( header , buffer , len , AE_EOL_CRLF );
        if( offset < AE_OK )
        {
            return AE_ERR;
        }
        if( offset == AE_OK )
        {
            break;
        }
        ret = readingSingleLine( header , buffer + header->buffer_pos , offset );
        if( ret < AE_OK )
        {
            return AE_ERR;
        }
        //如果解析好了，指针往后移
        header->buffer_pos += offset;
    };
    header->buffer_pos += strlen( AE_HEADER_END );
    return AE_OK;
}

int readingSingleLine(  httpHeader* header , const char* org , int len )
{
    char* ret;
    int value_len = 0;
    ret = findChar( ':' , org , len );
    if( ret == NULL )
    {
        if(  header->filed_nums <= 0 )
        {
            return AE_ERR;
        }
        //放在上一个域的值里
        //header->fileds[header->filed_nums-1].value.str_len += len;
        memcpy( header->fileds[header->filed_nums-1].value , org , len   );
        return AE_OK;
    }
    //org~ret :key   ret+1~org+len: value
    memcpy(header->fileds[header->filed_nums].key , org ,  ret-org );
    //Content-Length
    if( memcmp( org  , "Content-Length" ,  ret-org  ) == 0 )
    {
        header->content_length = -2;
    }
    //"Upgrade"
    if( memcmp( org  , "Upgrade" ,  ret-org  ) == 0 )
    {
        header->protocol = WEBSOCKET;
    }
	
	//multipart/form-data
    value_len = len - ( ret - org ) - 1;//:
    int eolen=0;
    eolen = getLeftEolLength( ret + 1 );
    memcpy( header->fileds[header->filed_nums].value , ret+eolen+1  ,  value_len-eolen  );
    if(  header->content_length == -2 )
    {
        header->content_length = atoi( header->fileds[header->filed_nums].value );
    }

	if( memcmp( org  , "Connection" ,  ret-org  ) == 0 )
    {
		header->keep_alive = 0;
		if( strstr( header->fileds[header->filed_nums].value , "keep-alive" ))
		{
			header->keep_alive = 1;
		}
    }
	
	if( memcmp( org  , "Transfer-Encoding" ,  ret-org  ) == 0 )
    {
		header->trunked = 0;
		if( strstr( header->fileds[header->filed_nums].value , "chunked" ))
		{
			header->trunked = 1;
		}
    }
	
    char* mfd = "multipart/form-data";
    if( memcmp( header->fileds[header->filed_nums].value , mfd ,  strlen( mfd )  ) == 0 )
    {
		int pregs = sscanf( header->fileds[header->filed_nums].value + strlen( mfd )  , "%*[^=]=%s" , header->boundary  );		
			if( pregs == 1 )
		{
		   header->multipart_data = MULTIPART_TYPE_FORM_DATA;
		}
	}
	
    header->filed_nums += 1;
    return AE_OK;
}


static char* getHeaderParams(  httpHeader* header , char* pkey )
{
    int i;
    for( i = 0 ; i < header->filed_nums ; i++ )
    {
        if( 1 ||  memcmp( header->fileds[i].key  , pkey , strlen( header->fileds[i].key ) ) == 0 )
        {
            printf( "key==[%s],value=[%s]\n" , header->fileds[i].key , header->fileds[i].value );
        }
    }
    return "";
}


static int httpHeaderParse( httpHeader* header ,  sds buffer , int len )
{
    header->buffer_pos = 0;
    header->filed_nums = 0;
    int ret = 0;
    ret = readingHeaderFirstLine( header , buffer , len );
    if( ret < AE_OK )
    {
        return AE_ERR;
    }
    ret = readingHeaders( header , buffer , len );
    if( ret < AE_OK )
    {
        return AE_ERR;
    }
    //getHeaderParams( header , " " );
    return AE_OK;
}


int isHttpProtocol( char* buffer , int len )
{
    char* httpVersion = "HTTP";
    strstr( buffer,httpVersion );
    if( strncmp( buffer , "GET" , 3 ) == 0 )
    {
        return AE_TRUE;
    }
    if( strncmp( buffer , "POST" , 4 ) == 0 )
    {
        return AE_TRUE;
    }
    return AE_FALSE;
}

void parsePostRequest( httpHeader* header , sds buffer , int len  )
{
	if( header->multipart_data >= MULTIPART_TYPE_FORM_DATA )
	{
		parse_multipart_data( header , buffer , len );
	}
	else
	{
		createWorkerTask(  header->connfd ,  buffer+header->buffer_pos , header->content_length , PIPE_EVENT_MESSAGE , "parsePostRequest" );
	}
}

int parse_trunked_body( httpHeader* header , sds buffer  )
{
	
	return 1;
}


static int httpBodyParse( httpHeader* header , sds buffer , int len )
{
    if( strncmp( header->method , "POST" , 4 ) == 0 )
    {
        //判断包体是否完整
        //包的总长-包头 < content_length,半包
        //包的总长-包头 > content_length,粘包
	if( header->content_length > 0 )
        {
            //半包
            if(  sdslen( buffer ) - header->buffer_pos < header->content_length )
            {
	           return CONTINUE_RECV;
            }
            //粘包或完整包
            else
            {
                header->complete_length = header->buffer_pos + header->content_length;
		//parsePostRequest(  header , data  , header->content_length );   
		parsePostRequest(  header , buffer , header->complete_length );
                return BREAK_RECV;
            }
        }
        else if( header->trunked == 1 )//trunk模式
        {
            	printf( "Http trunk body,Not Support ....\n" );
		//parse_trunked_body( header , buffer );
            	return BREAK_RECV;
        }
	else
	{
		printf( "Bad Header,Unkown Length body \n" );
            	return BREAK_RECV;
	}
    }
    else if( strncmp( header->method , "GET" , 3 ) == 0  )
    {
        char* uri;
        uri = strstr( header->uri , "?" );
	int ret;
	memset( header->mime_type , 0 , sizeof( header->mime_type ) );
	ret = get_mime_type( header->uri , header->mime_type );
	if( ret == 1 )
	{
		http_response_static( header );
		return BREAK_RECV;
	}
		
        if( uri != NULL  )
        {
            createWorkerTask(  header->connfd , uri+1 , strlen( uri) - 1 , PIPE_EVENT_MESSAGE, "parseGetRequest" );
        }
        else
        {
            createWorkerTask(  header->connfd , "",  0 , PIPE_EVENT_MESSAGE , "parseGetRequest empty data" );
        }
    }
    return BREAK_RECV;
}

int httpRequestParse(  int connfd , sds buffer , int len  )
{
    int ret = 0;
    int thid =  connfd%servG->reactorNum;
    httpHeader* header = servG->reactorThreads[thid].hh;
    memset( header , 0 , sizeof( header ));
    header->connfd = connfd;
    printf( "httpHeaderParse start buffer=[%s] \n" , buffer );

    ret = httpHeaderParse( header , buffer , sdslen( buffer ) );
    if( ret < AE_OK  )
    {
        if( header->protocol == WEBSOCKET )
        {
            servG->reactorThreads[thid].hs->frameType = WS_INCOMPLETE_FRAME;
        }
        return CONTINUE_RECV;
    }
    if( header->protocol != WEBSOCKET )
    {
        header->protocol = HTTP;
        servG->connlist[connfd].protoType = HTTP;
    }
    else
    {
        servG->connlist[connfd].protoType = WEBSOCKET;
    }
    //if body not complete need return to contuine recv;
    //need move to response case
    if( header->protocol == HTTP )
    {
        ret = httpBodyParse( header  , buffer , sdslen( buffer ) );
    }
    else
    {
        //websocket 
	 ret = wesocketRequestRarse( connfd, buffer , len , header , servG->reactorThreads[thid].hs  );
    }
    return ret;
}

enum wsFrameType parseHandshake( httpHeader* header  ,  handshake* hs  )
{
    int i;
    int count = 0;
    for( i = 0 ; i < header->filed_nums ; i++ )
    {
        //Sec-WebSocket-Key
        if( memcmp( header->fileds[i].key , "Sec-WebSocket-Key" ,  strlen( header->fileds[i].key ) ) == 0 )
        {
            memcpy( hs->key , header->fileds[i].value , strlen( header->fileds[i].value ) );
            count++;
        }
        //Sec-WebSocket-Version
        if( memcmp( header->fileds[i].key  , "Sec-WebSocket-Version" , strlen( header->fileds[i].key ) ) == 0 )
        {
            memcpy( hs->ver , header->fileds[i].value ,  strlen ( header->fileds[i].value  ) );
            count++;
        }
        //Sec-WebSocket-Extensions
        if( memcmp( header->fileds[i].key , "Sec-WebSocket-Extensions" , strlen(  header->fileds[i].key ) ) == 0 )
        {
            memcpy( hs->key_ext , header->fileds[i].value , strlen(  header->fileds[i].value )  );
            count++;
        }
    }
    //说明包头不全
    if( count != 3  )
    {
        hs->frameType = WS_ERROR_FRAME;
    }
    else
    {
        hs->frameType = WS_OPENING_FRAME;
    }
    return hs->frameType;
}


#define BUF_LEN 0xFFFF
int wesocketRequestRarse( int connfd , sds buffer , int len , httpHeader* header ,  handshake* hs )
{
    if( hs->state == WS_STATE_OPENING )
    {
        //websocket握手
        hs->frameType = parseHandshake( header , hs );
        header->complete_length = header->buffer_pos;
    }
    else
    {
        sds recv_data = sdsempty();
        size_t recv_len = 0;
        hs->frameType = wsParseInputFrame( buffer, len , &recv_data , &recv_len );
     
        if( recv_len <= 0  )
        {
            return CONTINUE_RECV;
        }
        header->complete_length = len;
        //make sure recv data is complete, then create task..
        createWorkerTask(  connfd , recv_data  , recv_len  , PIPE_EVENT_MESSAGE, "parseWebsocket" );
        //sdsfree( recv_data );
    }
    
    if( hs->frameType == WS_ERROR_FRAME ||  hs->frameType == WS_INCOMPLETE_FRAME )
    {
        printf( "InitHandshake Error Frame..\n");
        return CONTINUE_RECV;
    }
    
    if ( hs->state == WS_STATE_OPENING)
    {
        assert( hs->frameType == WS_OPENING_FRAME);
        if ( hs->frameType == WS_OPENING_FRAME)
        {
            uint8_t out_buffer[BUF_LEN];
            size_t frameSize = BUF_LEN;
            memset( out_buffer, 0, BUF_LEN);
            wsGetHandshakeAnswer( hs, out_buffer , &frameSize , hs->ver );
            if ( sdslen( servG->connlist[connfd].send_buffer ) == 0  )
            {
                int reactor_id = connfd % servG->reactorNum;
                aeEventLoop* el = servG->reactorThreads[reactor_id].reactor.eventLoop;
                aeCreateFileEvent(
                    el, connfd, AE_WRITABLE, onClientWritable, NULL
                );
            }
            servG->connlist[connfd].send_buffer = sdscatlen(
            servG->connlist[connfd].send_buffer ,(char*)out_buffer , frameSize
                                                  );
            //createWorkerTask( connfd , "" , 0 , PIPE_EVENT_CONNECT , "Websocket New Connection" );
        }
        hs->state = WS_STATE_NORMAL;
        return BREAK_RECV;
    }
    //recv websocket normal data
    else
    {
        return BREAK_RECV;
    }
}


char* get_filename(char*ptr,int n)
{
	int i = n;
	ptr+=n;
	while( i-- > 0)
	{
		if((*ptr--)=='/')
		{
				ptr +=2;
				break;
		}
	}
	return ptr;
}


int dir_exist(char *path)
{
	int s;
	struct stat info;
	s = stat(path, &info);
	return (s == 0 && (info.st_mode & S_IFDIR));
}

void put_upload_file( int connfd, char* filename , char* data , int len, char* destfile ) 
{
    if( servG->http_upload_dir )
    {
		if( dir_exist( servG->http_upload_dir ) == 0 )
		{
			printf( "Upload Dir Not Exist,Path=%s \n" , servG->http_upload_dir );
			return;
		}      
    }

    struct timeval tv;
    gettimeofday (&tv , NULL );
    snprintf( destfile , 255 , "%s/%d_%d_%d_%s" , servG->http_upload_dir , tv.tv_sec , tv.tv_usec , connfd , filename );
    
    int fd,size;
    fd = open( destfile  , O_WRONLY|O_CREAT  );
    if( fd < 0 )
    {
		printf( "Upload File Error,Cannot Open the file %s \n" , filename );
		return;
    }

    size = write( fd, data , len );
    if( size < 0 )
    {
		printf( "Upload File Error,Write Error errno=%d,error=%s \n" , errno , strerror( errno ));
    }
    close(fd);
}
/*
parse http mutipart data, mutipart types include
multipart/alternative 
multipart/byteranges
multipart/digest 
multipart/form-data 
multipart/mixed
multipart/parallel
multipart/related 
Content-Type:multipart/form-data; boundary=----WebKitFormBoundaryqQDPjyh5uVhzQQF1
*/
void print_asc( char* data , int len  )
{
    int i = 0;
    for( i =0; i < len ; i++ )
    {
		printf( " %d \n" , data[i] );
    }
}

static char*  binstrstr (const char * haystack, size_t hsize, const char* needle, size_t nsize)
{
    size_t p;
    if (haystack == NULL) return NULL;
    if (needle == NULL) return NULL;
    if (hsize < nsize) return NULL;
    for (p = 0; p <= (hsize - nsize); ++p) {
        if (memcmp(haystack + p, needle, nsize) == 0) {
            return (char*) (haystack + p);
        }
    }
    return NULL;
} 

void parse_multipart_form( httpHeader* header , sds buffer , int len )
{
	char* buff = buffer;
	int vlen;
    	char *crlf = 0 , *cl = 0;
	char key[255] = {0};
	char* filename;
	char filepath[255] = {0};

    	int sep_len = strlen( "\r\n--") + strlen( header->boundary );
	char sep[sep_len+1];
	snprintf( sep , sizeof( sep ) , "\r\n--%s" , header->boundary  );
    	int pos = sep_len;	

	//设置栈大小ulimit -s
	sds data = sdsnewlen(  0 , TMP_BUFFER_LENGTH );
	sdsclear( data  );
	//sds data = sdsempty();
	//char data[1024] = {0};
	while( 1 )
	{
		memset( key , 0 , sizeof( key ));
		//memset( filename , 0 , sizeof( filename ));		
		memset( filepath , 0 , sizeof( filepath ));
		if( pos + 2 >= header->content_length )break;

		sscanf( buffer+pos,
		"Content-Disposition: form-data; name=\"%255[^\"]\"; filename=\"%255[^\"]\"",
		key, filepath );

		filename = get_filename( filepath , strlen( filepath ));

		crlf = strstr((char *) buffer+pos , AE_HEADER_END );
		if( crlf == NULL)return;
		//cl = strstr( crlf , sep );
		cl = binstrstr( crlf, header->content_length - pos , sep , sep_len );
		if( cl == NULL )
		{
			printf( "Form Data One Part Is NULL \n");
			return;
		}
		vlen = cl - crlf;
		pos += cl - ( buffer+pos ) + sep_len +2;
		vlen -= strlen( "\r\n--" );   

		if( strlen( filename) )
		{
			//filename ,data
			printf( "Vlen=%d \n" , vlen );
			char destfile[255] = {0};

			put_upload_file( header->connfd , filename , crlf + strlen( AE_HEADER_END ) , vlen , &destfile );
			data = sdscatprintf( data , "%s=org_file:%s;dest_file:%s;size=%d&" , 
			key , filename , destfile , vlen  );
		}
		else
		{
			data = sdscatprintf( data , "%s=" , key  );//key
			data = sdscatlen( data , crlf + strlen( AE_HEADER_END ) , vlen);//val
			data = sdscat( data , "&" );
		}
	}
	createWorkerTask(  header->connfd , data  , sdslen( data )  , PIPE_EVENT_MESSAGE, "parse_multipart_form" );
//	printf( "data[%d][%s] \n" , sdslen( data ), data );
	sdsfree( data );
}

void parse_multipart_data( httpHeader* header , sds buffer , int len )
{
	if( header->multipart_data == MULTIPART_TYPE_FORM_DATA )
	{
		printf( "multipart_data[%d][%d],content-length=%d \n" , 
		header->buffer_pos, len , header->content_length  );
		//	char boundary[64];
		parse_multipart_form( header , buffer + header->buffer_pos , header->content_length );	
	}
	else
	{
		printf( "Not Support Mutipart Data:%d \n" , header->multipart_data );
	}	
}
