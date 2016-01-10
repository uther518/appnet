
//https://github.com/lchb369/Aenet.git

#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include "ae.h"
#include "anet.h"
#include <errno.h>
#include <sys/socket.h>
#include "../include/aeserver.h"

#include <unistd.h>
//aEventBase aEvBase;
//aWorkerBase aWorker;

void initOnLoopStart(struct aeEventLoop *el) 
{
  //  puts("master loop_init!!! \n");
}

//当监听的listenfd有事件发生时，
void onReadableEvent(aeEventLoop *el, int fd, void *privdata, int mask)
{
	if( fd == aEvBase.listenfd )
	{
		//此处发送给子进程
		char neterr[1024];
		int new_conn = 1;
		send( aEvBase.worker_process[aEvBase.worker_process_counter++].pipefd[0], ( char* )&new_conn, sizeof( new_conn ), 0 );
	//	printf( "master send request to child %d\n", aEvBase.worker_process_counter-1 );
		aEvBase.worker_process_counter %= WORKER_PROCESS_COUNT;
	}
	//如果收到主进程发来的信号事件,这是主进程发来的。。同一个进程间的通信。。
	else if( fd == aEvBase.sig_pipefd[0] )
	{   printf( "Recv Master Siginal fd=%d...\n" , fd );	
		int sig,ret,i;
		char signals[1024];
		ret = recv( aEvBase.sig_pipefd[0], signals, sizeof( signals ), 0 );
		if( ret == -1 )
		{
		  return;
		}
		else if( ret == 0 )
		{
		  return;
		}
		else
		{
			for(  i = 0; i < ret; ++i )
			{
				switch( signals[i] )
				{
					//child process stoped
					case SIGCHLD:
					{
						printf( "Master recv child process stoped signal\n");
						pid_t pid;
						int stat,pidx;
						//WNOHANG
						while ( ( pid = waitpid( -1, &stat, WNOHANG ) ) > 0 )
						{
							for( pidx = 0; pidx < WORKER_PROCESS_COUNT; ++pidx )
							{
								if( aEvBase.worker_process[pidx].pid == pid )
								{
									//aeDeleteFileEvent( aEvBase.el,aEvBase.worker_process[i].pipefd[0] ,AE_READABLE );
									close( aEvBase.worker_process[pidx].pipefd[0] );
									aEvBase.worker_process[pidx].pid = -1;
								}
						        }
						}
						aEvBase.running = 0;
						aeStop( aEvBase.el );
						break;
					}
					case SIGTERM:
					case SIGQUIT:
					case SIGINT:
					{
						int i;
						printf( "master kill all the clild now\n" );
						for( i = 0; i < WORKER_PROCESS_COUNT; ++i )
						{
							int pid = aEvBase.worker_process[i].pid;
							//alarm/kill send signal to process,
							//in child process need install catch signal functions;
							kill( pid, SIGTERM );
						}
						break;
					}
					default:
					{
						break;
					}
				}
			}
		} 
	}
	return;
}


void runMasterLoop()
{
	int res;
	//listenfd event
	res = aeCreateFileEvent( aEvBase.el, aEvBase.listenfd,AE_READABLE,onReadableEvent,NULL);
	printf("master create file event is ok? [%d]\n",res==0 );
	
	//timer event
	//res = aeCreateTimeEvent( aEvBase.el,5*1000,timerCallback,NULL,finalCallback);
	//printf("master create time event is ok? [%d]\n",!res);

	aeMain( aEvBase.el);
	aeDeleteEventLoop( aEvBase.el );
}


//此处send是发给了主进程的event_loop，而不是发给子进程的。
void masterSignalHandler( int sig )
{
    printf( "Master Send Sigal to Master Loop...\n");
    int save_errno = errno;
    int msg = sig;
    send( aEvBase.sig_pipefd[1], ( char* )&msg, 1, 0 );
    errno = save_errno;
}


//这里的信号是server管理员执行的中断等操作引起的事件。
//所以此处的addEvent是加入到主进程event_loop中的。
void installMasterSignal( aeEventLoop *l )
{
    int ret;
    char neterr[1024];
    ret  = socketpair( PF_UNIX, SOCK_STREAM, 0, aEvBase.sig_pipefd );
    assert( ret != -1 );
    anetNonBlock( neterr, aEvBase.sig_pipefd[1] );
	
     //把信号管道一端加到master event_loop中，使其被epoll关注
    ret = aeCreateFileEvent(l,aEvBase.sig_pipefd[0],AE_READABLE,onReadableEvent,NULL);
	
     //装载信号，指定回调函数,如果用户引发信号事件，则回调。
     addSignal( SIGCHLD, masterSignalHandler , 1 );	//catch child process exit event
     addSignal( SIGTERM, masterSignalHandler , 1 );  //catch exit event by kill or Ctrl+C ..
     addSignal( SIGINT,  masterSignalHandler , 1 );
//     addSignal( SIGQUIT , masterSignalHandler, 1 );
     addSignal( SIGPIPE, SIG_IGN , 1 );
}

aeServer* aeServerCreate( char* ip,int port )
{
    aeServer* serv = (aeServer*)zmalloc( sizeof(aeServer ));
    //aeServer serv;
    serv->runForever = startServer;
    serv->send = anetWrite;
    serv->close = freeClient;
    serv->listen_ip = ip;
    serv->port = port; 
    bzero( &aEvBase , sizeof( aEvBase ));
    aEvBase.serv = serv;    


    if( aEvBase.running )
    {
	  exit( 0 );
    }
    aEvBase.running = 1;
    aEvBase.pid = getpid();
    aEvBase.usable_cpu_num = sysconf(_SC_NPROCESSORS_ONLN);
    serv->connlist = shm_calloc( 1024 , sizeof( userClient ));


    aEvBase.el = aeCreateEventLoop( 1024 );
    aeSetBeforeSleepProc(aEvBase.el,initOnLoopStart );

    //install signal
    installMasterSignal( aEvBase.el );
    return aEvBase.serv;
}


//装载子进程
void installWorkerProcess()
{
    char* neterr;
    int ret,i;
    for(  i = 0; i < WORKER_PROCESS_COUNT; ++i )
    {
        ret = socketpair( PF_UNIX, SOCK_STREAM, 0, aEvBase.worker_process[i].pipefd );
        assert( ret != -1 );
        aEvBase.worker_process[i].pid = fork();
        if( aEvBase.worker_process[i].pid < 0 )
        {
            continue;
        }
        else if( aEvBase.worker_process[i].pid > 0 )
        {
			//父进程
            close( aEvBase.worker_process[i].pipefd[1] );
            anetNonBlock( neterr , aEvBase.worker_process[i].pipefd[0] );
            continue;
        }
        else
        {
			//子进程
            close( aEvBase.worker_process[i].pipefd[0] );
            anetNonBlock( neterr, aEvBase.worker_process[i].pipefd[1] );
            runWorkerProcess( i , aEvBase.worker_process[i].pipefd[1]  );
            exit( 0 );
        }
    }
}


int startServer( aeServer* serv )
{	
	int sockfd[2];
	int sock_count = 0;          

	listenToPort( serv->listen_ip, serv->port , sockfd , &sock_count );
	aEvBase.listenfd = sockfd[0];
	aEvBase.listenIPv6fd = sockfd[1];
	printf( "master listen count %d,listen fd %d , pid=%d \n",sock_count,aEvBase.listenfd,aEvBase.pid  );

 
	installWorkerProcess();	
	runMasterLoop();
	puts("Master Exit ,Everything is ok !!!\n");
	shm_free( serv->connlist );
	return 0;
}
