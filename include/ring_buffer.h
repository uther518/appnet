#ifndef _RINGBUFFER_H_
#define _RINGBUFFER_H_

#include <errno.h>

typedef struct {
    int length;
    int start;
    int end;
    int use_shm;
    char *buffer;
} ringBuffer;


ringBuffer *ringBuffer_create(int length , int use_shm );

void ringBuffer_destroy(ringBuffer * buffer);

int ringBuffer_read(ringBuffer * buffer, char *target, int amount);

int ringBuffer_write(ringBuffer * buffer, char *data, int length);

int ringBuffer_empty(ringBuffer * buffer);

int ringBuffer_full(ringBuffer * buffer);

int ringBuffer_available_data(ringBuffer * buffer);

int ringBuffer_available_space(ringBuffer * buffer);


#define ringBuffer_available_data(B) (\
        (B)->end % (B)->length - (B)->start)

#define ringBuffer_available_space(B) (\
        (B)->length - (B)->end - 1)

#define ringBuffer_full(B) (ringBuffer_available_space(B) == 0)

#define ringBuffer_empty(B) (ringBuffer_available_data((B)) == 0)

#define ringBuffer_puts(B, D) ringBuffer_write(\
        (B), bdata((D)), blength((D)))

#define ringBuffer_starts_at(B) (\
        (B)->buffer + (B)->start)

#define ringBuffer_ends_at(B) (\
        (B)->buffer + (B)->end)

#define ringBuffer_commit_read(B, A) (\
        (B)->start = ((B)->start + (A)) % (B)->length)

#define ringBuffer_commit_write(B, A) (\
        (B)->end = ((B)->end + (A)) % (B)->length)

#define ringBuffer_clear(B) ringBuffer_commit_read((B),\
        ringBuffer_available_data((B)));

#endif
