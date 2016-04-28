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
<pre>&lt;?<span style="color: #000000;">php</span></pre>
<p>&nbsp;</p>
<p>function onTaskCallback( $server , $data , $taskid , $from )<br />{<br />	 　　$pid = posix_getpid();<br />	 　　echo "onTaskCallback data=[{$data}],taskid=[{$taskid}],from=[{$from}],pid={$pid} \n";<br />}</p>
<pre></pre>
<p>function onConnect( $server , $fd )<br />{<br />	　　$pid = posix_getpid();<br />        　　echo "\n\nClient Connect:{$fd} pid={$pid} \n"; <br />}</p>
<pre></pre>
<p>//在worker进程中，表示回调，在task进程中表示请求<br />function onTask( $server, $data , $taskid, $from )<br />{<br />	　　$pid = posix_getpid();<br />	　　echo "onTask data=[{$data}],taskid=[{$taskid}],from=[{$from}],pid={$pid} \n";	<br />	　　//sleep( 3 );<br />	　　$ret = array(<br />	   　　　　'status' =&gt; 'ok',<br />	   　　　　'data' =&gt; array( 'a' =&gt; "aaaa" ),<br />	　　);<br />	　　$server-&gt;taskCallback( json_encode( $ret ) , $taskid , $from  );<br />}</p>
<pre></pre>
<p>function onRecv( $server , $fd , $buffer )<br />{<br />	　　$header = $server-&gt;getHeader();<br />	 　　$pid = posix_getpid();<br />	　　echo "Client Recv:[{$header['Protocol']}][{$header['Uri']}][{$buffer}][{$fd}],pid={$pid} \n";	</p>
<pre></pre>
<p> 　　if( $header['Protocol'] == "WEBSOCKET" )<br />	　　{<br />	  	 　　　　//Websocket::onReceive( $server , $fd,  $buffer );<br />	　　}<br />	　　elseif(  $header['Protocol'] == "HTTP"  )<br />    	　　{<br />		　　　　$data  = $buffer;<br />		　　　　$server-&gt;addAsynTask( $data , 1 );<br />		<br />		　　　　$server-&gt;setHeader( "Connection" , "keep-alive" );<br />		　　　　$server-&gt;send( $fd , $data );	<br />	　　}<br />	　　else<br />	　　{<br />        	　　　　$server-&gt;send( $fd , $buffer );<br />	　　}<br />};</p>
<pre></pre>
<p>function onClose( $server , $fd )<br />{ <br />	　　echo "Client Close:$fd \n";<br />	　　$header = $server-&gt;getHeader();<br />	　　if( $header['Protocol'] == "WEBSOCKET" )<br />        　　{<br />                 　　　　//Websocket::broadcastOffLine( $server , $fd  );<br />	　　}<br />};</p>
<pre></pre>
<p>function onStart( $server )<br />{<br />	　　$pid = posix_getpid();<br />        　　echo "On Worker Start!! pid={$pid} \n";<br />	　　//3000ms means 3second	<br />	　　$server-&gt;timerAdd( 3000 , "onTimerCallback" , "paramsxxx" );<br />};</p>
<pre></pre>
<p>//on worker shutdown,you must save data in last time.<br />function onFinal( $server  )<br />{<br />	　　$pid = posix_getpid();<br />        　　echo "On Worker Final!! pid={$pid} \n";<br />};</p>
<pre></pre>
<p>function onTimerCallback( $server , $timer_id , $params )<br />{<br />	　　$pid = posix_getpid();<br />	　　echo "onTimerCallback  ok,worker pid={$pid},timer_id={$timer_id}...\n";<br />	　　//if do not remove it, it will be call this function forever	<br />	　　$server-&gt;timerRemove( $timer_id );		<br />}</p>
<p>dl( "appnet.so");<br />$server = new appnetServer( "0.0.0.0" , 3011 );<br />$server-&gt;setOption( APPNET_OPT_WORKER_NUM , 2 );<br />$server-&gt;setOption( APPNET_OPT_ATASK_WORKER_NUM , 2 );<br />$server-&gt;setOption( APPNET_OPT_REACTOR_NUM, 2 );<br />$server-&gt;setOption( APPNET_OPT_MAX_CONNECTION , 10000 );<br />$server-&gt;setOption( APPNET_OPT_PROTO_TYPE , APPNET_PROTO_MIX );<br />$server-&gt;setOption( APPNET_HTTP_DOCS_ROOT , $_SERVER['PWD']."/example/www/" );<br />$server-&gt;setOption( APPNET_HTTP_UPLOAD_DIR, "/home/upload/"  );<br />$server-&gt;setOption( APPNET_HTTP_404_PAGE , "404.html" );<br />$server-&gt;setOption( APPNET_HTTP_50X_PAGE , "50x.html" );<br />$server-&gt;setOption( APPNET_HTTP_KEEP_ALIVE , 1 );<br />//注册网络事件回调函数<br />$server-&gt;addEventListener( APPNET_EVENT_CONNECT , "onConnect");<br />$server-&gt;addEventListener( APPNET_EVENT_RECV ,    "onRecv");<br />$server-&gt;addEventListener( APPNET_EVENT_CLOSE ,   "onClose");<br />$server-&gt;addEventListener( APPNET_EVENT_START ,   "onStart");<br />$server-&gt;addEventListener( APPNET_EVENT_FINAL ,   "onFinal");<br />$server-&gt;addEventListener( APPNET_EVENT_TASK ,    "onTask");<br />$server-&gt;addEventListener( APPNET_EVENT_TASK_CB , "onTaskCallback" );<br />$server-&gt;run();</p>
<p><span style="line-height: 1.5;">?&gt;</span></p>
</div>
<p>&nbsp;完整示例参见:<a href="https://github.com/lchb369/appnet_php7/blob/master/example/server.php" target="_blank">https://github.com/lchb369/appnet_php7/blob/master/example/server.php</a></p>
<p>&nbsp;更多介绍参见:<a href="https://github.com/lchb369/appnet_php7/wiki/appnet%E4%BB%8B%E7%BB%8D" target="_blank">https://github.com/lchb369/appnet_php7/wiki/appnet%E4%BB%8B%E7%BB%8D</a></p>
<p>&nbsp;</p>
<p><br />QQ交流群:379084776</p>