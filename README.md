# appnet介绍

appnet是一个基于linux epoll的多线程的高性能异步网络事件库，目标是用高性能的PHP版本
搭载appnet取代C/lua或C/python模式,快速构建强有力的长连接服务器，以弥补PHP固有的缺陷。
使其可广泛用于聊天系统，游戏服务器，消息通知服务器等实时通信场景。可对网络IO密集性场景
或CPU密集性场景配置reactor数量和woker数量的比例，使硬件运行于最佳状态。

其特点有
[1],高性能,纯C语言开发,epoll异步事件通知机制,可充分利用多核CPU。
[2],易用性,使用方式简单,并提供PHP7.0版本扩展。
[3],高并发,多线程网络IO,Per Thread One Loop并发模型，多个worker进程并行处理业务。
[4],多协议,可混合TCP协议，websocket协议和简单http协议与服务器通信。
[5],内存优化，进程间通信使用共享内存，兼容jemalloc和tcmalloc内存优化技术。

与其它网络库粗略比较
[1]，相比node.js单进程模式，appnet可充分利用多核CPU，不致于服务器资源浪费。
[2], 相比nginx多进程+fpm多进程，支持TCP协议，websocket协议等长连接。
[3], 相比libevent，原生支持多线程无需再次封装,支持混合协议。
[4], 相比memcache多线程+libevent模型，appnet支持多任务处理业务，具有更少的封装。
[5], 相比redis单进程网络模型，支持多线程

以上是一个美好的愿望，目前websocket协议和http协议尚未实现，还存已知的在进程终止时有可能会
内存泄露BUG，目前是一个人开发，也缺少更多的测试，希望有兴趣的同学一起参与进来测试,开发和提建议。
让PHP真正成为世界上最好的语言，没有之一。。

安装方法:
1,源码安装php_7.0.x
2,下载扩展到任意目录appnet_php7
3,执行如下指令:
 >cd appnet_php7
 >/usr/local/php7/bin/phpize
 >./configure --with-php-config=/usr/local/php7/bin/php-config 
 >make
 >make install
 >php test/test.php
 >telnet 127.0.0.1 3011
 开始测试吧。
 

<?php

dl( "appnet.so" );
$server = new appTcpServer( "0.0.0.0" , 3011 );
$server->on('connect', function( $serv , $fd )
{
    echo "Client:Connect:{$fd} \n";
});

$server->on('receive', function( $serv , $fd , $buffer )
{    
	echo "Client Recv:[{$buffer}][{$fd}] \n";
	$server->send( $fd , "hellow world!" );
	//close method
	//$server->close( $fd );
});

$server->on( 'close' , function( $serv , $fd )
{
    echo "Client Close:$fd";
});

$server->run();

?>


QQ交流群:379084776