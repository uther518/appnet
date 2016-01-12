# appnet
apple network &amp; application network

QQ群:379084776
安装方法:
1,源码安装php_7.0.x
2,下载扩展到任意目录appnet
3,执行如下指令:
 >cd appnet
 >phpize
 >./configure --with-php-config=/usr/local/php7/bin/php-config 
 >make
 >make install

<?php
dl( "appnet.so" );
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

?>
