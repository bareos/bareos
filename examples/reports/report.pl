#!/usr/bin/perl
#
# A bacula job report generator.
# It require MySQL 4.1.x or later
#
# If you have any comments question feel free to contact me, jb@soe.se
#
# /Jonas Björklund
#

use DBI;

$db_host        = "localhost";
$database       = "bacula";
$db_username    = "bacula";
$db_password    = "bacula";
$email 		= "$ARGV[0]";
$from		= "backup\@example.net";
$when 		= "$ARGV[1]";

if (!@ARGV) {
	print "\n report.pl email@hostname.com (TODAY|YESTERDAY|WEEK|MONTH)\n\n";
	exit;
}


if ($when eq "MONTH") {
	$where		= "StartTime > DATE_FORMAT(now() - INTERVAL 1 MONTH, '%Y-%m-%d')";
	$order 		= "ORDER BY StartTime DESC";
} elsif ($when eq "WEEK") {
	$where		= "StartTime > DATE_FORMAT(now() - INTERVAL 7 DAY, '%Y-%m-%d')";
	$order 		= "ORDER BY StartTime DESC";
} elsif ($when eq "YESTERDAY") {
	$where		= "StartTime > DATE_FORMAT(now() - INTERVAL 1 DAY, '%Y-%m-%d') AND StartTime < DATE_FORMAT(now(), '%Y-%m-%d')";
	$order 		= "ORDER BY JobStatus,Time DESC";
} else {
	$when = "TODAY";
	$where		= "StartTime > curdate()";
	$order 		= "ORDER BY JobStatus,Time DESC";
}

$sqlquery	= "SELECT JobStatus,Name,Level,JobBytes,JobFiles,DATE_FORMAT(StartTime, '%Y-%m-%d %H:%i') AS Start, TIMEDIFF(EndTime,StartTime) AS Time,PoolId
	FROM Job WHERE 
	$where
	$order";

$dbh = DBI->connect("DBI:mysql:database=$database:host=$db_host", $db_username,$db_password) or die;
        
my $sth = $dbh->prepare("$sqlquery"); $sth->execute() or die "Can't execute SQL statement : $dbh->errstr";
while(($jobstatus,$name,$level,$jobbytes,$jobfiles,$start,$time,$poolid) = $sth->fetchrow_array()) {
	my $sth2 = $dbh->prepare("SELECT Name FROM Pool WHERE PoolId = $poolid"); $sth2->execute() or die "Can't execute SQL statement : $dbh->errstr";
	($poolname) = $sth2->fetchrow_array();
	($hours,$minutes,$seconds) = split(":", $time);
	$seconds = sprintf("%.1f", $seconds + ($minutes * 60) + ($hours * 60 * 60));
	$time = sprintf("%.1f", ($seconds + ($minutes * 60) + ($hours * 60 * 60)) / 60);
	$bytesANDfiles = sprintf "%7.0f/%d", $jobbytes/1024/1024,$jobfiles;
	$kbs = 0;
	if ($jobbytes != 0) {
		$kbs = ($jobbytes/$seconds)/1024;
	}
	
	$text .= sprintf "%s %18.18s %1s %14s %16s %5sm %4.0f %9.9s\n", $jobstatus,$name,$level,$bytesANDfiles,$start,$time,$kbs,$poolname;
	$totalfiles = $totalfiles + $jobfiles;
	$totalbytes = $totalbytes + $jobbytes;
}
$totalbytes = sprintf("%.1f",$totalbytes / 1024 / 1024 / 1024);

my $sth = $dbh->prepare("SELECT count(*) FROM Job WHERE $where"); $sth->execute() or die "Can't execute SQL statement : $dbh->errstr";
($count_total) = $sth->fetchrow_array();
my $sth = $dbh->prepare("SELECT count(*) FROM Job WHERE $where AND JobStatus = 'T'"); $sth->execute() or die "Can't execute SQL statement : $dbh->errstr";
($count_ok) = $sth->fetchrow_array();
$count_fail = $count_total - $count_ok;
$counts = sprintf("%.1f", 100- (($count_fail/$count_total)*100)); 

       
open(MAIL,"|/usr/lib/sendmail -f$from -t");
print MAIL "From: $from\n";
print MAIL "To: $email\n";
print MAIL "Subject: Backup ($when) $counts% OK - Total $count_total jobs, $count_fail failed\n";
print MAIL "\n";
print MAIL "Total $count_total jobs - $count_ok jobs are OK.\n";
print MAIL "Total $totalbytes GB / $totalfiles files\n";
print MAIL "\n";

print MAIL "Status       JobName Lvl MBytes/Files            Start   Time KB/s      Pool\n";
print MAIL "============================================================================\n";
print MAIL $text;

print MAIL "============================================================================\n";
print MAIL <<EOF;


Status codes:

  T 	Terminated normally
  E 	Terminated in Error
  A 	Canceled by the user
  C 	Created but not yet running
  R 	Running
  B 	Blocked
  e 	Non-fatal error
  f 	Fatal error
  D 	Verify Differences
  F 	Waiting on the File daemon
  S 	Waiting on the Storage daemon
  m 	Waiting for a new Volume to be mounted
  M 	Waiting for a Mount
  s 	Waiting for Storage resource
  j 	Waiting for Job resource
  c 	Waiting for Client resource
  d 	Wating for Maximum jobs
  t 	Waiting for Start Time
  p 	Waiting for higher priority job to finish
	
EOF
close(MAIL);
