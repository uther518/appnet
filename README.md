# appnet
apple network &amp; application network

QQ群:379084776


$server = new appTcpServer( "0.0.0.0" , 3011 );

$server->on('connect', function( $serv , $fd ){
    echo "Client:Connect:{$fd} \n";
});


$server->on('receive', function( $serv , $fd , $buffer ){
    echo "Client Recv:[{$buffer}][{$fd}] \n";
    $server->send( $fd , "xxxxxxxx" );
});


$server->on( 'close' , function( $serv , $fd ){
    echo "Client Close:$fd";
});

$server->run();
