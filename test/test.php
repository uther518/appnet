<?php
$server = new appTcpServer( "0.0.0.0" , 3011 );

$server->on('connect', function( $serv , $fd ){ 
	echo "Client:Connect:{$fd} \n"; 
});
$server->on('receive', function( $serv , $fd , $buffer ){
	echo "Client Recv:[{$buffer}][{$fd}] \n";
	$serv->send(  $fd , "server send:".$buffer );
});

$server->on( 'close' , function( $serv , $fd ){ 
	echo "Client Close:$fd";
});

$server->run();






?>
