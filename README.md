<p># appnet介绍<br /><br />appnet是一个基于linux epoll的多线程的高性能异步网络库，目标是用php7+appnet快速构建高性能的长连接服务器。使其可广泛用于聊天系统，游戏服务器，消息通知服务器等实时通信场景。</p>
<p><img src="http://images2015.cnblogs.com/blog/234056/201604/234056-20160428172712252-741219753.jpg" alt="" width="959" height="638" /></p>
<p><br />其特点有</p>
<ul>
<li>高性能,核心用纯C语言开发,epoll异步非阻塞事件通知机制,单线程可支撑10万并发连接</li>
<li>易用性,使用方式简单,并提供PHP7.0版本扩展,简单几步就可塔建一个功能齐全的长连接服务器,不再需要nginx,apache,php-fpm等。</li>
<li>高并发,多线程异步网络IO,Per Thread One Loop并发模型，多个worker进程并行处理业务。</li>
<li>多协议,可混合TCP协议，websocket协议和简单http协议与服务器通信。</li>
<li>内存优化，进程间通信使用共享内存，兼容jemalloc和tcmalloc内存优化技术。</li>
<li>缓冲区优化，采用redis的动态缓冲区，根据数据包大小自动扩充，有效避免内存浪费和缓冲区溢出,其内存预分配策略降低了内存分配次数。从而提高内存分配效率。</li>
<li>异步任务，耗时的任务可以投递到单独的任务进程异步处理，工作进程无需等待。</li>










</ul>
<p><br />安装方法:<br />1,源码安装php_7.0.x<br />2,下载扩展到任意目录appnet_php7<br />3,执行如下指令:<br />&nbsp;&gt;cd appnet_php7<br />&nbsp;&gt;/usr/local/php7/bin/phpize<br />&nbsp;&gt;./configure --with-php-config=/usr/local/php7/bin/php-config<br />&nbsp;&gt;make<br />&nbsp;&gt;make install<br />&nbsp;&gt;php test/test.php<br />&nbsp;&gt;telnet 127.0.0.1 3011<br />&nbsp;开始测试吧。</p>
<div class="cnblogs_code">
<pre>&lt;?<span style="color: #000000;">php

function onTaskCallback( $server , $data , $taskid , $</span><span style="color: #0000ff;">from</span><span style="color: #000000;"> )
{
　　$pid </span>=<span style="color: #000000;"> posix_getpid();
　　echo </span><span style="color: #800000;">"</span><span style="color: #800000;">onTaskCallback data=[{$data}],taskid=[{$taskid}],from=[{$from}],pid={$pid} \n</span><span style="color: #800000;">"</span><span style="color: #000000;">;
}

function onConnect( $server , $fd )
{
　　$pid </span>=<span style="color: #000000;"> posix_getpid();
　　echo </span><span style="color: #800000;">"</span><span style="color: #800000;">\n\nClient Connect:{$fd} pid={$pid} \n</span><span style="color: #800000;">"</span><span style="color: #000000;">; 
}

