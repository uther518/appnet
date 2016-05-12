<?php

class Websocket
{

	private static $serv;

	private static $fd;

	private static $connections;

  	public static function onReceive(  $serv , $fd , $buffer )
	{
		$msg = json_decode( $buffer , true );
		if( empty( $msg ) || $msg == false )
		{
		   return;
		}
	
		if( $msg['cmd'] == 'login' )
		{
		   self::onLogin( $serv , $fd , $msg );
		}
		elseif( $msg['cmd'] == 'getOnline')
		{
		   self::getOnline( $serv ,$fd , $msg ); 
		}
		elseif( $msg['cmd'] == 'message')
		{
		   self::onMessage( $serv ,$fd , $msg );
		}	

	}

	public static function getOnline( $serv , $fd , $msg )
	{
	        $resMsg = array(
			'cmd' => 'getOnline',
		);
		foreach ( self::$connections as $clid => $info )
		{
			  $resMsg['list'][] = array(
				   'fd' => $clid,
				   'name' => $info['name'],
				   'avatar' => $info['avatar'],
			  );
	   }
	   $serv->send( $fd , json_encode( $resMsg ) );
	}

	public static function onLogin( $serv , $fd , $msg )
	{
		self::$connections[$fd]['name'] = $msg['name'];
        	self::$connections[$fd]['avatar'] = $msg['avatar'];

		$resMsg = array(
				'cmd' => 'login',
				'fd' => $fd,
				'name' => $msg['name'],
				'avatar' => $msg['avatar'],
			 );
		$len = $serv->send(  $fd , json_encode( $resMsg )  );
		self::broadcastNewUser( $serv , $fd , $msg , $resMsg );
	}

	public static function broadcastNewUser( $serv , $fd , $msg , $resMsg )
	{
		$resMsg['cmd'] = 'newUser';
		$loginMsg = array(
			'cmd' => 'fromMsg',
			'from' => 0,
			'channal' => 0 ,
			'data' => $msg['name']."上线鸟。。",
		);

		//将上线消息发送给所有人
		foreach ( self::$connections as $clid => $info )
		{
			if( $fd  !=  $clid )
			{
				$serv->send( $clid , json_encode( $resMsg ) );
				$serv->send( $clid , json_encode( $loginMsg ) );
			}
		}
	}

	public static function broadcastOffLine( $serv , $fd  )
	{
                $resMsg = array(
                        'cmd' => 'offline',
                        'fd' => $fd,
			'from' => 0,
                        'channal' => 0 ,
                        'data' => self::$connections[$fd]['name']."离开了。。",
                );

                //将下线消息发送给所有人
                foreach ( self::$connections as $clid => $info )
                {
                        if( $fd  !=  $clid )
                        {
                                $serv->send( $clid , json_encode( $resMsg ) );
                        }
                }
	}

	public static function onMessage( $serv , $fd , $msg )
	{
		$resMsg = $msg;
		$resMsg['cmd'] = 'fromMsg';
		//表示群发
		if( $msg['channal'] == 0 )
		{
			foreach ( self::$connections as $clid => $info )
			{
				$serv->send( $clid , json_encode( $resMsg ) );
			}
		}
		//表示私聊
		elseif ( $msg['channal'] == 1 )
		{
			$serv->send( $msg['to'] , json_encode( $resMsg ) );
			$serv->send( $msg['from'] , json_encode( $resMsg ) );
		}

		//$serv->close( $fd );
    }
}


function onTaskCallback( $server , $data , $taskid , $from )
{
	 $pid = posix_getpid();
	 echo "onTaskCallback data=[{$data}],taskid=[{$taskid}],from=[{$from}],pid={$pid} \n";
}

function onConnect( $server , $fd )
{
	$pid = posix_getpid();
        echo "\n\nClient Connect:{$fd} pid={$pid} \n"; 
}

//在worker进程中，表示回调，在task进程中表示请求
function onTask( $server, $data , $taskid, $from )
{
	$pid = posix_getpid();
	echo "onTask data=[{$data}],taskid=[{$taskid}],from=[{$from}],pid={$pid} \n";	
	//sleep( 3 );
	$ret = array(
	   'status' => 'ok',
	   'data' => array( 'a' => "aaaa" ),
	);
	$server->taskCallback( json_encode( $ret ) , $taskid , $from  );
}

function onRecv( $server , $fd , $buffer )
{
	$header = $server->getHeader();
	$pid = posix_getpid();
	echo "Client Recv:[{$header['Protocol']}][{$header['Uri']}][{$buffer}][{$fd}],pid={$pid} \n";	

	if( $header['Protocol'] == "WEBSOCKET" )
	{
	  	 Websocket::onReceive( $server , $fd,  $buffer );
	}
	elseif(  $header['Protocol'] == "HTTP"  )
    	{
		$data  = $buffer;
		$data .= microtime();
	//	$server->addAsynTask( $data , 1 );	
	//	$server->setHeader( "Connection" , "keep-alive" );
		$server->send( $fd , $data );	
	}
	else
	{
        	$server->send( $fd , $buffer );
	}
};

function onClose( $server , $fd )
{ 
	echo "Client Close:$fd \n";
	$header = $server->getHeader();
	if( $header['Protocol'] == "WEBSOCKET" )
    {
        Websocket::broadcastOffLine( $server , $fd  );
	}
};

function onStart( $server  )
{
	$pid = posix_getpid();
        echo "On Worker Start!! pid={$pid} \n";
	//3000ms means 3second	
	$server->timerAdd( 3000 , "onTimerCallback" , "paramsxxx" );
};

//on worker shutdown,you must save data in last time.
function onFinal( $server  )
{
	$pid = posix_getpid();
    echo "On Worker Final!! pid={$pid} \n";
};

function onTimerCallback( $server , $timer_id ,  $params )
{
	$pid = posix_getpid();
	echo "onTimerCallback  ok,worker pid={$pid},timer_id={$timer_id}...\n";
	//if do not remove it, it will be call this function forever	
	$server->timerRemove( $timer_id );		
}


dl( "appnet.so");
$server = new appnetServer( "0.0.0.0" , 3011 );

$server->setOption( APPNET_OPT_WORKER_NUM , 2 );
$server->setOption( APPNET_OPT_ATASK_WORKER_NUM , 2 );

$server->setOption( APPNET_OPT_REACTOR_NUM, 2 );
$server->setOption( APPNET_OPT_MAX_CONNECTION , 10000 );
$server->setOption( APPNET_OPT_PROTO_TYPE , APPNET_PROTO_MIX );

$server->setOption( APPNET_HTTP_DOCS_ROOT , $_SERVER['PWD']."/example/www/" );
$server->setOption( APPNET_HTTP_UPLOAD_DIR, "/home/upload/"  );
$server->setOption( APPNET_HTTP_404_PAGE , "404.html" );
$server->setOption( APPNET_HTTP_50X_PAGE , "50x.html" );
$server->setOption( APPNET_HTTP_KEEP_ALIVE , 1 );

$server->addEventListener( APPNET_EVENT_CONNECT , "onConnect");
$server->addEventListener( APPNET_EVENT_RECV ,    "onRecv");
$server->addEventListener( APPNET_EVENT_CLOSE ,   "onClose");
$server->addEventListener( APPNET_EVENT_START ,   "onStart");
$server->addEventListener( APPNET_EVENT_FINAL ,   "onFinal");
$server->addEventListener( APPNET_EVENT_TASK ,    "onTask");
$server->addEventListener( APPNET_EVENT_TASK_CB , "onTaskCallback" );
$server->run();

$info = $server->getInfo();


?>
