

#ifndef _HTTP_RESPONSE_H_
#define _HTTP_RESPONSE_H_

#include "sds.h"
#include "http_request.h"
#include <stdarg.h>

#define NULL_STRING ""
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

typedef struct
{
	char* status;
	char* data;
}header_status_t;

static header_status_t http_status_20x[] = {
	{"200 OK", 				NULL_STRING},
	{"201 Created", 		NULL_STRING},
	{"202 Accepted",		NULL_STRING},
	{NULL_STRING,			NULL_STRING},  /* "203 Non-Authoritative Information" */
	{"204 No Content",		NULL_STRING},
    {NULL_STRING,			NULL_STRING},  /* "205 Reset Content" */
	{"206 Partial Content",	NULL_STRING}
};

static header_status_t http_status_30x[] = {
    {"301 Moved Permanently",		http_error_301_page},
	{"302 Moved Temporarily",		http_error_302_page},
	{"303 See Other",				http_error_303_page},
	{"304 Not Modified",			NULL_STRING},
	{NULL_STRING,					NULL_STRING},  /* "305 Use Proxy" */
    {NULL_STRING,					NULL_STRING},  /* "306 unused" */
	{"307 Temporary Redirect",		http_error_307_page}
};

static header_status_t http_status_40x[] = {
    {"400 Bad Request",					http_error_400_page},
	{"401 Unauthorized",				http_error_401_page},
	{"402 Payment Required",			http_error_402_page},
	{"403 Forbidden",					http_error_403_page},
	{"404 Not Found",					http_error_404_page},
	{"405 Not Allowed",					http_error_405_page},
	{"406 Not Acceptable",				http_error_406_page},
	{NULL_STRING,						NULL_STRING},  /* "407 Proxy Authentication Required" */
	{"408 Request Time-out",			http_error_408_page},
	{"409 Conflict",					http_error_409_page},
	{"410 Gone",						http_error_410_page},
	{"411 Length Required",				http_error_411_page},
	{"412 Precondition Failed",			http_error_412_page},
	{"413 Request Entity Too Large",	http_error_413_page},
	{"414 Request-URI Too Large",		http_error_414_page},
	{"415 Unsupported Media Type",		http_error_415_page},
	{"416 Requested Range Not Satisfiable",http_error_416_page}
};
  
static header_status_t http_status_50x[] = {
    {"500 Internal Server Error",			http_error_500_page},
	{"501 Not Implemented",					http_error_501_page},
	{"502 Bad Gateway",						http_error_502_page},
	{"503 Service Temporarily Unavailable",	http_error_503_page},
	{"504 Gateway Time-out",				http_error_504_page},
        {NULL_STRING,							NULL_STRING},         /* "505 HTTP Version Not Supported" */
        {NULL_STRING,							NULL_STRING},         /* "506 Variant Also Negotiates" */
	{"507 Insufficient Storage",			http_error_507_page}
    /* "", */  /* "508 unused" */
    /* "", */  /* "509 unused" */
    /* "", */  /* "510 Not Extended" */
};

typedef enum
{
   HEADER_STATUS = 0,
   HEADER_SERVER,
   HEADER_CONTENT_TYPE,
   HEADER_CONTENT_LENGTH,
   HEADER_LOCATION,
   HEADER_END_LINE
}header_key;

static char* header_formats[] = {
	"%s %s"CRLF, //http1.1 200 OK
	"Server: %s"CRLF,
	"Content-Type: %s"CRLF,
	"Content-Length: %d"CRLF,
	"Location: %s"CRLF,
	CRLF
};

#define HTTP_VERSION_STR  "appnet/1.1.0"
static char http_server_string[] = "Server: appnet" CRLF;
static char http_server_full_string[] = "Server: " HTTP_VERSION_STR  CRLF;

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


int is_file(char *path);
int is_dir(char *path);
void http_response_static( httpHeader* reqHeader  );
void http_response_static_proc( httpHeader* reqHeader );
header_status_t get_http_status( int status );
int header_buffer_append(  header_out_t* header_out , char* data , int len );
int page_is_defined( char* page );
int resp_defined_error_page( header_out_t*  header_out , int err_code );
void get_file_path( char* uri , char* path );
void http_redirect( httpHeader* reqHeader ,  char* uri );
void header_append_length( header_out_t*  header_out , int len );
void create_common_header( header_out_t*  header_out, int status_code   );
void resp_error_page( header_out_t*  header_out, int status_code );

#endif /* _HTTP_RESPONSE_H_ */
