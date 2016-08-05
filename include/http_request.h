

#ifndef __HTTP_REQUEST_H_
#define __HTTP_REQUEST_H_

#include "sds.h"
#include "websocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TCP 0
#define HTTP 1
#define WEBSOCKET 2

#define AE_SPACE ' '
#define AE_EOL_CRLF "\r\n"
#define AE_HEADER_END "\r\n\r\n"

#define AE_HTTP_HEADER_MAX_SIZE 8192

typedef struct {
  char *pos;
  int len;
} headerString;

typedef struct {
  headerString key;
  headerString val;
} headerFiled;

enum { HEADER_METHOD = 1, HEADER_URI, HEADER_VERSION };

typedef struct {
  char method[16];
  char version[16];
  char uri[AE_HTTP_HEADER_MAX_SIZE];
} headerParams;

enum {
  MULTIPART_TYPE_FORM_DATA = 1,
  MULTIPART_TYPE_BYTERANGES,
  MULTIPART_TYPE_ALTERNATIVE,
  MULTIPART_TYPE_DIGEST,
  MULTIPART_TYPE_MIXED,
  MULTIPART_TYPE_PARALLED,
  MULTIPART_TYPE_RELATED
};

typedef struct {
  int connfd;
  int header_length;
  int content_length;
  int complete_length;
  int multipart_data;
  int nobody;
  int keep_alive;
  int trunked;
  int filed_nums;
  int buffer_pos;
  char method[8];
  char uri[1024];
  char version[16];
  char mime_type[32];
  char boundary[64];
  int protocol;
  int major;
  int minor;

  headerFiled fileds[20];
  headerParams params;

} httpHeader;

typedef enum {
  HRET_OK = 0,
  HRET_NOT_HTTP_REQUEST,
  HRET_UNCOMPLETE
} HttpRequestErrorType;

#define CHUNK_SZ 128

int wesocketRequestRarse(int connfd, sds buffer, int len, httpHeader *header,
                         handshake *hs);
static char *findEolChar(const char *s, int len);
static int getLeftEolLength(const char *s);
int bufferLineSearchEOL(httpHeader *header, const char *buffer, int len,
                        char *eol_style);
int bufferReadln(httpHeader *header, const char *buffer, int len,
                 char *eol_style);
char *findChar(char sp_char, const char *dest, int len);
char *findSpace(const char *s, int len);

/*
by RFC2616
http://www.w3.org/Protocols/rfc2616/rfc2616-sec5.html#sec5.1
The Request-Line begins with a method token, followed by the Request-URI and the
protocol version, and ending with CRLF.
The elements are separated by SP characters. No CR or LF is allowed except in
the final CRLF sequence.
Request-Line   = Method SP Request-URI SP HTTP-Version CRLF
*/
static int parseFirstLine(httpHeader *header, const char *buffer, int len);
static int readingHeaderFirstLine(httpHeader *header, const char *buffer,
                                  int len);
static int readingHeaders(httpHeader *header, const char *buffer, int len);
int readingSingleLine(httpHeader *header, const char *org, int len);
char *getHeaderParams(httpHeader *header, char *pkey);
int isHttpProtocol(char *buffer, int len);
void getGetMethodBody();
void getPostMethodBody();
int httpRequestParse(int connfd, sds buffer, int len);
int httpHeaderParse(httpHeader *header, sds buffer, int len);

#endif
