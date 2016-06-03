
var ws = {};
var client_id = 0;
var session_id="";
var private_chat = 1;
var current_friend_fd = 0;
var userlist = {};
var GET = new Object();
GET = getRequest();

var ip = document.domain; 
var port = location.port;

ip = "192.168.68.131";
port = "3011";

function getRequest()
{
	var url = location.search; // 获取url中"?"符后的字串
	var theRequest = new Object();
	if (url.indexOf("?") != -1) {
		var str = url.substr(1);
	
		strs = str.split("&");
		for ( var i = 0; i < strs.length; i++) {
			var decodeParam = decodeURIComponent( strs[i] );
			var param = decodeParam.split( "=" );
			theRequest[param[0]] = param[1];
		}
		
	}
	return theRequest;
}


/**
 * 显示所有在线列表
 * @param data
 */
function showOnlineList( data )
{
	var dataObj = $.evalJSON( data );
	var li="";
	for ( var i = 0; i < dataObj.list.length; i++ ) 
	{	
		if( client_id == dataObj.list[i].fd )
		{
			dataObj.list[i].name = "自己";
		}

		li = li + "<li id='friend_"+dataObj.list[i].fd+"'>";
        	li = li + "<div class='qq-hui-img'><img src='images/head/01.jpg'><i></i></div>";
        	li = li + "<div class='qq-hui-name'><span>"+dataObj.list[i].name+"</span><i>16:30</i></div>";
        	li = li + "<div class='qq-hui-txt' title=''>下次我们去公园拍摄吧~[图片]</div>";
		li = li + "</li>";
		userlist[dataObj.list[i].fd] = dataObj.list[i].name; 
		
	}
	$("#qq_list").html( li );
}

/**
 * 当有一个新用户连接上来时
 * @param userid
 */
function showNewUser( data ) 
{
	var dataObj = $.evalJSON( data );
	if( !userlist[dataObj.fd] )
	{
		userlist[dataObj.fd] = dataObj.name;
		if ( dataObj.fd != client_id )
		{
			li=  "<li id='friend_"+dataObj.fd+"'>";
			li = li + "<div class='qq-hui-img'><img src='images/head/01.jpg'><i></i></div>";
			li = li + "<div class='qq-hui-name'><span>"+dataObj.name+"</span><i>16:30</i></div>";
			li = li + "<div class='qq-hui-txt' title=''>下次我们去公园拍摄吧~[图片]</div>";
			li = li + "</li>";
			$("#qq_list").append( li );
		}
	}
}


//收到别人发来的消息
function showNewMsg( data )
{
	var dataObj = $.evalJSON( data );
	var content = xssFilter( dataObj.data )
	var fromId = dataObj.from;
	var channal = dataObj.channal;
	content = parseXss( content );
	
	//表示系统消息
	if (fromId == 0)
	{
		alert( "系统消息暂未处理"+dataObj.data );
	}
	else
	{
		var html = '';
		var to =  dataObj.to;
		
		var name = userlist[fromId];
		var now=new Date()
		var t_div = now.getFullYear()+"-"+(now.getMonth()+1)+"-"+now.getDate()+' '+now.getHours()+":"+now.getMinutes()+":"+now.getSeconds();
		
		//如果说话的是我自己
		if( client_id == fromId )
		{
			//就是展示给自己看自己发的内容
			//var name = $('.qq-top-name span').html()
			
			$('.qq-chat-txt ul').append('<li class="my"><div class="qq-chat-my"><span>'+name+'说</span><i>'+t_div+'</i></div><div class="qq-chat-ner">'+content+'</div></li>')
			$(".qq-chat-txt").scrollTop($(".qq-chat-txt")[0].scrollHeight);
			$('#qq-chat-text').val('').trigger("focus")
		} 
		else
		{
			
			$("#friend_"+fromId+" .qq-hui-txt" ).html( content );
			//大窗口只显示当前好友发来的消息
			if( current_friend_fd == fromId )
			{
				$('.qq-chat-txt ul').append('<li class="my"><div class="qq-chat-my"><span>'+name+'说</span><i>'+t_div+'</i></div><div class="qq-chat-ner">'+content+'</div></li>')
				$(".qq-chat-txt").scrollTop($(".qq-chat-txt")[0].scrollHeight);
				$('#qq-chat-text').val('').trigger("focus")
			}
			
		}
		
	}
}


