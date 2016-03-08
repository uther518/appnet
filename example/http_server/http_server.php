<?php

include "dispacher.php"

function onConnect( $server , $fd )
{
	$pid = posix_getpid();
    echo "Client Connect:{$fd} pid={$pid} \n"; 
}

function onRequest( $server , $fd , $data )
{
	$ret = Dispacher::request( $data  );
	$ret = $ret ? $ret : "";
    $server->send( $fd , $ret );
};

function onClose( $server , $fd )
{ 
	echo "Client Close:$fd \n";
};

//on worker run start,you can init env.
function onStart( $server  )
{
	$pid = posix_getpid();
    echo "On Worker Start!! pid={$pid} \n";
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
}


dl( "appnet.so");
$server = new appnetServer( "0.0.0.0" , 3011 );

$server->setOption( APPNET_OPT_WORKER_NUM , 1 );
$server->setOption( APPNET_OPT_REACTOR_NUM, 1 );
$server->setOption( APPNET_OPT_MAX_CONNECTION , 10000 );
$server->setOption( APPNET_OPT_PROTO_TYPE , APPNET_PROTO_MIX );



$server->addEventListener( APPNET_EVENT_CONNECT , "onConnect");
$server->addEventListener( APPNET_EVENT_RECV ,    "onRequest");
$server->addEventListener( APPNET_EVENT_CLOSE ,   "onClose");
$server->addEventListener( APPNET_EVENT_START ,   "onStart");
$server->addEventListener( APPNET_EVENT_FINAL ,   "onFinal");
$server->run();

$info = $server->getInfo();


?>
