/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2015 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:LiuChangbing                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_appnet.h"

/* If you declare any globals in php_appnet.h uncomment this:
ZEND_DECLARE_MODULE_GLOBALS(appnet)
*/

/* True global resources - no need for thread safety here */
static int le_appnet;
ZEND_DECLARE_MODULE_GLOBALS(appnet)
/* {{{ PHP_INI
 */
/* Remove comments and fill if you need to have entries in php.ini
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("appnet.global_value",      "42", PHP_INI_ALL, OnUpdateLong, global_value, zend_appnet_globals, appnet_globals)
    STD_PHP_INI_ENTRY("appnet.global_string", "foobar", PHP_INI_ALL, OnUpdateString, global_string, zend_appnet_globals, appnet_globals)
PHP_INI_END()
*/
/* }}} */

/* Remove the following function when you have successfully modified config.m4
   so that your module can be compiled into PHP, it exists only for testing
   purposes. */

/* Every user-visible function in PHP should document itself in the source */
/* {{{ proto string confirm_appnet_compiled(string arg)
   Return a string to confirm that the module is compiled in */
PHP_FUNCTION(confirm_appnet_compiled)
{
	char *arg = NULL;
	size_t arg_len, len;
	zend_string *strg;

	if (zend_parse_parameters(ZEND_NUM_ARGS(), "s", &arg, &arg_len) == FAILURE) {
		return;
	}

	strg = strpprintf(0, "Congratulations! You have successfully modified ext/%.78s/config.m4. Module %.78s is now compiled into PHP.", "appnet", arg);

	RETURN_STR(strg);
}
/* }}} */
/* The previous line is meant for vim and emacs, so it can correctly fold and
   unfold functions in source code. See the corresponding marks just before
   function definition, where the functions purpose is also documented. Please
   follow this convention for the convenience of others editing your code.
*/


/* {{{ php_appnet_init_globals
 */
/* Uncomment this function if you have INI entries
static void php_appnet_init_globals(zend_appnet_globals *appnet_globals)
{
	appnet_globals->global_value = 0;
	appnet_globals->global_string = NULL;
}
*/
/* }}} */
ZEND_BEGIN_ARG_INFO_EX(arginfo_appnet_server_construct, 0, 0, 2)
    ZEND_ARG_INFO(0, serv_host)
    ZEND_ARG_INFO(0, serv_port)
ZEND_END_ARG_INFO()


ZEND_BEGIN_ARG_INFO_EX(arginfo_appnet_server_on, 0, 0, 2 )
    ZEND_ARG_INFO(0, ha_name)
    ZEND_ARG_INFO(0, cb)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_appnet_server_send, 0, 0, 2 )
    ZEND_ARG_INFO(0, fd)
    ZEND_ARG_INFO(0, buffer)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_appnet_set_header, 0, 0, 1 )
    ZEND_ARG_INFO(0, line)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_appnet_server_close, 0, 0, 1 )
    ZEND_ARG_INFO(0, fd )
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_appnet_server_timer_add, 0, 0, 3 )
    ZEND_ARG_INFO(0, ms )
    ZEND_ARG_INFO(0, callback )
    ZEND_ARG_INFO(0, args )
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_appnet_server_timer_remove, 0, 0, 1 )
    ZEND_ARG_INFO(0, tid )
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_appnet_server_set_option, 0, 0, 2 )
    ZEND_ARG_INFO(0, key )
    ZEND_ARG_INFO(0, val )
ZEND_END_ARG_INFO()

/* {{{ appnet_functions[]
 *
 * Every user visible function must have an entry in appnet_functions[].
 */
