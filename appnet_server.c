
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_appnet.h"
#include <stdio.h>

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
   RETURN_LONG( sendlen );
}


ZEND_METHOD( appTcpServer , close )
{
   long fd;
   if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &fd   ) == FAILURE)
   {
        RETURN_FALSE;;
   }

   if( fd <= 0 )
   {
      printf( "app close fd=%d error \n" , fd );
      RETURN_FALSE;
  }

   aeServer* appserv = APPNET_G( appserv );
   aeConnection* uc = &appserv->connlist[fd];
   if( uc == NULL )
   {
       php_printf( "close error,client obj is null");
       RETURN_FALSE;
   }
   
   appserv->close( fd );
   RETURN_TRUE;
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

void appnetServer_onRecv( aeServer* s, aeConnection *c, char* buff , int len )
{
     aeServer* appserv = APPNET_G( appserv );
     zval retval;
     zval *args;
     zval *zserv = (zval*)appserv->ptr2;
     zval zdata;
     zval zfd;
     args = safe_emalloc(sizeof(zval),3, 0 );

     ZVAL_LONG( &zfd , (long)c->fd );
     ZVAL_STRINGL( &zdata , buff , len );
     ZVAL_COPY(&args[0], zserv  );
     ZVAL_COPY(&args[1], &zfd  );
     ZVAL_COPY(&args[2], &zdata  );

     if (call_user_function_ex(EG(function_table), NULL, appnet_tcpserv_callback[APPNET_TCP_SERVER_CB_onReceive],&retval, 3, args, 0, NULL) == FAILURE )
     {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "call_user_function_ex recv error");
     } 
	 
     if (EG(exception))
     {
		php_error_docref(NULL, E_WARNING, "bind recv callback failed");
     }

     zval_ptr_dtor(&zfd);
     zval_ptr_dtor(&zdata);   

     //zval_ptr_dtor(&args[1]);
     //zval_ptr_dtor(&args[2]);
     efree( args );
     if ( &retval != NULL)
     {
        zval_ptr_dtor(&retval);
     }

}

void appnetServer_onClose( aeServer* s , aeConnection *c )
{
   aeServer* appserv = APPNET_G( appserv );
   zval retval;
   zval *args;
   zval *zserv = (zval*)appserv->ptr2;
   zval zfd;

   args = safe_emalloc(sizeof(zval),2, 0 );
   ZVAL_LONG( &zfd , (long)c->fd );

   ZVAL_COPY(&args[0], zserv  );
   ZVAL_COPY(&args[1], &zfd  );

   if (call_user_function_ex(EG(function_table), NULL, appnet_tcpserv_callback[APPNET_TCP_SERVER_CB_onClose],&retval, 2, args, 0, NULL) == FAILURE )
   {
      php_error_docref(NULL TSRMLS_CC, E_WARNING, "call_user_function_ex connect error");
   }

   if (EG(exception))
   {
      php_error_docref(NULL, E_WARNING, "bind accept callback failed");
   }
   zval_ptr_dtor(&zfd);
   efree( args );
   if ( &retval != NULL)
   {
     zval_ptr_dtor(&retval);
   } 
   //s->close( c );
}


void appnetServer_onConnect( aeServer* s ,int fd )
{
   aeServer* appserv = APPNET_G( appserv );
   zval retval;
   zval *args;
   zval *zserv = (zval*)appserv->ptr2;
   zval zfd;

   args = safe_emalloc(sizeof(zval),2, 0 );
   ZVAL_LONG( &zfd , (long)fd );

   ZVAL_COPY(&args[0], zserv  );
   ZVAL_COPY(&args[1], &zfd  );

   if (call_user_function_ex(EG(function_table), NULL, appnet_tcpserv_callback[APPNET_TCP_SERVER_CB_onConnect],&retval, 2, args, 0, NULL) == FAILURE )
   {
      php_error_docref(NULL TSRMLS_CC, E_WARNING, "call_user_function_ex connect error");
   }

   if (EG(exception))
   {
      php_error_docref(NULL, E_WARNING, "bind accept callback failed");
   }
   zval_ptr_dtor(&zfd);
   efree( args );
   if ( &retval != NULL)
   {
     zval_ptr_dtor(&retval);
   }

}

aeServer* appnetTcpServInit( char* listen_ip , int port  )
{
     serv = aeServerCreate( listen_ip , port );
     serv->onConnect = 	&appnetServer_onConnect;
     serv->onRecv = 	&appnetServer_onRecv;
     serv->onClose = 	&appnetServer_onClose;
     return serv;
}

void appnetTcpServRun()
{
     serv->runForever( serv );
}
