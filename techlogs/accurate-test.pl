#!/usr/bin/perl -w
use strict;

# this test needs 25GB of free space for the database
# you need pwgen package to fill path and filename tables

# start with:
# filename=1 ./fill_table.pl &
# path=1 ./fill_table.pl &
# file=1 ./fill_table.pl &

# You can add more files with 
# start=400 end=600 file=1 ./fill_table.pl

# then test with
# ./fill_table.pl

# you can run optimize with
# filename=1 optimize=1 ./fill_table.pl

# you can get mysql profiling info with
# profile=1 ./fill_table.pl

# to get values
# ./fill_table.pl > /dev/null

# to use something else than mysql on regress
# dbi="DBI:Pg:database=regress" ./fill_table.pl

use DBI;
my $dbi = $ENV{dbi} || "DBI:mysql:database=regress";
my $table = ($dbi =~ /mysql/i)?'TABLE':'';
my $dbh = DBI->connect($dbi, "", "");
die "Can't connect to your database:\n$DBI::errstr\n"
    unless ($dbh);

$dbh->{FetchHashKeyName} = 'NAME_lc';

if ($ENV{filename}) {
    my $t = time;
    my $j=0;
    $dbh->begin_work();
    print "Fill Filename table\n";
    open(FILE, "pwgen -1 11 12000000|") or die "Please, install pwgen";
    my $sth = $dbh->prepare("INSERT INTO Filename (Name) VALUES (?),(?),(?),(?),(?),(?),(?),(?),(?),(?)");
    while (!eof(FILE))
    {
        my @val;
        for my $i (1..10) {
            my $l = <FILE>;
            chop($l);
            push @val, "$l$j";
            $j++;
        }
        if (scalar(@val) == 10) {
            $sth->execute(@val) or die;
        }
    }
    close(FILE);
    $dbh->commit();
    print "Inserting to Filename ", 12000000/(time - $t), "f/sec\n";
    $dbh->do("OPTIMIZE TABLE Filename") if $ENV{optimize};
    $dbh->do("ANALYZE $table Filename");
}
if ($ENV{path}) {
    my $j=0;
    my $t = time;
    $dbh->begin_work();
    print "Fill Path table\n";
    open(DIR, "pwgen -1 8 4000000|") or die "Please, install pwgen";
    my $sth = $dbh->prepare("INSERT INTO Path (Path) VALUES (?),(?),(?),(?),(?),(?),(?),(?),(?),(?)");
    while (!eof(DIR))
    {
        my @val;
        for my $i (1..10) {
            my $l = <DIR>;
            chop($l);
            push @val, "$l$j";
            $j++;
        }
        if (scalar(@val) == 10) {
            $sth->execute(@val) or die "@val DBI::errstr";
        }
     }
    close(DIR);
    $dbh->commit();
    print "Inserting to Path ", 4000000/(time - $t), "p/sec\n";
    $dbh->do("OPTIMIZE TABLE Path") if $ENV{optimize};
    $dbh->do("ANALYZE $table Path");
}