</span><span style="color: #008000;">//</span><span style="color: #008000;">在worker进程中，表示回调，在task进程中表示请求</span>
function onTask( $server, $data , $taskid, $<span style="color: #0000ff;">from</span><span style="color: #000000;"> )
{
　　$pid </span>=<span style="color: #000000;"> posix_getpid();
　　echo </span><span style="color: #800000;">"</span><span style="color: #800000;">onTask data=[{$data}],taskid=[{$taskid}],from=[{$from}],pid={$pid} \n</span><span style="color: #800000;">"</span><span style="color: #000000;">;    
　　</span><span style="color: #008000;">//</span><span style="color: #008000;">sleep( 3 );</span>
　　$ret =<span style="color: #000000;"> array(
　　　　</span><span style="color: #800000;">'</span><span style="color: #800000;">status</span><span style="color: #800000;">'</span> =&gt; <span style="color: #800000;">'</span><span style="color: #800000;">ok</span><span style="color: #800000;">'</span><span style="color: #000000;">,
　　　　</span><span style="color: #800000;">'</span><span style="color: #800000;">data</span><span style="color: #800000;">'</span> =&gt; array( <span style="color: #800000;">'</span><span style="color: #800000;">a</span><span style="color: #800000;">'</span> =&gt; <span style="color: #800000;">"</span><span style="color: #800000;">aaaa</span><span style="color: #800000;">"</span><span style="color: #000000;"> ),
　　);
　　$server</span>-&gt;taskCallback( json_encode( $ret ) , $taskid , $<span style="color: #0000ff;">from</span><span style="color: #000000;"> );
}

function onRecv( $server , $fd , $buffer )
{
　　$header </span>= $server-&gt;<span style="color: #000000;">getHeader();
　　$pid </span>=<span style="color: #000000;"> posix_getpid();
　　echo </span><span style="color: #800000;">"</span><span style="color: #800000;">Client Recv:[{$header['Protocol']}][{$header['Uri']}][{$buffer}][{$fd}],pid={$pid} \n</span><span style="color: #800000;">"</span><span style="color: #000000;">;

　　</span><span style="color: #0000ff;">if</span>( $header[<span style="color: #800000;">'</span><span style="color: #800000;">Protocol</span><span style="color: #800000;">'</span>] == <span style="color: #800000;">"</span><span style="color: #800000;">WEBSOCKET</span><span style="color: #800000;">"</span><span style="color: #000000;"> )
　　{
　　　　</span><span style="color: #008000;">//</span><span style="color: #008000;">Websocket::onReceive( $server , $fd, $buffer );</span>
<span style="color: #000000;">　　}
　　elseif( $header[</span><span style="color: #800000;">'</span><span style="color: #800000;">Protocol</span><span style="color: #800000;">'</span>] == <span style="color: #800000;">"</span><span style="color: #800000;">HTTP</span><span style="color: #800000;">"</span><span style="color: #000000;"> )
　　{
　　　　$data </span>=<span style="color: #000000;"> $buffer;
　　　　$server</span>-&gt;addAsynTask( $data , <span style="color: #800080;">1</span><span style="color: #000000;"> );

　　　　$server</span>-&gt;setHeader( <span style="color: #800000;">"</span><span style="color: #800000;">Connection</span><span style="color: #800000;">"</span> , <span style="color: #800000;">"</span><span style="color: #800000;">keep-alive</span><span style="color: #800000;">"</span><span style="color: #000000;"> );
　　　　$server</span>-&gt;<span style="color: #000000;">send( $fd , $data );    
　　}
　　</span><span style="color: #0000ff;">else</span><span style="color: #000000;">
　　{
　　　　$server</span>-&gt;<span style="color: #000000;">send( $fd , $buffer );
　　}
};

function onClose( $server , $fd )
{ 
　　echo </span><span style="color: #800000;">"</span><span style="color: #800000;">Client Close:$fd \n</span><span style="color: #800000;">"</span><span style="color: #000000;">;
　　$header </span>= $server-&gt;<span style="color: #000000;">getHeader();
　　</span><span style="color: #0000ff;">if</span>( $header[<span style="color: #800000;">'</span><span style="color: #800000;">Protocol</span><span style="color: #800000;">'</span>] == <span style="color: #800000;">"</span><span style="color: #800000;">WEBSOCKET</span><span style="color: #800000;">"</span><span style="color: #000000;"> )
　　{
　　　　</span><span style="color: #008000;">//</span><span style="color: #008000;">Websocket::broadcastOffLine( $server , $fd );</span>
<span style="color: #000000;">　　}
};

function onStart( $server )
{
　　$pid </span>=<span style="color: #000000;"> posix_getpid();
　　echo </span><span style="color: #800000;">"</span><span style="color: #800000;">On Worker Start!! pid={$pid} \n</span><span style="color: #800000;">"</span><span style="color: #000000;">;
　　</span><span style="color: #008000;">//</span><span style="color: #008000;">3000ms means 3second    </span>
　　$server-&gt;timerAdd( <span style="color: #800080;">3000</span> , <span style="color: #800000;">"</span><span style="color: #800000;">onTimerCallback</span><span style="color: #800000;">"</span> , <span style="color: #800000;">"</span><span style="color: #800000;">paramsxxx</span><span style="color: #800000;">"</span><span style="color: #000000;"> );
};

</span><span style="color: #008000;">//</span><span style="color: #008000;">on worker shutdown,you must save data in last time.</span>
<span style="color: #000000;">function onFinal( $server )
{
　　$pid </span>=<span style="color: #000000;"> posix_getpid();
　　echo </span><span style="color: #800000;">"</span><span style="color: #800000;">On Worker Final!! pid={$pid} \n</span><span style="color: #800000;">"</span><span style="color: #000000;">;
};

function onTimerCallback( $server , $timer_id , $</span><span style="color: #0000ff;">params</span><span style="color: #000000;"> )
{
　　$pid </span>=<span style="color: #000000;"> posix_getpid();
　　echo </span><span style="color: #800000;">"</span><span style="color: #800000;">onTimerCallback ok,worker pid={$pid},timer_id={$timer_id}...\n</span><span style="color: #800000;">"</span><span style="color: #000000;">;
　　</span><span style="color: #008000;">//</span><span style="color: #008000;">if do not remove it, it will be call this function forever    </span>
　　$server-&gt;<span style="color: #000000;">timerRemove( $timer_id );    
}

dl( </span><span style="color: #800000;">"</span><span style="color: #800000;">appnet.so</span><span style="color: #800000;">"</span><span style="color: #000000;">);
$server </span>= <span style="color: #0000ff;">new</span> appnetServer( <span style="color: #800000;">"</span><span style="color: #800000;">0.0.0.0</span><span style="color: #800000;">"</span> , <span style="color: #800080;">3011</span><span style="color: #000000;"> );
$server</span>-&gt;setOption( APPNET_OPT_WORKER_NUM , <span style="color: #800080;">2</span><span style="color: #000000;"> );
$server</span>-&gt;setOption( APPNET_OPT_ATASK_WORKER_NUM , <span style="color: #800080;">2</span><span style="color: #000000;"> );
$server</span>-&gt;setOption( APPNET_OPT_REACTOR_NUM, <span style="color: #800080;">2</span><span style="color: #000000;"> );
$server</span>-&gt;setOption( APPNET_OPT_MAX_CONNECTION , <span style="color: #800080;">10000</span><span style="color: #000000;"> );
$server</span>-&gt;<span style="color: #000000;">setOption( APPNET_OPT_PROTO_TYPE , APPNET_PROTO_MIX );
$server</span>-&gt;setOption( APPNET_HTTP_DOCS_ROOT , $_SERVER[<span style="color: #800000;">'</span><span style="color: #800000;">PWD</span><span style="color: #800000;">'</span>].<span style="color: #800000;">"</span><span style="color: #800000;">/example/www/</span><span style="color: #800000;">"</span><span style="color: #000000;"> );
$server</span>-&gt;setOption( APPNET_HTTP_UPLOAD_DIR, <span style="color: #800000;">"</span><span style="color: #800000;">/home/upload/</span><span style="color: #800000;">"</span><span style="color: #000000;"> );
$server</span>-&gt;setOption( APPNET_HTTP_404_PAGE , <span style="color: #800000;">"</span><span style="color: #800000;">404.html</span><span style="color: #800000;">"</span><span style="color: #000000;"> );
$server</span>-&gt;setOption( APPNET_HTTP_50X_PAGE , <span style="color: #800000;">"</span><span style="color: #800000;">50x.html</span><span style="color: #800000;">"</span><span style="color: #000000;"> );
$server</span>-&gt;setOption( APPNET_HTTP_KEEP_ALIVE , <span style="color: #800080;">1</span><span style="color: #000000;"> );
</span><span style="color: #008000;">//</span><span style="color: #008000;">注册网络事件回调函数</span>
$server-&gt;addEventListener( APPNET_EVENT_CONNECT , <span style="color: #800000;">"</span><span style="color: #800000;">onConnect</span><span style="color: #800000;">"</span><span style="color: #000000;">);
$server</span>-&gt;addEventListener( APPNET_EVENT_RECV , <span style="color: #800000;">"</span><span style="color: #800000;">onRecv</span><span style="color: #800000;">"</span><span style="color: #000000;">);
$server</span>-&gt;addEventListener( APPNET_EVENT_CLOSE , <span style="color: #800000;">"</span><span style="color: #800000;">onClose</span><span style="color: #800000;">"</span><span style="color: #000000;">);
$server</span>-&gt;addEventListener( APPNET_EVENT_START , <span style="color: #800000;">"</span><span style="color: #800000;">onStart</span><span style="color: #800000;">"</span><span style="color: #000000;">);
$server</span>-&gt;addEventListener( APPNET_EVENT_FINAL , <span style="color: #800000;">"</span><span style="color: #800000;">onFinal</span><span style="color: #800000;">"</span><span style="color: #000000;">);
$server</span>-&gt;addEventListener( APPNET_EVENT_TASK , <span style="color: #800000;">"</span><span style="color: #800000;">onTask</span><span style="color: #800000;">"</span><span style="color: #000000;">);
$server</span>-&gt;addEventListener( APPNET_EVENT_TASK_CB , <span style="color: #800000;">"</span><span style="color: #800000;">onTaskCallback</span><span style="color: #800000;">"</span><span style="color: #000000;"> );
$server</span>-&gt;<span style="color: #000000;">run();

</span>?&gt;</pre>
</div>
<p>&nbsp;</p>
<p>&nbsp;完整示例参见:<a href="https://github.com/lchb369/appnet_php7/blob/master/example/server.php" target="_blank">https://github.com/lchb369/appnet_php7/blob/master/example/server.php</a></p>
<p>&nbsp;更多介绍参见:<a href="https://github.com/lchb369/appnet_php7/wiki/appnet%E4%BB%8B%E7%BB%8D" target="_blank">https://github.com/lchb369/appnet_php7/wiki/appnet%E4%BB%8B%E7%BB%8D</a></p>
<p>&nbsp;</p>
<p><br />QQ交流群:379084776</p>