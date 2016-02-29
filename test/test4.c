
#include <stdio.h>
#include <stddef.h>


typedef struct
{
	int pos;
	int length;
}header_value_t;

typedef struct 
{
	header_value_t server;
	header_value_t date;
}header_data_pos_t;

typedef struct
{
	int count;
	int length;
	char data[4096];
}header_out_t;


typedef enum
{
   header_server = 0,
   header_date	  
}header_key;

header_value_t header_values_arr[2] = {0};


void header_out_push(  header_out_t* header_out , int key ,  char* data , int len )
{
	header_values_arr[key].pos = header_out->length;
	header_values_arr[key].length = len;
	
	memcpy( header_out->data + header_out->length , data , len  );
	header_out->length += len;
	header_out->count += 1;
}

int main()
{

header_out_t  header_out;
memset( &header_out , 0 , sizeof( header_out ));

char* serv = "Server/1.1.0";
char* date = "Data-1985";

header_out_push( &header_out , header_server , serv , strlen( serv )  );
header_out_push( &header_out , header_date ,   date , strlen( date ) );


printf( "header_out length=%d,count=%d,data=%s \n" , header_out.length , header_out.count , header_out.data );
return 0;
}