if ($ENV{file}) {
    # jobs 10, 20, 30, 40..400 are full of 2M files
    # jobs 1..5 are incr of 200,000 files
    # jobs 6..9 are diff of 500,000 files
    my $jobid=$ENV{start} || 1;
    my $end = $ENV{end} || 400;
    my $type;
    my $nb;
    while ($jobid < $end) {
        if (($jobid % 10) == 0) {
            $type = 'F';
            $nb = 200000;
        } elsif (($jobid % 10) < 6) {
            $type = 'I';
            $nb = 20000;
        } else {
            $type = 'F';
            $nb = 50000;
        }

        $dbh->begin_work();
        my $t = time;
        $dbh->do("
INSERT INTO Job (Job, Name, JobFiles, Type, Level, JobStatus, JobTDate)
     VALUES ('AJob$jobid', 'Ajob$jobid', $nb*10, 'B', '$type', 'T', '$t')");
        my $sth = $dbh->prepare("INSERT INTO File (JobId, FileIndex, PathId, FilenameId,LStat, MD5) VALUES ($jobid,1,?,?,'lstat', ''),($jobid,1,?,?,'lstat', ''),($jobid,1,?,?,'lstat', ''),($jobid,1,?,?,'lstat', ''),($jobid,1,?,?,'lstat', ''),($jobid,1,?,?,'lstat', ''),($jobid,1,?,?,'lstat', ''), ($jobid,1,?,?,'lstat', ''), ($jobid,1,?,?,'lstat', ''), ($jobid,1,?,?,'lstat','')");
        my $fid = $jobid*10;
        my $pid = $jobid*10;
        for my $nb (1..$nb) {
            $fid = ($fid + 1111) % 12000000;
            $pid = ($pid + 11) % 3000000;
            $sth->execute($pid, $fid,
                          $pid, $fid+1,
                          $pid, $fid+2,
                          $pid, $fid+3,
                          $pid, $fid+4,
                          $pid, $fid+5,
                          $pid, $fid+6,
                          $pid, $fid+7,
                          $pid, $fid+8,
                          $pid, $fid+9
                ) or die DBI::errstr;
        }
        print "JobId=$jobid files=", $nb*10, "\n";
        $dbh->commit();
        print "Insert takes ", scalar(time) - $t, 
              "secs for ", $nb*10, " records\n";
        $jobid++;
    }

    $dbh->do("OPTIMIZE TABLE File,Job") if $ENV{optimize};
    $dbh->do("ANALYZE $table Job");
    $dbh->do("ANALYZE $table File");
}


if (!($ENV{file} or $ENV{path} or $ENV{filename}) or $ENV{testsql})
{
    do_bench("13,23", 400000);
    do_bench("17,37", 1000000);
    do_bench("10,13", 2200000);
    do_bench("20,27", 2500000);
    do_bench("50,60", 4000000);
    do_bench("70,80,82,92,103", 4600000);
    do_bench("30,40,17,23,34", 4900000);
    do_bench("110,120,130", 6000000);
    print_mysql_profiles() if $ENV{profile};
}

sub do_bench
{
    my ($jobid, $nb) = @_;

    if ($dbi =~ /mysql/i) {
        $dbh->do("set profiling=1") if $ENV{profile};
        foreach my $i (1..4) {
            print "Doing Accurate query...\n";
            my $begin = time;
            my $ret = $dbh->selectrow_arrayref("SELECT count(1) from (
SELECT Path.Path, Filename.Name, Temp.FileIndex, Temp.JobId, LStat, MD5 
FROM ( 
 SELECT FileId, Job.JobId AS JobId, FileIndex, File.PathId AS PathId,
        File.FilenameId AS FilenameId, LStat, MD5 
   FROM Job, File, 
        ( SELECT MAX(JobTDate) AS JobTDate, PathId, FilenameId 
            FROM 
             ( SELECT JobTDate, PathId, FilenameId
                 FROM File JOIN Job USING (JobId) WHERE File.JobId IN ($jobid) 
                UNION ALL 
               SELECT JobTDate, PathId, FilenameId 
                 FROM BaseFiles JOIN File USING (FileId) JOIN Job ON (BaseJobId = Job.JobId) 
                WHERE BaseFiles.JobId IN ($jobid)
             ) AS tmp GROUP BY PathId, FilenameId
        ) AS T1 
  WHERE (Job.JobId IN ( SELECT DISTINCT BaseJobId FROM BaseFiles WHERE JobId IN ($jobid)) OR Job.JobId IN ($jobid)) 
    AND T1.JobTDate = Job.JobTDate 
    AND Job.JobId = File.JobId
    AND T1.PathId = File.PathId
    AND T1.FilenameId = File.FilenameId 
 ) AS Temp 
 JOIN Filename ON (Filename.FilenameId = Temp.FilenameId) 
 JOIN Path ON (Path.PathId = Temp.PathId)
WHERE FileIndex > 0 ORDER BY Temp.JobId, FileIndex ASC
) as a") or die "Can't do query $DBI::errstr";

            print "Big select takes ", scalar(time) - $begin, "secs for $ret->[0] records\n";
            print STDERR 
                "new|$ret->[0]|$nb|", (scalar(time) - $begin),"\n";
            print "Doing Accurate query...\n";
        }
        foreach my $i (1..4) {
            my $begin = time;
            my $ret = $dbh->selectrow_arrayref("SELECT count(1) from (
SELECT Path.Path, Filename.Name, Temp.FileIndex, Temp.JobId, LStat, MD5 
FROM ( 
 SELECT FileId, Job.JobId AS JobId, FileIndex, File.PathId AS PathId,
        File.FilenameId AS FilenameId, LStat, MD5 
   FROM Job, File, 
        ( SELECT MAX(JobTDate) AS JobTDate, PathId, FilenameId 
            FROM File JOIN Job USING (JobId) WHERE File.JobId IN ($jobid) 
           GROUP BY PathId, FilenameId
        ) AS T1 
  WHERE Job.JobId IN ($jobid)
    AND T1.JobTDate = Job.JobTDate 
    AND Job.JobId = File.JobId
    AND T1.PathId = File.PathId
    AND T1.FilenameId = File.FilenameId 
 ) AS Temp 
 JOIN Filename ON (Filename.FilenameId = Temp.FilenameId) 
 JOIN Path ON (Path.PathId = Temp.PathId)
WHERE FileIndex > 0 ORDER BY Temp.JobId, FileIndex ASC
) as a") or die "Can't do query $DBI::errstr";

            print "Big select without base takes ", scalar(time) - $begin, "secs for $ret->[0] records\n";
            print STDERR 
                "base|$ret->[0]|$nb|", (scalar(time) - $begin),"\n";
        }
        if (0) {
        foreach my $i (1..4) {
            print "Doing Old Accurate query...\n";
            my $begin = time;
            my $ret = $dbh->selectrow_arrayref("SELECT count(1) from (
 SELECT Path.Path, Filename.Name, File.FileIndex, File.JobId, File.LStat 
 FROM ( 
  SELECT max(FileId) as FileId, PathId, FilenameId 
    FROM (SELECT FileId, PathId, FilenameId FROM File WHERE JobId IN ($jobid)) AS F 
   GROUP BY PathId, FilenameId 
  ) AS Temp 
 JOIN Filename ON (Filename.FilenameId = Temp.FilenameId) 
 JOIN Path ON (Path.PathId = Temp.PathId) 
 JOIN File ON (File.FileId = Temp.FileId) 
WHERE File.FileIndex > 0 ORDER BY JobId, FileIndex ASC 
) as a") or die "Can't do query $DBI::errstr";

            print "Big select takes ", scalar(time) - $begin, 
                  "secs for $ret->[0] records\n";
            print STDERR 
                "old|$ret->[0]|$nb|", (scalar(time) - $begin),"\n";
        }
        }
    } else {
        foreach my $i (1..4) {
            print "Doing Accurate query...\n";
            my $begin = time;
            my $ret = $dbh->selectrow_arrayref("SELECT count(1) from (
SELECT Path.Path, Filename.Name, Temp.FileIndex, Temp.JobId, LStat, MD5 
FROM ( 
       SELECT DISTINCT ON (FilenameId, PathId) StartTime, JobId, FileId, 
          FileIndex, PathId, FilenameId, LStat, MD5 
     FROM File JOIN Job USING (JobId) 
    WHERE JobId IN ($jobid) 
    ORDER BY FilenameId, PathId, StartTime DESC
) AS Temp 
 JOIN Filename ON (Filename.FilenameId = Temp.FilenameId) 
 JOIN Path ON (Path.PathId = Temp.PathId)
WHERE FileIndex > 0 ORDER BY Temp.JobId, FileIndex ASC
) as a") or die "Can't do query $DBI::errstr";
            
            print "Big select takes ", scalar(time) - $begin,
                   "secs for $ret->[0] records\n";
            print STDERR 
                "new|$ret->[0]|$nb|", (scalar(time) - $begin),"\n";
        }
    }
}

use Data::Dumper;
sub print_mysql_profiles
{
    if ($dbi =~ /mysql/i) {
        my $all = $dbh->selectall_arrayref("show profiles");
        open(FP, ">>log");
        print FP Data::Dumper::Dumper($all);
        
        for(my $i=0; $i < (scalar(@$all)); $i++) {
            my $r = $dbh->selectall_arrayref("show profile cpu for query $i");
            foreach my $j (@$r) {
                print FP sprintf("%6i | %20s | %8s | %3i | %3i\n", $i, @$j);
            }
            print FP "\n";
        }
        
        close(FP);
    }
}
