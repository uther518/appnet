<?php

dl( "appnet.so");
$server = new appTcpServer( "0.0.0.0" , 3011 );

$server->on('connect', function( $serv , $fd ){ 
	$pid = posix_getpid();
	echo "Client:Connect:{$fd} pid={$pid} \n"; 
});
$server->on('receive', function( $serv , $fd , $buffer ){
	echo "Client Recv:[{$buffer}][{$fd}] \n";
       
	$len = $serv->send(  $fd , $buffer );
	echo "Send len=".$len."\n";
      
    	list( $cmd , $id ) = explode( ":" , $buffer );
	if( trim($cmd) == 'close' && $id > 0 )
	{
		 $id = intval( $id );
       		 $serv->close( $id );
		 echo "closeid:".$id."\n";
	}
});

$server->on( 'close' , function( $serv , $fd ){ 
	echo "Client Close:$fd \n";
});

$server->run();





?>
