#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "share_memory.h"
typedef struct _info
{
   int a;
   int b;

}info;

int main()
{

   info* list = shm_calloc( 10 , sizeof( info )); 
int n;
 for( n = 0 ;n < 3; n++)
{
    pid_t child;  
    if((child = fork()) == -1)  
    {  
        perror("fork");  
        exit(EXIT_FAILURE);  
    }  
    else if(child == 0)                 //子进程中  
    {
       int i;
	
	for( i =1; i < 10;i++ )
	{
	   list[n].a = i+100*n;
        }  
        sleep(1);
       for( i =1; i < 10;i++ )
        {
           printf( "child list.a=%d \n" , list[2-n].a );
        }    
	shm_free( list );   
       exit(0);  
    }  
    else      
    {  
	int i;
       	sleep( 2 );
	 for( i =0; i < 10;i++ )
        {
           printf( "parent list.a=%d \n" , list[i].a );
        }  
    }  
};

shm_free( list );
  return 0;
}
