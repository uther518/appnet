<?php


for( $i=0;$i<2;$i++)
{

$fp = fsockopen("127.0.0.1", 3011 , $errno, $errstr, 30);

echo $errstr;   
}

sleep( 30 );
?>
