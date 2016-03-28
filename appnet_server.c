
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_appnet.h"
#include <stdio.h>

zval* appnet_serv_callback[APPNET_SERVER_CALLBACK_NUM];
static int appnet_set_callback(int key, zval* cb TSRMLS_DC);
aeServer* serv;


typedef struct _timerArgs
{
	int    ts;
	void*  func;
	zval*  params;
}timerArgs;

//=====================================================
ZEND_METHOD( appnetServer , __construct )
{
    size_t host_len = 0;
    char* serv_host;
    long  serv_port;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sl", &serv_host, &host_len, &serv_port ) == FAILURE)
    {
        return;
    }
    aeServer* appserv = appnetServInit( serv_host , serv_port );
    //save to global var
    appserv->ptr2 = getThis();
    APPNET_G( appserv ) = appserv;
}

ZEND_METHOD( appnetServer , setOption )
{
    size_t key_len;
    size_t val_len;
    char*  key;
    char*  val;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &key, &key_len, &val , &val_len ) == FAILURE)
    {
        return;
    }
    aeServer* appserv = APPNET_G( appserv );
    int ret = appserv->setOption( key,val );
    if( ret == AE_TRUE )
    {
	RETURN_TRUE;
    }
    RETURN_FALSE;
}


ZEND_METHOD( appnetServer , setHeader )
{
    size_t val_len;
    char*  val;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s",  &val , &val_len ) == FAILURE)
    {
        return;
    }
    aeServer* appserv = APPNET_G( appserv );
    int ret = appserv->setHeader( val );
    if( ret == AE_TRUE )
    {
		RETURN_TRUE;
    }
    RETURN_FALSE;
}

ZEND_METHOD( appnetServer , getInfo )
{
	array_init(return_value);
	aeServer* appserv = APPNET_G( appserv );
	if( appserv == NULL )
	{
		return;
	}
	add_assoc_long(return_value, "worker_num" ,appserv->workerNum );
	add_assoc_long(return_value, "reactor_num" , appserv->reactorNum  );
	add_assoc_long(return_value, "max_connection" , appserv->maxConnect );
	add_assoc_long(return_value, "protocol" , appserv->protocolType );
}


ZEND_METHOD( appnetServer , run )
{
   appnetServRun();
}

ZEND_METHOD( appnetServer , getHeader )
{
	array_init(return_value);
	aeServer* appserv = APPNET_G( appserv );

	int i;
	int connfd = appserv->worker->connfd;
	httpHeader* header = &appserv->connlist[connfd].hh;
	if( header->protocol == HTTP )
	{
	add_assoc_string(return_value, "Protocol" , "HTTP" );
	}
	else if(  header->protocol == WEBSOCKET )
	{
		 add_assoc_string(return_value, "Protocol" , "WEBSOCKET" );
	}
	else
	{
	add_assoc_string(return_value, "Protocol" , "TCP" );
	return;
	}
	add_assoc_string(return_value, "Method" , header->method );
	add_assoc_string(return_value, "Uri" , header->uri );
	add_assoc_string(return_value, "Version" , header->version );

	for( i = 0 ; i < header->filed_nums ; i++ )
	{
	add_assoc_string(return_value, header->fileds[i].key  , header->fileds[i].value  );
	}
}

ZEND_METHOD( appnetServer , send )
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

int onTimer( aeEventLoop *l, int id,void *data  )
{
	timerArgs* timerArg = (timerArgs*)data;

	zval* callback = (zval*)timerArg->func;
	zval* params = (zval*)timerArg->params;

	aeServer* appserv = APPNET_G( appserv );
	zval retval;

	zval *args;
	zval *zserv = (zval*)appserv->ptr2;
	zval zid;
	args = safe_emalloc(sizeof(zval),3, 0 );

	ZVAL_LONG( &zid , (long)id );
	ZVAL_COPY(&args[0], zserv  );
	ZVAL_COPY(&args[1] , &zid    );
	ZVAL_COPY(&args[2], params );

	if (call_user_function_ex(EG(function_table), NULL, callback  ,&retval, 3 , args , 0, NULL) == FAILURE )
	{
	 php_error_docref(NULL TSRMLS_CC, E_WARNING, "call_user_function_ex timer callback error");
	}

	if (EG(exception))
	{
	 php_error_docref(NULL, E_WARNING, "timer callback failed");
	}

	zval_ptr_dtor(&zid);

	efree( args );
	if ( &retval != NULL)
	{
	zval_ptr_dtor(&retval);
	}

	return timerArg->ts;
}


ZEND_METHOD( appnetServer , timerAdd )
{
    long ms;
    zval* callback;
    zval* args;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "lz|z", &ms, &callback, &args ) == FAILURE)
    {
        RETURN_FALSE;
    }

    timerArgs* timerArg = zmalloc( sizeof( timerArgs ));
    timerArg->func = callback;
    timerArg->ts = ms;
    timerArg->params = args; 


    timerAdd(  ms , onTimer , timerArg  );   
    RETURN_TRUE;
}

