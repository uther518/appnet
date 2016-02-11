

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/ring_buffer.h"
#include "../include/share_memory.h"
#include "../include/zmalloc.h"


#define check(A, M, ...) if(!(A)) {\
    printf(M, ##__VA_ARGS__); errno=0; goto error; }


ringBuffer *ringBuffer_create(int length , int use_shm )
{
	ringBuffer *buffer;
	if( use_shm )
	{
		buffer = shm_calloc(  1 , sizeof(ringBuffer) );
        }
	else
	{
		buffer = zcalloc( sizeof(ringBuffer) );
	}

	if( buffer == NULL )
	{
	    printf( "ringBuffer_create error \n");
	    return NULL;
	}

	buffer->length = length + 1;
    	buffer->start = 0;
    	buffer->end = 0;
	buffer->use_shm = ( use_shm > 0 ) ? 1 : 0;
	
	if( buffer->use_shm == 1 )
	{
		buffer->buffer = shm_calloc(buffer->length , 1 );
        }
	else
	{
		buffer->buffer = zcalloc(buffer->length);
	}
	

    return buffer;
}

void ringBuffer_destroy(ringBuffer * buffer)
{
    if (buffer)
	{
		if( buffer->use_shm )
		{
			shm_free( buffer->buffer , 1 );
			shm_free( buffer , 1 );
		}
		else
		{
			zfree(buffer->buffer);
			zfree(buffer);
		}
    }
}

int ringBuffer_write(ringBuffer * buffer, char *data, int length)
{
    if (ringBuffer_available_data(buffer) == 0) {
        buffer->start = buffer->end = 0;
    }

    check(length <= ringBuffer_available_space(buffer),
            "Not enough space: %d request, %d available",
            ringBuffer_available_data(buffer), length);

    void *result = memcpy(ringBuffer_ends_at(buffer), data, length);
    check(result != NULL, "Failed to write data into buffer.");

    ringBuffer_commit_write(buffer, length);

    return length;
error:
    return -1;
}

int ringBuffer_read(ringBuffer * buffer, char *target, int amount)
{
    check(amount <= ringBuffer_available_data(buffer),
            "Not enough in the buffer: has %d, needs %d",
            ringBuffer_available_data(buffer), amount);

    void *result = memcpy(target, ringBuffer_starts_at(buffer), amount);
    check(result != NULL, "Failed to write buffer into data.");

    ringBuffer_commit_read(buffer, amount);

    if (buffer->end == buffer->start) {
        buffer->start = buffer->end = 0;
    }

    return amount;
error:
    return -1;
}

