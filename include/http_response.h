

#ifndef _HTTP_RESPONSE_H_
#define _HTTP_RESPONSE_H_

#include "sds.h"
#include "http_request.h"

#define CRLF   "\r\n"
#define HEADER_MAX_LENGTH  4096


static char http_error_301_page[] =
"<html>" CRLF
"<head><title>301 Moved Permanently</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>301 Moved Permanently</h1></center>" CRLF
;

static char http_error_302_page[] =
"<html>" CRLF
"<head><title>302 Found</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>302 Found</h1></center>" CRLF
;


static char http_error_303_page[] =
"<html>" CRLF
"<head><title>303 See Other</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>303 See Other</h1></center>" CRLF
;


static char http_error_307_page[] =
"<html>" CRLF
"<head><title>307 Temporary Redirect</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>307 Temporary Redirect</h1></center>" CRLF
;


static char http_error_400_page[] =
"<html>" CRLF
"<head><title>400 Bad Request</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>400 Bad Request</h1></center>" CRLF
;


static char http_error_401_page[] =
"<html>" CRLF
"<head><title>401 Authorization Required</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>401 Authorization Required</h1></center>" CRLF
;


static char http_error_402_page[] =
"<html>" CRLF
"<head><title>402 Payment Required</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>402 Payment Required</h1></center>" CRLF
;


static char http_error_403_page[] =
"<html>" CRLF
"<head><title>403 Forbidden</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>403 Forbidden</h1></center>" CRLF
;


static char http_error_404_page[] =
"<html>" CRLF
"<head><title>404 Not Found</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>404 Not Found</h1></center>" CRLF
;


static char http_error_405_page[] =
"<html>" CRLF
"<head><title>405 Not Allowed</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>405 Not Allowed</h1></center>" CRLF
;


static char http_error_406_page[] =
"<html>" CRLF
"<head><title>406 Not Acceptable</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>406 Not Acceptable</h1></center>" CRLF
;


static char http_error_408_page[] =
"<html>" CRLF
"<head><title>408 Request Time-out</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>408 Request Time-out</h1></center>" CRLF
;


static char http_error_409_page[] =
"<html>" CRLF
"<head><title>409 Conflict</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>409 Conflict</h1></center>" CRLF
;


static char http_error_410_page[] =
"<html>" CRLF
"<head><title>410 Gone</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>410 Gone</h1></center>" CRLF
;


static char http_error_411_page[] =
"<html>" CRLF
"<head><title>411 Length Required</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>411 Length Required</h1></center>" CRLF
;


static char http_error_412_page[] =
"<html>" CRLF
"<head><title>412 Precondition Failed</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>412 Precondition Failed</h1></center>" CRLF
;


static char http_error_413_page[] =
"<html>" CRLF
"<head><title>413 Request Entity Too Large</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>413 Request Entity Too Large</h1></center>" CRLF
;


static char http_error_414_page[] =
"<html>" CRLF
"<head><title>414 Request-URI Too Large</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>414 Request-URI Too Large</h1></center>" CRLF
;


static char http_error_415_page[] =
"<html>" CRLF
"<head><title>415 Unsupported Media Type</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>415 Unsupported Media Type</h1></center>" CRLF
;


static char http_error_416_page[] =
"<html>" CRLF
"<head><title>416 Requested Range Not Satisfiable</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>416 Requested Range Not Satisfiable</h1></center>" CRLF
;


static char http_error_494_page[] =
"<html>" CRLF
"<head><title>400 Request Header Or Cookie Too Large</title></head>"
CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>400 Bad Request</h1></center>" CRLF
"<center>Request Header Or Cookie Too Large</center>" CRLF
;


static char http_error_495_page[] =
"<html>" CRLF
"<head><title>400 The SSL certificate error</title></head>"
CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>400 Bad Request</h1></center>" CRLF
"<center>The SSL certificate error</center>" CRLF
;


static char http_error_496_page[] =
"<html>" CRLF
"<head><title>400 No required SSL certificate was sent</title></head>"
CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>400 Bad Request</h1></center>" CRLF
"<center>No required SSL certificate was sent</center>" CRLF
;


