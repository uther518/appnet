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

$server->setOption( APPNET_OPT_WORKER_NUM , 1 );
$server->setOption( APPNET_OPT_REACTOR_NUM, 1 );
$server->setOption( APPNET_OPT_MAX_CONNECTION , 10000 );
$server->setOption( APPNET_OPT_PROTO_TYPE , APPNET_PROTO_TCP_ONLY );
$server->setOption( APPNET_OPT_PROTO_TYPE , APPNET_PROTO_HTTP_ONLY );
$server->setOption( APPNET_OPT_PROTO_TYPE , APPNET_PROTO_WEBSOCKET_ONLY );

$info = $server->getInfo();

$server->on( APPNET_EVENT_CONNECT , function( $server , $fd )
{ 
	$pid = posix_getpid();
	echo "Client:Connect:{$fd} pid={$pid} \n"; 
});

$server->on( APPNET_EVENT_RECV , function( $server , $fd , $buffer )
{
	echo "Client Recv:[{$buffer}][{$fd}] \n";
	$header = $server->getHeader();
	if( $header['Protocol'] == "WEBSOCKET" )
	{
	   Websocket::onReceive( $server , $fd,  $buffer );
	}
	else
	{
           $server->send( $fd , $buffer );
	}
});

$server->on( APPNET_EVENT_CLOSE , function( $server , $fd )
{ 
	echo "Client Close:$fd \n";
});

//on worker run start,you can init env.
$server->on( APPNET_EVENT_START , function( $server  )
{
	$pid = posix_getpid();
        echo "On Worker Start!! pid={$pid} \n";

	//3000ms means 3second	
	$server->timerAdd( 3000 , "onTimerCallback" , "paramsxxx" );
});

//on worker shutdown,you must save data in last time.
$server->on( APPNET_EVENT_FINAL , function( $server  )
{
	$pid = posix_getpid();
        echo "On Worker Final!! pid={$pid} \n";
});

function onTimerCallback( $server , $timer_id ,  $params )
{
	$pid = posix_getpid();
	echo "onTimerCallback  ok,worker pid={$pid},timer_id={$timer_id}...\n";
	
	//if do not remove it, it will be call this function forever	
	$server->timerRemove( $timer_id );		
}
$server->run();


?>
