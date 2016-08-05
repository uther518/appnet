<?php

/**
  一个同时支持tcp,http,websocket的Appnet Server
  需求安装的扩展:
  1,appnet.so  
  2,多worker进程共享数据可以使用apcu,如下WebChat中的用法
    apcu下载地址:http://pecl.php.net/package/APCu
    php.ini配置:
    apc.enabled=1
	apc.shm_size=32M
	apc.enable_cli=1
**/

error_reporting(E_ERROR | E_WARNING | E_PARSE);
define( "WORKER_NUM" , 1 );

class WebChat
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
		elseif( $msg['cmd'] == 'disconnect' )
		{
		    self::broadcastOffLine( $serv , $fd  );
		    $serv->close( $fd );
		}	

	}

	public static function getOnline( $serv , $fd , $msg )
	{
	    $resMsg = array(
			'cmd' => 'getOnline',
		);
		$list = self::getConnlist();
		foreach ( $list as $clid => $info )
		{
			$resMsg['list'][] = array(
			   'fd' => $clid,
			   'name' => $info['name'],
			   'avatar' => $info['avatar'],
			);
	   }
	   $serv->send( $fd , json_encode( $resMsg ) );
	}


	private static function setConnlist( $fd , $conn )
	{
		if( WORKER_NUM > 1 )
		{
			$list = apcu_fetch( "webchat_conn_list"  );
			$list[$fd] = $conn;
			apcu_store( "webchat_conn_list" ,  $list );
		}
		else
		{
			self::$connections[$fd] = $conn;
		}
	}


	private static function delConnlist( $fd )
        {
                if( WORKER_NUM > 1 )
                {
                        $list = apcu_fetch( "webchat_conn_list"  );
                        unset( $list[$fd] );
                        apcu_store( "webchat_conn_list" ,  $list );
                }
                else
                {
                       unset( self::$connections[$fd] );
                }
        }


	private static function getConnlist()
	{
		if(  WORKER_NUM > 1 )
		{
			$list = apcu_fetch( "webchat_conn_list"  );
			return $list;
		}
		return self::$connections;
	}

	public static function onLogin( $serv , $fd , $msg )
	{
		$list = self::getConnlist();
		if(  $list  )
		{
			foreach( $list as $connfd => $info )
			{
				if( $info['session_id'] == $msg['session_id'] && $fd != $connfd )
				{
					echo "login repeat by same brower";
					self::broadcastOffLine( $serv , $connfd  );
				}
			}
		}


		self::setConnlist( $fd , array( 'name' => $msg['name'] , 'avatar' => $msg['avatar'] , 'session_id' => $msg['session_id'] ));

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

		$list = self::getConnlist();
		//将上线消息发送给所有人
		foreach ( $list as $clid => $info )
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
		$list = self::getConnlist();
		if(empty($list[$fd]))return;

		$resMsg = array(
			'cmd' => 'offline',
			'fd' => $fd,
			'from' => 0,
			'channal' => 0 ,
			'data' => $list[$fd]['name'],
		);
		
		//将下线消息发送给所有人
		foreach ( $list as $clid => $info )
		{
			if( $fd  !=  $clid )
			{
				$serv->send( $clid , json_encode( $resMsg ) );
			}
		}
		unset( $list[$fd] );	
		self::delConnlist( $fd  );
	}

	public static function onMessage( $serv , $fd , $msg )
	{
		$resMsg = $msg;
		$resMsg['cmd'] = 'fromMsg';
		//表示群发
		if( $msg['channal'] == 0 )
		{

			$list = self::getConnlist();
			foreach ( $list as $clid => $info )
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
    echo "\n\nNew Connect:{$fd} pid={$pid} \n"; 
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
	echo "RecvClient:[{$header['Protocol']}][{$buffer}][{$fd}],pid={$pid} \n";	

	if( $header['Protocol'] == "WEBSOCKET" )
	{
	  	 WebChat::onReceive( $server , $fd,  $buffer );
	}
	elseif(  $header['Protocol'] == "HTTP"  )
	{
		if( $header['Uri'] == '/' )
		{
			/*redirct url,httpRedirect( $url , $status ),$status can be 301 or 302*/
			$server->httpRedirect( "/website/index.html" );
			
			/*response a error header code if you need*/
			//$server->httpRespCode( 403 );
			return;
		}
		
		$data  = $buffer;
		$data .= microtime();
		echo "Response:[".$data."]\n";
		
		/*add async task to task worker process*/
		//$server->addAsynTask( $data , 1 );	
		$server->setHeader( "Connection" , "keep-alive" );
		//ajax访问时，会有跨域问题
		$server->setHeader( "Access-Control-Allow-Origin" , "*" );
		$server->send( $fd , $data );	
	}
	else
	{
		$server->send( $fd , $buffer );
	}
};

function onClose( $server , $fd )
{ 
	echo "CloseClient:$fd \n";
	$header = $server->getHeader();
	if( $header['Protocol'] == "WEBSOCKET" )
    	{
		WebChat::broadcastOffLine( $server , $fd  );
	}
};

function onStart( $server  )
{
	$pid = posix_getpid();
	echo "On Worker Start!! pid={$pid} \n";
	//3000ms means 3second	
	$server->timerAdd( 3000 , "flag or json data" );
};

//on worker shutdown,you must save data in last time.
function onFinal( $server  )
{
	$pid = posix_getpid();
    	echo "On Worker Final!! pid={$pid} \n";
};

function onTimer( $server , $timer_id ,  $flag )
{
	$pid = posix_getpid();
	echo "onTimer:flag={$flag},pid={$pid},timer_id={$timer_id}...\n";
	//if do not remove it, it will be call this function forever	
	$server->timerRemove( $timer_id );		
}


//dl( "appnet.so");
$server = new AppnetServer( "0.0.0.0" , 3011 );

$server->setOption( APPNET_OPT_DAEMON , 0 );
$server->setOption( APPNET_OPT_WORKER_NUM , WORKER_NUM );
$server->setOption( APPNET_OPT_TASK_WORKER_NUM , 0 );

$server->setOption( APPNET_OPT_REACTOR_NUM, 1 );
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
$server->addEventListener( APPNET_EVENT_TIMER ,   "onTimer");
$server->addEventListener( APPNET_EVENT_TASK ,    "onTask");
$server->addEventListener( APPNET_EVENT_TASK_CB , "onTaskCallback" );
$server->run();

$info = $server->getInfo();


?>