ZEND_METHOD( appnetServer , timerRemove )
{
    long tid;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &tid  ) == FAILURE)
    {
        RETURN_FALSE;
    }

    timerRemove(  tid );
    RETURN_TRUE;
}


ZEND_METHOD( appnetServer , close )
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



ZEND_METHOD( appnetServer , on )
{
    size_t type_len,i,ret;
    char*  type;
    zval *cb;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "sz",  &type, &type_len, &cb ) == FAILURE)
    {
      return;
    }
    
    char *callback[APPNET_SERVER_CALLBACK_NUM] = {
        APPNET_EVENT_CONNECT,
        APPNET_EVENT_RECV,
        APPNET_EVENT_CLOSE,
		APPNET_EVENT_START,
		APPNET_EVENT_FINAL,
        APPNET_EVENT_TIMER
    };
	
    for (i = 0; i < APPNET_SERVER_CALLBACK_NUM; i++)
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
    appnet_serv_callback[key] = emalloc(sizeof(zval));
    if (appnet_serv_callback[key] == NULL)
    {
        return AE_ERR;
    }
    *(appnet_serv_callback[key]) =  *cb;
    zval_copy_ctor( appnet_serv_callback[key]);
    return AE_OK;
}

void appnetServer_onRecv( aeServer* s, aeConnection *c, sds buff , int len )
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

	if (call_user_function_ex(EG(function_table), NULL, appnet_serv_callback[APPNET_SERVER_CB_onReceive],&retval, 3, args, 0, NULL) == FAILURE )
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
	//sdsfree( buff );
	if ( &retval != NULL)
	{
	zval_ptr_dtor(&retval);
	}

}

void appnetServer_onClose( aeServer* s , aeConnection *c )
{
	if( c == NULL )
	{
		printf( "Error appnetServer_onClose Connection is NULL\n");
		return;
	}

	aeServer* appserv = APPNET_G( appserv );
	zval retval;
	zval *args;
	zval *zserv = (zval*)appserv->ptr2;
	zval zfd;

	args = safe_emalloc(sizeof(zval),2, 0 );
	ZVAL_LONG( &zfd , (long)c->fd );

	ZVAL_COPY(&args[0], zserv  );
	ZVAL_COPY(&args[1], &zfd  );

	if (call_user_function_ex(EG(function_table), NULL, appnet_serv_callback[APPNET_SERVER_CB_onClose],&retval, 2, args, 0, NULL) == FAILURE )
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

	if (call_user_function_ex(EG(function_table), NULL, appnet_serv_callback[APPNET_SERVER_CB_onConnect],&retval, 2, args, 0, NULL) == FAILURE )
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


void appnetServer_onStart( aeServer* s  )
{
	aeServer* appserv = APPNET_G( appserv );
	zval retval;
	zval *args;
	zval *zserv = (zval*)appserv->ptr2;

	args = safe_emalloc(sizeof(zval),1, 0 );
	ZVAL_COPY(&args[0], zserv  );

	if (call_user_function_ex(EG(function_table), NULL, 
	appnet_serv_callback[APPNET_SERVER_CB_onStart],&retval, 1, args, 0, NULL) == FAILURE )
	{
	  php_error_docref(NULL TSRMLS_CC, E_WARNING, "call_user_function_ex onStart error");
	}

	if (EG(exception))
	{
	  php_error_docref(NULL, E_WARNING, "bind onStart callback failed");
	}
	efree( args );
	if ( &retval != NULL)
	{
	 zval_ptr_dtor(&retval);
	}
}

void appnetServer_onFinal( aeServer* s  )
{
	aeServer* appserv = APPNET_G( appserv );
	zval retval;
	zval *args;
	zval *zserv = (zval*)appserv->ptr2;

	args = safe_emalloc(sizeof(zval),1, 0 );
	ZVAL_COPY(&args[0], zserv  );
	if (call_user_function_ex(EG(function_table), NULL,
		 appnet_serv_callback[APPNET_SERVER_CB_onFinal],&retval, 1, args, 0, NULL) == FAILURE )
	{
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "call_user_function_ex onFinal error");
	}

	if (EG(exception))
	{
		php_error_docref(NULL, E_WARNING, "bind onFinal callback failed");
	}
	
	efree( args );
	if ( &retval != NULL)
	{
		zval_ptr_dtor(&retval);
	}
}

aeServer* appnetServInit( char* listen_ip , int port  )
{
	serv = aeServerCreate( listen_ip , port );
	serv->onConnect = 	&appnetServer_onConnect;
	serv->onRecv = 	&appnetServer_onRecv;
	serv->onClose = 	&appnetServer_onClose;
	serv->onStart =    &appnetServer_onStart;
	serv->onFinal =    &appnetServer_onFinal;
	return serv;
}

void appnetServRun()
{
	serv->runForever( serv );
}
