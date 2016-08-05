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

#ifndef PHP_APPNET_H
#define PHP_APPNET_H

extern zend_module_entry appnet_module_entry;
#define phpext_appnet_ptr &appnet_module_entry

#define PHP_APPNET_VERSION                                                     \
  "0.1.0" /* Replace with version number for your extension */

#ifdef PHP_WIN32
#define PHP_APPNET_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#define PHP_APPNET_API __attribute__((visibility("default")))
#else
#define PHP_APPNET_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif
#include "include/appnet_server.h"
/*
        Declare any global variables you may need between the BEGIN
        and END macros here:

ZEND_BEGIN_MODULE_GLOBALS(appnet)
        zend_long  global_value;
        char *global_string;
ZEND_END_MODULE_GLOBALS(appnet)
*/

/* Always refer to the globals in your function as APPNET_G(variable).
   You are encouraged to rename these macros something shorter, see
   examples in any other php module directory.
*/
/*
#define APPNET_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(appnet, v)

#if defined(ZTS) && defined(COMPILE_DL_APPNET)
ZEND_TSRMLS_CACHE_EXTERN();
#endif

#endif	/* PHP_APPNET_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

#define APPNET_SERVER_CALLBACK_NUM 8
#define APPNET_SERVER_CB_onConnect 0 // accept new connection(worker)
#define APPNET_SERVER_CB_onReceive 1 // receive data(worker)
#define APPNET_SERVER_CB_onClose 2   // close tcp connection(worker)
#define APPNET_SERVER_CB_onStart 3
#define APPNET_SERVER_CB_onFinal 4
#define APPNET_SERVER_CB_onTimer 5 // timer call(master)
#define APPNET_SERVER_CB_onTask 6
#define APPNET_SERVER_CB_onTask_CB 7

#define APPNET_EVENT_CONNECT "connect"
#define APPNET_EVENT_RECV "receive"
#define APPNET_EVENT_CLOSE "close"
#define APPNET_EVENT_START "start"
#define APPNET_EVENT_FINAL "final"
#define APPNET_EVENT_TIMER "timer"
#define APPNET_EVENT_TASK "task"
#define APPNET_EVENT_TASK_CB "task_cb"

extern zval *appnet_serv_callback[APPNET_SERVER_CALLBACK_NUM];

zend_class_entry *AppnetServer;

ZEND_METHOD(AppnetServer, __construct);
ZEND_METHOD(AppnetServer, on);
ZEND_METHOD(AppnetServer, run);
ZEND_METHOD(AppnetServer, send);
ZEND_METHOD(AppnetServer, close);
ZEND_METHOD(AppnetServer, getHeader);
ZEND_METHOD(AppnetServer, setHeader);
ZEND_METHOD(AppnetServer, httpRedirect);
ZEND_METHOD(AppnetServer, httpRespCode);
ZEND_METHOD(AppnetServer, timerAdd);
ZEND_METHOD(AppnetServer, timerRemove);
ZEND_METHOD(AppnetServer, setOption);
ZEND_METHOD(AppnetServer, getInfo);
ZEND_METHOD(AppnetServer, setEventCallback);
ZEND_METHOD(AppnetServer, addEventListener);
ZEND_METHOD(AppnetServer, addAsynTask);
ZEND_METHOD(AppnetServer, taskCallback);

appnetServer *appnetServInit(char *serv_host, int serv_port);
void appnetServRun();

ZEND_BEGIN_MODULE_GLOBALS(appnet)
appnetServer *appserv;
ZEND_END_MODULE_GLOBALS(appnet)

extern ZEND_DECLARE_MODULE_GLOBALS(appnet);
/*
#ifdef ZTS
#define APPNET_G(v) TSRMG(appnet_globals_id, zend_appnet_globals *, v)
#else
#define APPNET_G(v) (appnet_globals.v)
#endif
*/

#define APPNET_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(appnet, v)
#if defined(ZTS) && defined(COMPILE_DL_APPNET)
ZEND_TSRMLS_CACHE_EXTERN();
#endif

#endif /* PHP_APPNET_H */
