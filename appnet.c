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
  | Author:                                                              |
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
ZEND_BEGIN_ARG_INFO_EX(arginfo_tcpserver_construct, 0, 0, 2)
    ZEND_ARG_INFO(0, serv_host)
    ZEND_ARG_INFO(0, serv_port)
ZEND_END_ARG_INFO()


ZEND_BEGIN_ARG_INFO_EX(arginfo_tcpserver_on, 0, 0, 2 )
    ZEND_ARG_INFO(0, ha_name)
    ZEND_ARG_INFO(0, cb)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tcpserver_send, 0, 0, 2 )
    ZEND_ARG_INFO(0, fd)
    ZEND_ARG_INFO(0, buffer)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_tcpserver_close, 0, 0, 1 )
    ZEND_ARG_INFO(0, fd )
ZEND_END_ARG_INFO()

/* {{{ appnet_functions[]
 *
 * Every user visible function must have an entry in appnet_functions[].
 */
const zend_function_entry appnet_functions[] = {
	PHP_FE(confirm_appnet_compiled,	NULL)		/* For testing, remove later. */
    PHP_ME(appTcpServer,    __construct,   arginfo_tcpserver_construct ,   ZEND_ACC_PUBLIC|ZEND_ACC_CTOR)
    PHP_ME(appTcpServer,    on,            arginfo_tcpserver_on , ZEND_ACC_PUBLIC )
    PHP_ME(appTcpServer,    run,           NULL ,                 ZEND_ACC_PUBLIC )
    PHP_ME(appTcpServer,    send,          arginfo_tcpserver_send , ZEND_ACC_PUBLIC )
    PHP_ME(appTcpServer,    close,         arginfo_tcpserver_close , ZEND_ACC_PUBLIC )
	{NULL, NULL, NULL} 
};
/* }}} */



static void php_appnet_init_globals(zend_appnet_globals* appnet_globals)
{  
	printf( "php_appnet_init_globals....\n");
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
	INIT_CLASS_ENTRY( ce , "appTcpServer" , appnet_functions );
	appTcpServer = zend_register_internal_class( &ce TSRMLS_CC );
	
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
    for (i = 0; i < APPNET_TCP_SERVER_CALLBACK_NUM ; i++)
    {
        if ( appnet_tcpserv_callback[i] != NULL)
        {
            zval_dtor( appnet_tcpserv_callback[i] );
            efree(appnet_tcpserv_callback[i]);
	    php_printf( "efree request shutdown..\n ");
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
