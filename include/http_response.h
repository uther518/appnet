

#ifndef _HTTP_RESPONSE_H_
#define _HTTP_RESPONSE_H_

#include "sds.h"
#include "http_request.h"

typedef struct 
{
	char hRespCode[64];
	char hServerName[32];
	char hContentLength[128];
	char hContentType[128];
	
}commonResponse;




#endif /* _HTTP_RESPONSE_H_ */
