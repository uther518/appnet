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


	public static function onMessage( $serv , $fd , $msg )
	{

		echo "onMessage php.................\n";
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


function onConnect( $server , $fd )
{
	$pid = posix_getpid();
        echo "Client Connect:{$fd} pid={$pid} \n"; 
}

function onRecv( $server , $fd , $buffer )
{
	$header = $server->getHeader();
	echo "Client Recv:[{$header['Protocol']}][{$header['Uri']}][{$buffer}][{$fd}] \n";	

	if( $header['Protocol'] == "WEBSOCKET" )
	{
	   Websocket::onReceive( $server , $fd,  $buffer );
	}
	elseif(  $header['Protocol'] == "HTTP"  )
        {
		$data  = $buffer;
		$server->setHeader( "Connection" , "keep-alive" );
		$server->send( $fd , "xxxxxxxxxx".$data );	
	}
	else
	{
        	$server->send( $fd , $buffer );
	}
};

function onClose( $server , $fd )
{ 
	echo "Client Close:$fd \n";
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

$server->setOption( APPNET_OPT_WORKER_NUM , 1 );
$server->setOption( APPNET_OPT_REACTOR_NUM, 1 );
$server->setOption( APPNET_OPT_MAX_CONNECTION , 100 );
$server->setOption( APPNET_OPT_PROTO_TYPE , APPNET_PROTO_MIX );

$server->setOption( APPNET_HTTP_DOCS_ROOT , $_SERVER['PWD']."/example/www/" );
$server->setOption( APPNET_HTTP_UPLOAD_DIR, "/home/www/"  );
$server->setOption( APPNET_HTTP_404_PAGE , "404.html" );
$server->setOption( APPNET_HTTP_50X_PAGE , "50x.html" );
$server->setOption( APPNET_HTTP_KEEP_ALIVE , 1 );

$server->addEventListener( APPNET_EVENT_CONNECT , "onConnect");
$server->addEventListener( APPNET_EVENT_RECV ,    "onRecv");
$server->addEventListener( APPNET_EVENT_CLOSE ,   "onClose");
$server->addEventListener( APPNET_EVENT_START ,   "onStart");
$server->addEventListener( APPNET_EVENT_FINAL ,   "onFinal");
$server->run();

$info = $server->getInfo();


?>
