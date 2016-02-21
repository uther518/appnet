<p># appnet介绍<br /><br />appnet是一个基于linux epoll的多线程的高性能异步网络事件库，目标是用高性能的PHP版本搭载appnet取代C/lua或C/python模式,快速构建强有力的长连接服务器，以弥补PHP固有的缺陷。使其可广泛用于聊天系统，游戏服务器，消息通知服务器等实时通信场景。可对网络IO密集性场景或CPU密集性场景配置reactor数量和woker数量的比例，使硬件运行于最佳状态。<br /><br />其特点有</p>
<ul>
<li>高性能,核心用纯C语言开发,epoll异步非阻塞事件通知机制,单线程可支撑10万并发连接</li>
<li>易用性,使用方式简单,并提供PHP7.0版本扩展,简单几步就可塔建一个功能齐全的长连接服务器,不再需要nginx,apache,php-fpm等。</li>
<li>高并发,多线程异步网络IO,Per Thread One Loop并发模型，多个worker进程并行处理业务。</li>
<li>多协议,可混合TCP协议，websocket协议和简单http协议与服务器通信。</li>
<li>内存优化，进程间通信使用共享内存，兼容jemalloc和tcmalloc内存优化技术。</li>
<li>缓冲区优化，采用redis的动态缓冲区，根据数据包大小自动扩充，有效避免内存浪费和缓冲区溢出,其内存预分配策略降低了内存分配次数。从而提高内存分配效率。</li>


</ul>
<p><br />安装方法:<br />1,源码安装php_7.0.x<br />2,下载扩展到任意目录appnet_php7<br />3,执行如下指令:<br />&nbsp;&gt;cd appnet_php7<br />&nbsp;&gt;/usr/local/php7/bin/phpize<br />&nbsp;&gt;./configure --with-php-config=/usr/local/php7/bin/php-config<br />&nbsp;&gt;make<br />&nbsp;&gt;make install<br />&nbsp;&gt;php test/test.php<br />&nbsp;&gt;telnet 127.0.0.1 3011<br />&nbsp;开始测试吧。</p>
<div class="cnblogs_code">
<pre>&lt;?<span style="color: #000000;">php

</span><span style="color: #0000ff;">function</span> onConnect( <span style="color: #800080;">$server</span> , <span style="color: #800080;">$fd</span><span style="color: #000000;"> )
{
      </span><span style="color: #800080;">$pid</span> =<span style="color: #000000;"> posix_getpid();
      </span><span style="color: #0000ff;">echo</span> "Client Connect:{<span style="color: #800080;">$fd</span>} pid={<span style="color: #800080;">$pid</span>} \n"<span style="color: #000000;">; 
}

