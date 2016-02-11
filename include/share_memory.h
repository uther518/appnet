

#include <sys/shm.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h> 
//#define SHM_TYPE_MMAP


#define SHM_MMAP_FILE_LEN  64
typedef struct _shareMemory_mmap
{
    int size;
    char mapfile[SHM_MMAP_FILE_LEN];
    int tmpfd;
    int key;
    int shmid;
    void *mem;
} shareMemory;

void *shareMemory_mmap_create(shareMemory *object, int size, char *mapfile);
void *shareMemory_sysv_create(shareMemory *object, int size, int key);
int shareMemory_sysv_free(shareMemory *object, int rm);
int shareMemory_mmap_free(shareMemory *object);

void* shm_malloc(size_t size);
void  shm_free(void *ptr , int rm );
void* shm_calloc(size_t num, size_t _size);
void* shm_realloc(void *ptr, size_t new_size);



