
//https://github.com/lchb369/Aenet.git

#include <stdio.h>
#include "server.h"

//子进程
void myRecv( aeServer* serv , userClient *c , int len )
{
     printf( "recv len = %d \n" , len  );
     printf( "on_read recv data = %s \n" , c->recv_buffer );

     char buff[64] = "i recv your message:\n";
     int sendlen;
     sendlen = serv->send( c->fd , buff , sizeof( buff ));
     sendlen = serv->send( c->fd , c->recv_buffer , strlen( c->recv_buffer ));
}

void myClose( aeServer* serv ,userClient *c )
{
     printf( "Worker Client closed  = %d  \n", c->fd );
     serv->close( c );
}

void myConnect( aeServer* serv , userClient *c )
{
     printf( "New Client Connected fd =%d \n" , c->fd );
}


int main( int argc,char *argv[] )
{
     aeServer* serv = aeServerCreate();
     serv->onConnect = &myConnect;
     serv->onRecv = &myRecv;
     serv->onClose = &myClose;
     serv->runForever( "0.0.0.0" , 3002 );
     return 0;
}
