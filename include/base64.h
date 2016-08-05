/**
 https://github.com/nori0428/mod_websocket/blob/master/src/mod_websocket_base64.h
 */

#ifndef	_BASE64_H_
#define	_BASE64_H_

#include <stdio.h>
#include <unistd.h>

size_t base64_encode( char *buf , size_t nbuf , const unsigned char *p , size_t n );
int base64_decode( unsigned char ** , size_t * , const unsigned char * );
#endif	/* _BASE64_H_ */
