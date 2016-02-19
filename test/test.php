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




dl( "appnet.so");
$server = new appnetServer( "0.0.0.0" , 3011 );
$server->setOption( appnetServer::OPT_WORKER_NUM , 1 );
$server->setOption( appnetServer::OPT_REACTOR_NUM, 1 );
$server->setOption( appnetServer::OPT_MAX_CONNECTION , 10000 );
$server->setOption( appnetServer::OPT_PROTOCOL_TYPE , appnetServer::PROTOCOL_TYPE_TCP_ONLY );
$server->setOption( appnetServer::OPT_PROTOCOL_TYPE , appnetServer::PROTOCOL_TYPE_HTTP_ONLY );
$server->setOption( appnetServer::OPT_PROTOCOL_TYPE , appnetServer::PROTOCOL_TYPE_WEBSOCKET_ONLY );

$info = $server->getInfo();
print_r( $info );

$server->on('connect', function( $serv , $fd )
{ 
	$pid = posix_getpid();
	echo "Client:Connect:{$fd} pid={$pid} \n"; 
});

$server->on('receive', function( $serv , $fd , $buffer )
{
	echo "Client Recv:[{$buffer}][{$fd}] \n";
	$header = $serv->getHeader();
	if( $header['Protocol'] == "WEBSOCKET" )
	{
	   Websocket::onReceive( $serv , $fd,  $buffer );
	}
	else
	{
           $serv->send( $fd , $buffer );
	}
});

$server->on( 'close' , function( $serv , $fd )
{ 
	echo "Client Close:$fd \n";
});

//on worker run start,you can init env.
$server->on( 'start' , function( $serv  )
{
	$pid = posix_getpid();
        echo "On Worker Start!! pid={$pid} \n";

	//3000ms means 3second	
	$serv->timerAdd( 3000 , "onTimerCallback" , "paramsxxx" );
});

//on worker shutdown,you must save data in last time.
$server->on( 'final' , function( $serv  )
{
	$pid = posix_getpid();
        echo "On Worker Final!! pid={$pid} \n";
});

function onTimerCallback( $serv , $timer_id ,  $params )
{
	$pid = posix_getpid();
	echo "onTimerCallback  ok,worker pid={$pid},timer_id={$timer_id}...\n";
	
	//if do not remove it, it will be call this function forever	
	$serv->timerRemove( $timer_id );		
}
$server->run();


?>
