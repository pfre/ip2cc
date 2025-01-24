<HTML>
<body>

<?php

#exec time begin.
$timeparts = explode(" ",microtime());
$starttime = $timeparts[1].substr($timeparts[0],1);

function findcountry($ip){
	exec( "ip2cc.exe ".$ip, $doms );
}

findcountry($_SERVER['REMOTE_ADDR']);
#findcountry("150.101.177.134");

#exec time end.
$timeparts = explode(" ",microtime());
$endtime = $timeparts[1].substr($timeparts[0],1);
echo "<br>exec time: ".round($endtime - $starttime, 5);
?>

</body>
</HTML>