</span><span style="color: #0000ff;">function</span> onRecv( <span style="color: #800080;">$server</span> , <span style="color: #800080;">$fd</span> , <span style="color: #800080;">$buffer</span><span style="color: #000000;"> )
{
    </span><span style="color: #0000ff;">echo</span> "Client Recv:[{<span style="color: #800080;">$buffer</span>}][{<span style="color: #800080;">$fd</span>}] \n"<span style="color: #000000;">;
    </span><span style="color: #800080;">$header</span> = <span style="color: #800080;">$server</span>-&gt;<span style="color: #000000;">getHeader();
    </span><span style="color: #800080;">$server</span>-&gt;send( <span style="color: #800080;">$fd</span> , <span style="color: #800080;">$buffer</span><span style="color: #000000;"> );
    
};
</span><span style="color: #0000ff;">function</span> onClose( <span style="color: #800080;">$server</span> , <span style="color: #800080;">$fd</span><span style="color: #000000;"> )
{ 
    </span><span style="color: #0000ff;">echo</span> "Client Close:<span style="color: #800080;">$fd</span> \n"<span style="color: #000000;">;
};
</span><span style="color: #008000;">//</span><span style="color: #008000;">on worker run start,you can init env.</span>
<span style="color: #0000ff;">function</span> onStart( <span style="color: #800080;">$server</span><span style="color: #000000;">  )
{
    </span><span style="color: #800080;">$pid</span> =<span style="color: #000000;"> posix_getpid();
        </span><span style="color: #0000ff;">echo</span> "On Worker Start!! pid={<span style="color: #800080;">$pid</span>} \n"<span style="color: #000000;">;
    </span><span style="color: #008000;">//</span><span style="color: #008000;">3000ms means 3second    </span>
    <span style="color: #800080;">$server</span>-&gt;timerAdd( 3000 , "onTimerCallback" , "paramsxxx"<span style="color: #000000;"> );
};
</span><span style="color: #008000;">//</span><span style="color: #008000;">on worker shutdown,you must save data in last time.</span>
<span style="color: #0000ff;">function</span> onFinal( <span style="color: #800080;">$server</span><span style="color: #000000;">  )
{
    </span><span style="color: #800080;">$pid</span> =<span style="color: #000000;"> posix_getpid();
    </span><span style="color: #0000ff;">echo</span> "On Worker Final!! pid={<span style="color: #800080;">$pid</span>} \n"<span style="color: #000000;">;
};
</span><span style="color: #0000ff;">function</span> onTimerCallback( <span style="color: #800080;">$server</span> , <span style="color: #800080;">$timer_id</span> ,  <span style="color: #800080;">$params</span><span style="color: #000000;"> )
{
    </span><span style="color: #800080;">$pid</span> =<span style="color: #000000;"> posix_getpid();
    </span><span style="color: #0000ff;">echo</span> "onTimerCallback  ok,worker pid={<span style="color: #800080;">$pid</span>},timer_id={<span style="color: #800080;">$timer_id</span>}...\n"<span style="color: #000000;">;
    
    </span><span style="color: #008000;">//</span><span style="color: #008000;">if do not remove it, it will be call this function forever    </span>
    <span style="color: #800080;">$server</span>-&gt;timerRemove( <span style="color: #800080;">$timer_id</span><span style="color: #000000;"> );        
}<br />
</span><span style="color: #008080;">dl</span>( "appnet.so"<span style="color: #000000;">);
</span><span style="color: #800080;">$server</span> = <span style="color: #0000ff;">new</span> appnetServer( "0.0.0.0" , 3011<span style="color: #000000;"> );
</span><span style="color: #800080;">$server</span>-&gt;setOption( APPNET_OPT_WORKER_NUM , 1<span style="color: #000000;"> );
</span><span style="color: #800080;">$server</span>-&gt;setOption( APPNET_OPT_REACTOR_NUM, 1<span style="color: #000000;"> );
</span><span style="color: #800080;">$server</span>-&gt;setOption( APPNET_OPT_MAX_CONNECTION , 10000<span style="color: #000000;"> );
</span><span style="color: #800080;">$server</span>-&gt;setOption( APPNET_OPT_PROTO_TYPE ,<span style="color: #000000;"> APPNET_PROTO_MIX );
</span><span style="color: #800080;">$info</span> = <span style="color: #800080;">$server</span>-&gt;<span style="color: #000000;">getInfo();

</span><span style="color: #800080;">$server</span>-&gt;addEventListener( APPNET_EVENT_CONNECT , "onConnect"<span style="color: #000000;">);
</span><span style="color: #800080;">$server</span>-&gt;addEventListener( APPNET_EVENT_RECV ,    "onRecv"<span style="color: #000000;">);
</span><span style="color: #800080;">$server</span>-&gt;addEventListener( APPNET_EVENT_CLOSE ,   "onClose"<span style="color: #000000;">);
</span><span style="color: #800080;">$server</span>-&gt;addEventListener( APPNET_EVENT_START ,   "onStart"<span style="color: #000000;">);
</span><span style="color: #800080;">$server</span>-&gt;addEventListener( APPNET_EVENT_FINAL ,   "onFinal"<span style="color: #000000;">);

</span><span style="color: #800080;">$server</span>-&gt;<span style="color: #000000;">run();


</span>?&gt;</pre>
</div>
<p>&nbsp;</p>
<p>&nbsp;更多介绍参见:<a href="https://github.com/lchb369/appnet_php7/wiki/APPNET%E6%96%87%E6%A1%A3%E7%9B%AE%E5%BD%95" target="_blank">https://github.com/lchb369/appnet_php7/wiki/appnet%E4%BB%8B%E7%BB%8D</a></p>
<p>&nbsp;</p>
<p><br />QQ交流群:379084776</p>
