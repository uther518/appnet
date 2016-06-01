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
<p><br />安装方法:<br />1,源码安装php_7.0.x<br />2,下载扩展到任意目录appnet_php7<br />3,执行如下指令:<br />&nbsp;&gt;cd appnet_php7<br />&nbsp;&gt;/usr/local/php7/bin/phpize<br />&nbsp;&gt;./configure --with-php-config=/usr/local/php7/bin/php-config<br />&nbsp;&gt;make<br />&nbsp;&gt;make install</p>
<p><strong>启动服务器</strong></p>
<p>&nbsp;&gt;php example/server.php&nbsp;</p>
<p><strong>TCP测试:</strong></p>
<p>&nbsp;&gt;telnet 127.0.0.1 3011</p>
<p><strong>Http/Websocket测试:</strong></p>
<p>访问:http://192.168.68.131:3011/index.html</p>
<p>将其中的IP换为你自己的服务器IP</p>
<p>&nbsp;</p>
<p>完整示例参见:<a href="https://github.com/lchb369/appnet_php7/blob/master/example/server.php" target="_blank">https://github.com/lchb369/appnet_php7/blob/master/example/server.php</a></p>
<p>&nbsp;更多介绍参见:<a href="https://github.com/lchb369/appnet_php7/wiki/appnet%E4%BB%8B%E7%BB%8D" target="_blank">https://github.com/lchb369/appnet_php7/wiki/appnet%E4%BB%8B%E7%BB%8D</a></p>
<p>&nbsp;QQ交流群:379084776</p>