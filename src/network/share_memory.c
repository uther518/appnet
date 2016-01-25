
#include "share_memory.h"
#define SHM_TYPE_MMAP 1

void* shm_malloc(size_t size)
{
    shareMemory object;
    void *mem;
    //object对象需要保存在头部
    size += sizeof(shareMemory);
#ifdef SHM_TYPE_MMAP
    mem = shareMemory_mmap_create(&object, size, NULL);
#else
	mem = shareMemory_sysv_create(&object, size, 0 );
#endif	
    if (mem == NULL)
    {
        return NULL;
    }
    else
    {
        memcpy(mem, &object, sizeof(shareMemory));
        return mem + sizeof(shareMemory);
    }
}

void* shm_calloc(size_t num, size_t _size)
{
    shareMemory object;
    void *mem;
    void *ret_mem;
    //object对象需要保存在头部
    int size = sizeof(shareMemory) + (num * _size);
#ifdef SHM_TYPE_MMAP
    mem = shareMemory_mmap_create(&object, size, NULL);
#else
   mem = shareMemory_sysv_create(&object, size, 0 );
#endif
   if (mem == NULL)
    {
        return NULL;
    }
    else
    {
        memcpy(mem, &object, sizeof(shareMemory));
        ret_mem = mem + sizeof(shareMemory);
        //calloc需要初始化
        bzero(ret_mem, size - sizeof(shareMemory));
        return ret_mem;
    }
}

void shm_free(void *ptr ,int rm )
{
    //object对象在头部，如果释放了错误的对象可能会发生段错误
    shareMemory *object = ptr - sizeof(shareMemory);
#ifdef DEBUG
    char check = *(char *)(ptr + object->size); //尝试访问
    printf("check:%c\n", check);
#endif

#ifdef SHM_TYPE_MMAP 
    shareMemory_mmap_free(object);
#else
    shareMemory_sysv_free( object, rm );
#endif
}

void* shm_realloc(void *ptr, size_t new_size)
{
    shareMemory *object = ptr - sizeof(shareMemory);
#ifdef DEBUG
    char check = *(char *)(ptr + object->size); //尝试访问
    printf("check:%c\n", check);
#endif
    void *new_ptr;
    new_ptr = shm_malloc(new_size);
    if(new_ptr==NULL)
    {
        return NULL;
    }
    else
    {
        memcpy(new_ptr, ptr, object->size);
        shm_free(ptr , 1 );
        return new_ptr;
    }
}

void *shareMemory_mmap_create(shareMemory *object, int size, char *mapfile)
{
    void *mem;
    int tmpfd = -1;
    int flag = MAP_SHARED;
    bzero(object, sizeof(shareMemory));

#ifdef MAP_ANONYMOUS
    flag |= MAP_ANONYMOUS;
#else
    if(mapfile == NULL)
    {
        mapfile = "/dev/zero";
    }
    if((tmpfd = open(mapfile, O_RDWR)) < 0)
    {
        return NULL;
    }
    strncpy(object->mapfile, mapfile, SHM_MMAP_FILE_LEN);
    object->tmpfd = tmpfd;
#endif

    mem = mmap(NULL, size, PROT_READ | PROT_WRITE, flag, tmpfd, 0);
#ifdef MAP_FAILED
    if (mem == MAP_FAILED)
#else
    if (!mem)
#endif
    {
        printf("mmap() failed. Error: %s[%d]", strerror(errno), errno);
        return NULL;
    }
    else
    {
        object->size = size;
        object->mem = mem;
        return mem;
    }
}

int shareMemory_mmap_free(shareMemory *object)
{
    return munmap(object->mem, object->size);
}

void *shareMemory_sysv_create(shareMemory *object, int size, int key)
{
    int shmid;
    void *mem;
    bzero(object, sizeof(shareMemory));

    if (key == 0)
    {
        key = IPC_PRIVATE;
    }
    //SHM_R | SHM_W |
    if ((shmid = shmget(key, size, IPC_CREAT)) < 0)
    {
        printf("shmget() failed. Error: %s[%d]", strerror(errno), errno);
        return NULL;
    }
    if ((mem = shmat(shmid, NULL, 0)) < 0)
    {
        printf("shmat() failed. Error: %s[%d]", strerror(errno), errno);
        return NULL;
    }
    else
    {
        object->key = key;
        object->shmid = shmid;
        object->size = size;
        object->mem = mem;
        return mem;
    }
}

int shareMemory_sysv_free(shareMemory *object, int rm)
{
    int shmid = object->shmid;
    int ret = shmdt(object->mem);
    if (rm == 1)
    {
        shmctl( shmid, IPC_RMID, NULL);
    }
    return ret;
}

