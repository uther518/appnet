
#include <stdio.h>
#include <stddef.h>

typedef struct _header_data_pos_t
{
	int server;
	int date;
	int content_length;
	int content_encoding;
	int location;
	int last_modified;
	int accept_ranges;
	int expires;
	int cache_control;
	int etag;
}header_data_pos_t;

/*
int header_name_pos[] = {
	{ "Server" , offsetof( header_data_pos_t , server )  },
	{ "Date" , offsetof( header_data_pos_t , date )  },
	
};

*/
typedef struct _header_out_t
{
	header_data_pos_t data_pos;
	char data[1024];
	int count;
	int length;
}header_out_t;



void header_out_push(  header_out_t* header_out , int pos , char* data , int len )
{
	memcpy( &header_out->data + header_out->length , data , len  );
	header_out->length += len;
	header_out->count += 1;

printf( "start pos=%d...\n" , pos );
	
	memcpy( &header_out->data_pos +  pos  ,  len , sizeof( len ));	

printf( "end...\n");
}

int main()
{

header_out_t  header_out;
memset( &header_out , 0 , sizeof( header_out ));

char* serve = "Server/1.1.0";
header_out_push( &header_out , offsetof( header_data_pos_t , server ) , serve , strlen( serve )  );

printf( "header_out length=%d,count=%d,data_pos=%d,data=%s \n" , header_out.length , header_out.count , &header_out.data_pos + offsetof( header_data_pos_t , server ) , header_out.data );
return 0;
}