const zend_function_entry appnet_functions[] = {
    PHP_FE(confirm_appnet_compiled,	NULL)		/* For testing, remove later. */
    PHP_ME(appnetServer,    __construct,   arginfo_appnet_server_construct ,   ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
    PHP_ME(appnetServer,    on,            arginfo_appnet_server_on , ZEND_ACC_PUBLIC )
    PHP_MALIAS( appnetServer,    setEventCallback,     on ,       arginfo_appnet_server_on , ZEND_ACC_PUBLIC )
    PHP_MALIAS( appnetServer,    addEventListener,     on ,       arginfo_appnet_server_on , ZEND_ACC_PUBLIC )
    PHP_ME(appnetServer,    run,           NULL ,                 ZEND_ACC_PUBLIC )
    PHP_ME(appnetServer,    send,          arginfo_appnet_server_send , ZEND_ACC_PUBLIC )
    PHP_ME(appnetServer,    close,         arginfo_appnet_server_close , ZEND_ACC_PUBLIC )
    PHP_ME(appnetServer,    getHeader,     NULL ,                 ZEND_ACC_PUBLIC )
	PHP_ME(appnetServer,    setHeader,     arginfo_appnet_set_header, ZEND_ACC_PUBLIC )
    PHP_ME(appnetServer,    timerAdd,      arginfo_appnet_server_timer_add , ZEND_ACC_PUBLIC )
    PHP_ME(appnetServer,    timerRemove,   arginfo_appnet_server_timer_remove , ZEND_ACC_PUBLIC )
    PHP_ME(appnetServer ,   setOption,     arginfo_appnet_server_set_option , ZEND_ACC_PUBLIC )
    PHP_ME(appnetServer,    getInfo,     NULL ,                 ZEND_ACC_PUBLIC )
	{NULL, NULL, NULL} 
};
/* }}} */



static void php_appnet_init_globals(zend_appnet_globals* appnet_globals)
{  
}

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(appnet)
{
	/* If you have INI entries, uncomment these lines
	REGISTER_INI_ENTRIES();
	*/
    ZEND_INIT_MODULE_GLOBALS( appnet , php_appnet_init_globals , NULL);


	zend_class_entry ce;
	INIT_CLASS_ENTRY( ce , "appnetServer" , appnet_functions );
	appnetServer = zend_register_internal_class( &ce TSRMLS_CC );

	#define REGISTER_APPNET_CLASS_CONST_LONG(const_name, value) \
	zend_declare_class_constant_long( appnetServer, const_name, sizeof(const_name)-1, (zend_long)value);

	#define REGISTER_APPNET_CLASS_CONST_STRING(const_name, value) \
	zend_declare_class_constant_stringl( appnetServer, const_name, sizeof(const_name)-1, value, sizeof(value)-1);

	REGISTER_STRING_CONSTANT( "APPNET_OPT_WORKER_NUM" , 	OPT_WORKER_NUM  , CONST_CS | CONST_PERSISTENT );
	REGISTER_STRING_CONSTANT( "APPNET_OPT_REACTOR_NUM", 	OPT_REACTOR_NUM , CONST_CS | CONST_PERSISTENT );
	REGISTER_STRING_CONSTANT( "APPNET_OPT_MAX_CONNECTION", 	OPT_MAX_CONNECTION , CONST_CS | CONST_PERSISTENT  );
    REGISTER_STRING_CONSTANT( "APPNET_OPT_PROTO_TYPE", 		OPT_PROTOCOL_TYPE , CONST_CS | CONST_PERSISTENT );

	//class const style
	//REGISTER_APPNET_CLASS_CONST_STRING("OPT_WORKER_NUM", 	OPT_WORKER_NUM );
	//REGISTER_APPNET_CLASS_CONST_STRING("OPT_REACTOR_NUM", 	OPT_REACTOR_NUM );	
	//REGISTER_APPNET_CLASS_CONST_STRING("OPT_MAX_CONNECTION", OPT_MAX_CONNECTION );
	//REGISTER_APPNET_CLASS_CONST_STRING("OPT_PROTOCOL_TYPE", OPT_PROTOCOL_TYPE );


	REGISTER_STRING_CONSTANT( "APPNET_EVENT_CONNECT",	APPNET_EVENT_CONNECT,	CONST_CS | CONST_PERSISTENT );
	REGISTER_STRING_CONSTANT( "APPNET_EVENT_RECV",     	APPNET_EVENT_RECV,	CONST_CS | CONST_PERSISTENT );
	REGISTER_STRING_CONSTANT( "APPNET_EVENT_CLOSE",		APPNET_EVENT_CLOSE, 	CONST_CS | CONST_PERSISTENT  );
	REGISTER_STRING_CONSTANT( "APPNET_EVENT_START",     APPNET_EVENT_START , 	CONST_CS | CONST_PERSISTENT );
	REGISTER_STRING_CONSTANT( "APPNET_EVENT_FINAL",  	APPNET_EVENT_FINAL, 	CONST_CS | CONST_PERSISTENT  );
    REGISTER_STRING_CONSTANT( "APPNET_EVENT_TIMER",     APPNET_EVENT_TIMER , 	CONST_CS | CONST_PERSISTENT );

	REGISTER_LONG_CONSTANT( "APPNET_PROTO_TCP_ONLY", 	PROTOCOL_TYPE_TCP_ONLY , CONST_CS | CONST_PERSISTENT  );
	REGISTER_LONG_CONSTANT( "APPNET_PROTO_HTTP_ONLY", 	PROTOCOL_TYPE_HTTP_ONLY , CONST_CS | CONST_PERSISTENT  );
	REGISTER_LONG_CONSTANT( "APPNET_PROTO_WEBSOCKET_ONLY", 	PROTOCOL_TYPE_WEBSOCKET_ONLY , CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT( "APPNET_PROTO_HTTP_MIX", 	PROTOCOL_TYPE_HTTP_MIX , CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT( "APPNET_PROTO_WEBSOCKET_MIX", 	PROTOCOL_TYPE_WEBSOCKET_MIX , CONST_CS | CONST_PERSISTENT );
	REGISTER_LONG_CONSTANT( "APPNET_PROTO_MIX",   		PROTOCOL_TYPE_WEBSOCKET_MIX , CONST_CS | CONST_PERSISTENT );

	REGISTER_STRING_CONSTANT( "APPNET_HTTP_KEEP_ALIVE",     APPNET_HTTP_KEEP_ALIVE , CONST_CS | CONST_PERSISTENT );
	REGISTER_STRING_CONSTANT( "APPNET_HTTP_DOCS_ROOT" , 	APPNET_HTTP_DOCS_ROOT,  CONST_CS | CONST_PERSISTENT  );
	REGISTER_STRING_CONSTANT( "APPNET_HTTP_UPLOAD_DIR" ,    APPNET_HTTP_UPLOAD_DIR, CONST_CS | CONST_PERSISTENT  );
	REGISTER_STRING_CONSTANT( "APPNET_HTTP_404_PAGE" , 	APPNET_HTTP_404_PAGE , 	CONST_CS | CONST_PERSISTENT  );
	REGISTER_STRING_CONSTANT( "APPNET_HTTP_50X_PAGE" , 	APPNET_HTTP_50X_PAGE , 	CONST_CS | CONST_PERSISTENT  );

	//zend_declare_property_null(appTcpServer, "serv_ip", strlen("serv_ip"), ZEND_ACC_PUBLIC TSRMLS_CC);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(appnet)
{
	printf( "PHP_MSHUTDOWN_FUNCTION....\n");
	/* uncomment this line if you have INI entries
	UNREGISTER_INI_ENTRIES();
	*/
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request start */
/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(appnet)
{
#if defined(COMPILE_DL_APPNET) && defined(ZTS)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}
/* }}} */

/* Remove if there's nothing to do at request end */
/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(appnet)
{
    //zval* appnet_tcpserv_callback[APPNET_TCP_SERVER_CALLBACK_NUM];
    int i;
    for (i = 0; i < APPNET_SERVER_CALLBACK_NUM ; i++)
    {
        if ( appnet_serv_callback[i] != NULL)
        {
            zval_dtor( appnet_serv_callback[i] );
            efree(appnet_serv_callback[i]);
        }
    }
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(appnet)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "appnet support", "enabled");
	php_info_print_table_end();

	/* Remove comments if you have entries in php.ini
	DISPLAY_INI_ENTRIES();
	*/
}
/* }}} */


/* {{{ appnet_module_entry
 */
zend_module_entry appnet_module_entry = {
	STANDARD_MODULE_HEADER,
	"appnet",
	appnet_functions,
	PHP_MINIT(appnet),
	PHP_MSHUTDOWN(appnet),
	PHP_RINIT(appnet),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(appnet),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(appnet),
	PHP_APPNET_VERSION,
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_APPNET
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE();
#endif
ZEND_GET_MODULE(appnet)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