function delUser( userid )
{
	$('#friend_' + userid).remove();
	delete (userlist[userid]);
}

function websocket_open( qq )
{
	if ( window.WebSocket || window.MozWebSocket) 
	{
		if( client_id == 0  )
		{
			ws = new WebSocket( "ws://"+ip+":"+port );
		}
		else
		{
			alert( "您已经登录了" );
		}
		/**
		 * 连接建立时触发
		 */
		ws.onopen = function(e)
		{
			msg = new Object();
			msg.cmd = 'login';
			msg.name = qq;
			msg.avatar = "";
			if( $.cookie('webqq_session_id' ))
			{
				msg.session_id = $.cookie('webqq_session_id' );
			}
			else
			{
				msg.session_id = new Date().getTime();
			}
			$.cookie('webqq_session_id', msg.session_id  );
			$("#qq-top-name").val( qq );
			ws.send( $.toJSON( msg ));	
		};
		
			
		//有消息到来时触发
		ws.onmessage = function( e )
		{
			var cmd = $.evalJSON( e.data ).cmd;
			if( cmd == 'login' )
			{
				client_id = $.evalJSON( e.data ).fd;			
				//获取在线列表
				msg = new Object();
				msg.cmd = 'getOnline';
				ws.send( $.toJSON( msg ) );
			}
			else if( cmd == 'getOnline' )
			{
				showOnlineList( e.data );
			}
			else if( cmd == 'newUser' )
			{
				showNewUser( e.data );
			}
			else if( cmd == 'fromMsg' )
			{
				showNewMsg( e.data );
			}
			else if( cmd == 'offline' )
			{
				//alert( "offline");
				var cid = $.evalJSON( e.data ).fd;
				delUser( cid ); 
				//showNewMsg( e.data  );	
			}
		};
			
		/**
		 * 连接关闭事件
		 */
		ws.onclose = function(e) 
		{
			if( confirm( "您已退出聊天室" ))
			{
				client_id = 0;
			}
		};
		
		/**
		 * 异常事件
		 */
		ws.onerror = function(e)
		{
			//alert( "异常:"+e.data );
			console.log("onerror");
		};
			
		
	}
	else
	{
		alert("您的浏览器不支持WebSocket，请使用Chrome/FireFox/Safari/IE10浏览器");
	}
	
}


$(document).ready(function(){

  $('.qq-xuan li').click(function(){
    $(this).addClass('qq-xuan-li').siblings().removeClass('qq-xuan-li')
  })
  $('.qq-hui-txt').hover(function(){
    var aa = $(this).html()
    $('.qq-hui-txt').attr('title',aa)
  })
  $('.login-close').click(function(){
     $(this).parent().parent().css('display','none')
  })
  
  
  //好友列表，双击事件，打开聊天窗口
  $("#qq_list").on("dblclick","li", function() {
	  
	var friend_id = $(this).attr("id");
	var fri = friend_id.split("_");

	current_friend_fd = fri[1];
	
        $('.qq-chat').css('display','block').removeClass('mins')
	$('.qq-chat-t-name').html($(this).find('span').html())
	$('.qq-chat-t-head img').attr('src',$(this).find('img').attr('src'))
	$('.qq-chat-you span').html($(this).find('span').html())
	$('.qq-chat-you i').html($(this).find('.qq-hui-name i').html())
	$('.qq-chat-ner').html($(this).find('.qq-hui-txt').html())
	$("#qq-chat-text").trigger("focus")
	$('.my').remove()
  });
  
  //QQ icon打开事件
  $('.qq-exe img').dblclick(function(){
    $('.qq-login').css('display','block').removeClass('mins')
  })
 
  
  $('.close').click(function(){
    $(this).parent().parent().parent().css('display','none')
  })
  
   $('#logout').click(function(){
	disConnect();
    $(this).parent().parent().parent().css('display','none')
  })
  
  
  $('.min').click(function(){
    $(this).parent().parent().parent().addClass('mins')
  })
  
  $('.qq .close').click(function(){
    $('.qq-chat').css('display','none')
  })
  $('#qq-chat-text').keydown(function(e){
    if(e.keyCode == 27){
	  $('.qq-chat').css('display','none')
    }
  })
  
  //发送消息,私聊
  $('.fasong').click(function(){
	if($('#qq-chat-text').val()==''){
	  alert("发送内容不能为空,请输入内容")
	}else if($('#qq-chat-text').val()!=''){
		sendMsg();
	}
  })
  
 
  $('.close-chat').click(function(){
    $('.qq-chat').css('display','none')
  })
  
  $(".qq-hui").niceScroll({
    touchbehavior:false,cursorcolor:"#ccc",cursoropacitymax:1,cursorwidth:6,horizrailenabled:true,cursorborderradius:3,autohidemode:true,background:'none',cursorborder:'none'
  });
  
  
    //点击登录
  $('.login-but').click(function(){
	if($('.login-txt').find('input').val() == ''){
	  alert('请输入账号或者密码')
	}else if($('login-txt input').val() != ''){
	
	  var qq_code = $('#qq_code').val();
	  websocket_open( qq_code );
		
      $('.qq').css('display','block').removeClass('mins')
	  $('.qq-login').css('display','none')
	}
  })
  
  //键盘回车登录
  $('.login-txt input').keydown(function(e){
    if(e.keyCode == 13){
      if($('.login-txt').find('input').val() == ''){
	  alert('请输入账号或者密码')
	}else if($('login-txt input').val() != ''){
		
	  var qq_code = $('#qq_code').val();
	  websocket_open( qq_code );
		
      $('.qq').css('display','block').removeClass('mins')
	  $('.qq-login').css('display','none')
	}
    }
  })
  
  
});