static char http_error_497_page[] =
"<html>" CRLF
"<head><title>400 The plain HTTP request was sent to HTTPS port</title></head>"
CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>400 Bad Request</h1></center>" CRLF
"<center>The plain HTTP request was sent to HTTPS port</center>" CRLF
;


static char http_error_500_page[] =
"<html>" CRLF
"<head><title>500 Internal Server Error</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>500 Internal Server Error</h1></center>" CRLF
;


static char http_error_501_page[] =
"<html>" CRLF
"<head><title>501 Not Implemented</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>501 Not Implemented</h1></center>" CRLF
;


static char http_error_502_page[] =
"<html>" CRLF
"<head><title>502 Bad Gateway</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>502 Bad Gateway</h1></center>" CRLF
;


static char http_error_503_page[] =
"<html>" CRLF
"<head><title>503 Service Temporarily Unavailable</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>503 Service Temporarily Unavailable</h1></center>" CRLF
;


static char http_error_504_page[] =
"<html>" CRLF
"<head><title>504 Gateway Time-out</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>504 Gateway Time-out</h1></center>" CRLF
;


static char http_error_507_page[] =
"<html>" CRLF
"<head><title>507 Insufficient Storage</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>507 Insufficient Storage</h1></center>" CRLF
;

#define http_status_200  200
#define http_status_300  300
#define http_status_400  400
#define http_status_500  500
#define http_status_max  510

static char* http_status_20x[] = {
	"200 OK",
    "201 Created",
    "202 Accepted",
    "",  /* "203 Non-Authoritative Information" */
    "204 No Content",
    "",  /* "205 Reset Content" */
    "206 Partial Content"
};

static char* http_status_30x[] = {
    "301 Moved Permanently",
    "302 Moved Temporarily",
    "303 See Other",
    "304 Not Modified",
    "",  /* "305 Use Proxy" */
    "",  /* "306 unused" */
    "307 Temporary Redirect"
};

static char* http_status_40x[] = {
    "400 Bad Request",
    "401 Unauthorized",
    "402 Payment Required",
    "403 Forbidden",
    "404 Not Found",
    "405 Not Allowed",
    "406 Not Acceptable",
    "",  /* "407 Proxy Authentication Required" */
    "408 Request Time-out",
    "409 Conflict",
    "410 Gone",
    "411 Length Required",
    "412 Precondition Failed",
    "413 Request Entity Too Large",
    "414 Request-URI Too Large",
    "415 Unsupported Media Type",
    "416 Requested Range Not Satisfiable"
};
  
static char* http_status_50x[] = {
    "500 Internal Server Error",
    "501 Not Implemented",
    "502 Bad Gateway",
    "503 Service Temporarily Unavailable",
    "504 Gateway Time-out",
    "",        /* "505 HTTP Version Not Supported" */
    "",        /* "506 Variant Also Negotiates" */
    "507 Insufficient Storage"
    /* "", */  /* "508 unused" */
    /* "", */  /* "509 unused" */
    /* "", */  /* "510 Not Extended" */
};

typedef enum
{
   HEADER_STATUS = 0,
   HEADER_SERVER,
   HEADER_CONTENT_TYPE,
   HEADER_CONTENT_LENGTH
}header_key;

static char* header_formats[] = {
	"%s %s"CRLF, //http1.1 200 OK
	"Server: %s"CRLF,
	"Content-Type: %s"CRLF,
	"Content-Length: %d"CRLF
};

inline char* get_http_status( int status )
{
	if( status < http_status_200 || status  > http_status_max )
	{
		return "";
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
}

static char http_server_string[] = "Server: appnet" CRLF;
static char http_server_full_string[] = "Server: " HTTP_VERSION_STR CRLF;

typedef struct
{
	int pos;
	int length;
}header_value_t;

typedef struct
{
	header_value_t server;
	header_value_t date;
}header_data_pos_t;

typedef struct
{
	httpHeader* req;
	int count;
	int length;
	char data[4096];
}header_out_t;


#endif /* _HTTP_RESPONSE_H_ */
