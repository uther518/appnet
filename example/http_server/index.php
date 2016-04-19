<?php

$startTime = microtime( true );
define( 'DEBUG' , true );

define( "ROOT_DIR" , dirname( dirname(  __FILE__ ) ) );
define( "MOD_DIR" , ROOT_DIR ."/Model" );
define( "LIB_DIR" , ROOT_DIR ."/Lib" );
define( "CON_DIR" , ROOT_DIR ."/Controller/Api" );
define( "CONFIG_DIR" , ROOT_DIR . "/Config" );

//调试模式
if( DEBUG || isset( $_GET["debug"] )  )
{
	error_reporting( E_ALL ^ E_NOTICE );
	ini_set( 'display_errors' , 'On' );
}
else
{
	error_reporting( 0 );
	ini_set( 'display_errors' , 'Off' );
}


$method = explode( '.' , $_REQUEST['method']  );
$controller = ucfirst( strtolower( $method[0] ) ) . 'Controller';
$action = $method[1];
$uid = $_REQUEST['uid'] ? $_REQUEST['uid'] : 0;

include LIB_DIR."/Common.php";
//性能分析器
$config = Common::getConfig();
if( $config['xhprof']['isOpen'] && function_exists( 'xhprof_enable' )  )
{
	xhprof_enable(XHPROF_FLAGS_CPU + XHPROF_FLAGS_MEMORY);
	include_once LIB_DIR .'/Xhprof/xhprof_lib.php';
	include_once LIB_DIR .'/Xhprof/xhprof_runs.php';
}

if( file_exists( CON_DIR . "/{$controller}.php" ) )
{
	$conObject = new $controller;
	if( method_exists( $conObject , $action ) )
	{
		try
		{
			$conObject->setUser( $uid );
			$data = $conObject->$action();
			$code = 0;
			$msg = '';
		
			if( isset( $data['errorCode'] ) && $data['errorCode'] > 0 )
			{
				$code = $data['errorCode'];
				$data = new stdClass();
				$msg = '';
			}
			else
			{
				DataObjects::save();
			}
		}
		catch ( Exception $e )
		{
			$data = new stdClass();
			$code = $e->getCode();
			$msg = $e->getMessage();
		
			$errorLog  = new RunLog( "traceException" , $uId  );
			$errorContent = "\nParam:".json_encode( array_merge( $_GET , $_POST ) )."\nErrorLog:".$e;
			$errorLog->addLog(  $errorContent );
		}
	}
	
	$result = array
	(
		'code' => (int)$code ,
		'method' => (string)$_REQUEST['method'] ,
		'msg' => (string)$msg ,
		'data' => $data,
	);
	$result['ts'] = $_SERVER['REQUEST_TIME'];
	
	$endTime = microtime( true );
	if( DEBUG && isset( $_GET["debug"] ) )
	{
		echo "<pre>";print_r( $result );printf( "\n%dus.\n" , ( $endTime - $startTime ) * 1000000 );
		echo "</pre>";
	}
	echo json_encode( $result );
	
	if( $config['xhprof']['isOpen'] && function_exists( 'xhprof_disable' ) )
	{
		$xhprof_data = xhprof_disable();
		$xhprof_runs = new XHProfRuns_Default();
		$method = str_replace( "." , "::" , $_GET['method'] );
		$xhprof_runs->save_run( $xhprof_data , $method  );
	}
	return;
}
else 
{
	echo json_encode( array( 'code' => 3 , 'msg' => 'method not exist' ) );exit;
}

?>