<HTML>
<body>

<?php

#exec time begin.
$timeparts = explode(" ",microtime());
$starttime = $timeparts[1].substr($timeparts[0],1);

function findcountry($ip){
    $countrydb = file("ip-to-country.txt");
    $ip_number = sprintf("%u",ip2long($ip));

    #binary search attempt
    $low = 0;
    $high = count($countrydb) -1;

    $count = 0;
    while($low <= $high) {
        $count++;
        $mid = floor(($low + $high) / 2);  // C floors for you
        $num1 = substr($countrydb[$mid], 1, 10);
        $num2 = substr($countrydb[$mid], 14, 10);
        if($num1 <= $ip_number && $ip_number <= $num2){
            #start at 27 go 2
            #substr($$countrydb[$mid], 27, 2);
            print "Found your country: " . substr($countrydb[$mid], 27, 2);
            break;
        } else {
            if ($ip_number < $num1) {
                $high = $mid - 1;
            } else {
                $low = $mid + 1;
            }
        }
    }
    print "<br>\nlines checked:$count total lines in file:"
    .count($countrydb)
    ." filesize: ".filesize("ip-to-country.txt")." kb";
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
