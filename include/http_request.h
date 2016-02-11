


#ifndef __HTTP_REQUEST_H_
#define __HTTP_REQUEST_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "websocket.h"

#define AE_SPACE       ' '
#define AE_EOL_CRLF        "\r\n"
#define AE_HEADER_END	   "\r\n\r\n"
/*
#define AE_OK        0
#define AE_ERR       -1
*/

#define AE_HTTP_HEADER_MAX_SIZE          8192

//http://www.w3.org/Protocols/rfc2616/rfc2616.html
//响应完后，再将包头与包体从recv_buffer中移除
//查找eol在字符串中的位置，找不到就继续接收。
//如果header太长，返回错误，断开

//从s开头，在len个长度查找，每次循环最多找128bytes

typedef struct
{
  char* str_pos;
  int   str_len;
}headerString;

typedef struct
{
	char key[64];
	char value[1024];
	int  buffer_pos;//这个域的开始位置在buffer中的偏移量
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
	//..
	
}headerParams;
#define TCP 0
#define HTTP 1
#define WEBSOCKET 2
typedef struct
{
	int connfd;
	int header_length;//header部分总长
	int content_length;//body长
	int complete_length; //整包长
	int filed_nums; //headerFiled numbers
	int buffer_pos; //解析的位置,作为起始位置
	char method[8]; //
	char uri[1024];
	char version[16];
	int  protocol;
	headerFiled fileds[30]; 
	headerParams params;   //分析的结果结构体
	sds buffer;
}httpHeader;


typedef enum
{
	HRET_OK = 0,
	HRET_NOT_HTTP_REQUEST,
	HRET_UNCOMPLETE
}HttpRequestErrorType;	

int wesocketRequestRarse( int connfd , sds buffer , int len , httpHeader* header ,  handshake* hs );
#define CHUNK_SZ 128
static char* findEolChar( const char* s , int len );

static int getLeftEolLength( const char* s );

int bufferLineSearchEOL( httpHeader* header , 
	const char* buffer , int len , char* eol_style );

int bufferReadln( httpHeader* header , const char* buffer , 
	int len , char* eol_style );
	
char* findChar(  char sp_char , const char* dest , int len );

char* findSpace(  const char* s , int len );


char* findChar(  char sp_char , const char* dest , int len );
/*
by RFC2616
http://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html#sec5.1
The Request-Line begins with a method token, followed by the Request-URI and the protocol version, and ending with CRLF. 
The elements are separated by SP characters. No CR or LF is allowed except in the final CRLF sequence.
Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
*/
static int parseFirstLine( httpHeader* header , const char* buffer , int len );
static int  readingHeaderFirstLine( httpHeader* header ,
	const char* buffer , int len );
static int readingHeaders( httpHeader* header , const char* buffer , int len );
int readingSingleLine(  httpHeader* header , const char* org , int len );
static char* getHeaderParams(  httpHeader* header , char* pkey );
int isHttpProtocol( char* buffer , int len );
void getGetMethodBody();
void getPostMethodBody();
int httpRequestParse( int connfd , sds buffer , int len  );
static int httpHeaderParse( httpHeader* header ,  sds buffer , int len );

#endif