function disConnect()
{
	var msg = new Object();
	msg.cmd = "disconnect";
	msg.from = client_id;
	msg.channal = 1;

	//ws.close();
	ws.send( $.toJSON( msg ) );
}

function sendMsg()
{
	var content = $('#qq-chat-text').val();
	content = content.replace(" ", "&nbsp;");
	if( !content )
	{
		return false;
	}
	
	//群发
	if( private_chat == 0 )
	{
		var msg = new Object();
		msg.cmd = 'message';
		msg.from = client_id;
		msg.channal = 0;
		msg.data = content;
		ws.send( $.toJSON( msg ) );
	}
	//私聊
	else
	{
		var msg = new Object();
		msg.cmd = 'message';
		msg.from = client_id;
		msg.to = current_friend_fd;
		msg.channal = 1;
		msg.data = content;
		ws.send( $.toJSON( msg ) );
	}
	//$('#msg_content').val( "" );
	return false;

}

function xssFilter(val) 
{
	val = val.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/\x22/g, '&quot;').replace(/\x27/g, '&#39;');
	return val;
}

function parseXss( val )
{
	val = val.replace( /#(\d*)/g,  '<img src="resource/img/face/$1.gif" />'  );
	val = val.replace( '&amp;' , '&' );
	return val;
}


function GetDateT() {
	var d;
	d = new Date();
	var h,i,s;
	
	h = d.getHours();
	i = d.getMinutes();
	s = d.getSeconds();
	
	h = ( h < 10 ) ? '0'+h : h;
	i = ( i < 10 ) ? '0'+i : i;
	s = ( s < 10 ) ? '0'+s : s;
	return h+":"+i+":"+s;
}




(function($){
	$.fn.extend({
		insertAtCaret:function( myValue )
		{
			var $t=$(this)[0];
			if( document.selection ) 
			{
				this.focus();
				sel = document.selection.createRange();
				sel.text = myValue;
				this.focus();
			}
			else if( $t.selectionStart || $t.selectionStart == '0')
			{
			
				var startPos = $t.selectionStart;
				var endPos = $t.selectionEnd;
				var scrollTop = $t.scrollTop;
				$t.value = $t.value.substring(0, startPos) + myValue + $t.value.substring(endPos, $t.value.length);
				this.focus();
				$t.selectionStart = startPos + myValue.length;
				$t.selectionEnd = startPos + myValue.length;
				$t.scrollTop = scrollTop;
			}
			else 
			{
				
				this.value += myValue;
				this.focus();
			}
		}
	}) 
})(jQuery);

