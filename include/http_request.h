


#ifndef __HTTP_REQUEST_H_
#define __HTTP_REQUEST_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "websocket.h"


#define TCP 0
#define HTTP 1
#define WEBSOCKET 2

#define AE_SPACE       ' '
#define AE_EOL_CRLF        "\r\n"
#define AE_HEADER_END	   "\r\n\r\n"

#define AE_HTTP_HEADER_MAX_SIZE          8192

//http://www.w3.org/Protocols/rfc2616/rfc2616.html
//响应完后，再将包头与包体从recv_buffer中移除
//查找eol在字符串中的位置，找不到就继续接收。
//如果header太长，返回错误，断开

//从s开头，在len个长度查找，每次循环最多找128bytes

typedef struct
{
  char* pos;
  int   len;
}headerString;

typedef struct
{
	headerString key;
	headerString val;
}headerFiled;

//会把一些长见的option例举在这里，如果没有的，可以去fileds里去找,提高访问效率。
enum
{
	HEADER_METHOD = 1,
	HEADER_URI,
	HEADER_VERSION
};

typedef struct
{
	char method[16];
	char version[16];
	char uri[AE_HTTP_HEADER_MAX_SIZE];
}headerParams;

enum
{
	MULTIPART_TYPE_FORM_DATA = 1,
	MULTIPART_TYPE_BYTERANGES,
	MULTIPART_TYPE_ALTERNATIVE,
	MULTIPART_TYPE_DIGEST,
	MULTIPART_TYPE_MIXED,
	MULTIPART_TYPE_PARALLED,
	MULTIPART_TYPE_RELATED
};

typedef struct
{
	int connfd;
	int header_length;//header部分总长
	int content_length;//body长
	int complete_length; //整包长
	int multipart_data;
	int nobody;	//是否需要返回包体,比如HEAD不需要
	int keep_alive;
	int trunked;
	int filed_nums; //headerFiled numbers
	int buffer_pos; //解析的位置,作为起始位置
	char method[8]; //
	char uri[1024];
	char version[16];
	char mime_type[32];
	char boundary[64];
	int  protocol;
	int  major;
	int  minor;
	//list* out;
	
	headerFiled fileds[20]; 
	headerParams params;   //分析的结果结构体

}httpHeader;


typedef enum
{
	HRET_OK = 0,
	HRET_NOT_HTTP_REQUEST,
	HRET_UNCOMPLETE
}HttpRequestErrorType;	

#define CHUNK_SZ 128

int wesocketRequestRarse( int connfd , sds buffer , int len , httpHeader* header ,  handshake* hs );
static char* findEolChar( const char* s , int len );
static int getLeftEolLength( const char* s );
int bufferLineSearchEOL( httpHeader* header , const char* buffer , int len , char* eol_style );
int bufferReadln( httpHeader* header , const char* buffer , int len , char* eol_style );
char* findChar(  char sp_char , const char* dest , int len );
char* findSpace(  const char* s , int len );

/*
by RFC2616
http://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html#sec5.1
The Request-Line begins with a method token, followed by the Request-URI and the protocol version, and ending with CRLF. 
The elements are separated by SP characters. No CR or LF is allowed except in the final CRLF sequence.
Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
*/
static int parseFirstLine( httpHeader* header , const char* buffer , int len );
static int  readingHeaderFirstLine( httpHeader* header ,const char* buffer , int len );
static int readingHeaders( httpHeader* header , const char* buffer , int len );
int readingSingleLine(  httpHeader* header , const char* org , int len );
char* getHeaderParams(  httpHeader* header , char* pkey );
int isHttpProtocol( char* buffer , int len );
void getGetMethodBody();
void getPostMethodBody();
int httpRequestParse( int connfd , sds buffer , int len  );
int httpHeaderParse( httpHeader* header ,  sds buffer , int len );

#endif
