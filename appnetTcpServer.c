
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_appnet.h"

#include <stdio.h>
//#include "include/aeserver.h"

#define APPNET_TCP_SERVER_CALLBACK_NUM              4
#define APPNET_TCP_SERVER_CB_onConnect              0 //accept new connection(worker)
#define APPNET_TCP_SERVER_CB_onReceive              1 //receive data(worker)
#define APPNET_TCP_SERVER_CB_onClose                2 //close tcp connection(worker)
#define APPNET_TCP_SERVER_CB_onTimer                3 //timer call(master)

zval* appnet_tcpserv_callback[APPNET_TCP_SERVER_CALLBACK_NUM];
static int appnet_set_callback(int key, zval* cb TSRMLS_DC);
aeServer* serv;

//=====================================================
ZEND_METHOD( appTcpServer , __construct )
{
    size_t host_len = 0;
    char* serv_host;
    long  serv_port;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &serv_host, &host_len, &serv_port ) == FAILURE)
    {
        return;
    }

    aeServer* appserv = appnetTcpServInit( serv_host , serv_port );
    //save to global var
    appserv->ptr2 = getThis();
    APPNET_G( appserv ) = appserv;
}

ZEND_METHOD( appTcpServer , run )
{
   //serv->ptr2 = getThis();
   appnetTcpServRun();
}

ZEND_METHOD( appTcpServer , send )
{
   size_t len = 0;
   long fd;
   char* buffer;
   if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ls", &fd,&buffer,&len ) == FAILURE)
   {
        return;
   }
   aeServer* appserv = APPNET_G( appserv );
   int sendlen; 
   sendlen = appserv->send( fd , buffer , len );
}


ZEND_METHOD( appTcpServer , close )
{
   int fd;
   if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &fd   ) == FAILURE)
   {
        return;
   }
//   serv->close( fd );
}



ZEND_METHOD( appTcpServer , on )
{
    size_t type_len,i,ret;
    char*  type;
    zval *cb;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz",  &type, &type_len, &cb ) == FAILURE)
    {
      return;
    }
    
    char *callback[APPNET_TCP_SERVER_CALLBACK_NUM] = {
        "connect",
        "receive",
        "close",
        "timer"
    };
    for (i = 0; i < APPNET_TCP_SERVER_CALLBACK_NUM; i++)
    {
        if (strncasecmp(callback[i], type , type_len ) == 0)
        {
            ret = appnet_set_callback(i, cb TSRMLS_CC);
            break;
        }
    }
    if (ret < 0)
    {
        php_error_docref(NULL TSRMLS_CC, E_ERROR, "Unknown event types[%s]", type );
    }
}

static int appnet_set_callback(int key, zval* cb TSRMLS_DC)
{
    appnet_tcpserv_callback[key] = emalloc(sizeof(zval));
    if (appnet_tcpserv_callback[key] == NULL)
    {
        return AE_ERR;
    }
    *(appnet_tcpserv_callback[key]) =  *cb;
    zval_copy_ctor( appnet_tcpserv_callback[key]);
    //printf( "callback.....%x\n" , &appnet_tcpserv_callback[key] );
    return AE_OK;
}


void appnetServerRecv( aeServer* s , userClient *c , int len )
{
     zval retval;
     zval args[3];
     zval* zserv = (zval*)s->ptr2;

     args[0] = *zserv;
     ZVAL_LONG( &args[1] , c->fd );
     ZVAL_STRING(&args[2], c->recv_buffer );

     if (call_user_function_ex(EG(function_table), NULL, 
		appnet_tcpserv_callback[APPNET_TCP_SERVER_CB_onReceive] ,
		&retval, 3, args, 0, NULL TSRMLS_CC) == SUCCESS TSRMLS_CC )
     {
        zval_ptr_dtor(&retval);
     } else {
         php_error_docref(NULL, E_WARNING, "bind recv callback failed");
     }
 //    zval_ptr_dtor(&args[3]);
}

void appnetServerClose( aeServer* s ,userClient *c )
{
    zval retval;
    zval args[2];
    zval *cb_serv = ( zval*)s;

    ZVAL_COPY_VALUE(&args[0], cb_serv );
    ZVAL_LONG( &args[1] , c->fd );

    if (call_user_function_ex(EG(function_table), NULL, appnet_tcpserv_callback[APPNET_TCP_SERVER_CB_onClose] , &retval, 2 , args, 0, NULL) == SUCCESS )
    {
		zval_ptr_dtor(&retval);
    }else{     
		php_error_docref(NULL, E_WARNING, "bind close callback failed");
    }
    printf( "Worker Client closed  = %d  \n", c->fd );
    serv->close( c );
}

void appnetServerConnect( aeServer* s , userClient *c )
{
     printf( "New Client Connected fd =%d \n" , c->fd );
     zval retval;
     zval args[2];
     zval* cb_serv = (zval*)s;     
     args[0] = *cb_serv;
     ZVAL_LONG( &args[1],  c->fd  );

     if( call_user_function_ex(EG(function_table), NULL, appnet_tcpserv_callback[APPNET_TCP_SERVER_CB_onConnect] , &retval, 2 , args , 0, NULL TSRMLS_CC) == SUCCESS )
     {
         zval_ptr_dtor(&retval);
     } else {
         php_error_docref(NULL, E_WARNING, "bind connect callback failed");
     }
     
     if (EG(exception))
     {
        zend_exception_error(EG(exception), E_ERROR TSRMLS_CC);
     }
     printf( "New Client Connected fd xxxx \n"  );
     zval_ptr_dtor(&args[0]);
}


aeServer* appnetTcpServInit( char* listen_ip , int port  )
{
     serv = aeServerCreate( listen_ip , port );
     serv->onConnect = 	&appnetServerConnect;
     serv->onRecv = 	&appnetServerRecv;
     serv->onClose = 	&appnetServerClose;
     return serv;
}

void appnetTcpServRun()
{
     serv->runForever( serv );
}
