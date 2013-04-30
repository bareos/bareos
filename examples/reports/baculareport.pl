#!/usr/bin/perl -w
#
# bacula report generation
#
# (C) Arno Lehmann 2005
# IT-Service Lehmann
#

#
# Usage: See funtion print_usage
# or use this script with option --help
#
# Version history:
#
# 0.2     publicly available, works reliable
# 0.3     increasing weight of No. of tapes in guess reliability
#         and including tape capacity guessing when no volumes in subpool
#         using default values from temp. table

use strict;
use DBI;
use Getopt::Long;
use Math::BigInt;

my $version="0.3";
$0 =~ /.*\/([^\/]*)$/;
my $ME = $1;

my $debug = 0;
my $db_host = "";
my $db_user = "bacula";
my $db_database = "mysql:bacula";
my $db_pass = "";

my $do_usage = "";
my $do_version = "";

my @temp_tables;

my @the_pools;

my $out_pooldetails = "";
my $out_bargraph = 1;
my $out_bargraphlen = 70;
my $out_subpools = "";
my $out_subpooldetails = "";
my $out_subbargraph = "";
my $out_cutmarks = "";

# This is the data we're interested in:
# In this array we have a hash reference to each Pool.
# A pool consists of a hash having
#  Name
#  Id
#  BytesTotal
#  VolumesTotal
#  VolumesFull (This is State Full
#  VolumesEmpty (This is Purged and Recycle)
#  VolumesPartly (Append)
#  VolumesAway (Archive, Read-Only)
#  VolumesOther (Busy, Used)
#  VolumesOff (Disabled, Error)
#  VolumesCleaning
#  BytesFree
#  GuessReliability (This is the weighted average of the Reliability
#    of all the Media Type Guesses in this Pool)
#  MediaTypes is an array of references to hashes for collected
#    information for all the Media Types in this pool.
#    This has the same as the pools summary and adds
#   MediaType The String
#   AvgFullBytes (The Avg. Number of Bytes per full Volume)
#   BytesFreeEmpty (The estimated Free Bytes on Empty Volumes)
#   BytesFreePartly
#
# We use: $the_pools[0]->MediaTypes[0]->{MediaType} or
#         $the_pools[1]->Id
# I hope you get the point. I hope I do.

Getopt::Long::Configure("bundling");
GetOptions("host=s"=>\$db_host,
           "user|U=s"=>\$db_user,
           "database|D=s"=>\$db_database,
           "password|P=s"=>\$db_pass,
           "debug=i"=>\$debug,
           "help|h"=>\$do_usage,
           "version|V"=>\$do_version,
           "subpools|s"=>\$out_subpools,
           "subpool-details"=>\$out_subpooldetails,
           "pool-details|d"=>\$out_pooldetails,
           "pool-bargraph!"=>\$out_bargraph,
           "bar-length|l=i"=>\$out_bargraphlen,
           "cutmarks|c"=>\$out_cutmarks,
           "subpool-bargraph"=>\$out_subbargraph
           );

debug_out(100, "I've got
host: $db_host
user: $db_user
database: $db_database
password: $db_pass
debug: $debug
help: $do_usage
version: $do_version
output requested:
  pool details: $out_pooldetails
  subpools: $out_subpools
  subpool details: $out_subpooldetails
  bargraph: $out_bargraph
  subpool bargraph: $out_subbargraph
  bar length: $out_bargraphlen
  cutmarks: $out_cutmarks
I was called as $0 and am version $version.
Was that helpful?");

if ($do_usage) {
    do_usage();
    exit 1;
}
if ($do_version) {
    do_version();
    exit 1;
}

$out_subpools = 1 if ($out_subpooldetails);
$out_subpools = 1 if ($out_subbargraph);
$out_bargraphlen = 70 if (15 > $out_bargraphlen);
$out_bargraphlen = 70 if (200 < $out_bargraphlen);
$out_bargraph = 1 if (! $out_pooldetails);

debug_out(100, "Output options after dependencies:
  pool details: $out_pooldetails
  subpools: $out_subpools
  subpool details: $out_subpooldetails
  bargraph: $out_bargraph
  subpool bargraph: $out_subbargraph
  bar length: $out_bargraphlen
  cutmarks: $out_cutmarks
");

my (undef, $min, $hour, $mday, $mon, $year) = localtime();
$year += 1900;
$mon = sprintf("%02i", $mon+1);
$mday = sprintf("%02i", $mday);
$min = sprintf("%02i", $min);
$hour = sprintf("%02i", $hour);
print "bacula volume / pool status report $year-$mon-$mday $hour:$min\n",
    "Volumes Are Full, Other, Append, Empty, aWay or X (error)\n";
my $dbconn = "dbi:" . $db_database;
$dbconn .= "\@" . $db_host if $db_host;
debug_out(40, "DBI connect with $dbconn");

my $h_db = DBI->connect($dbconn,
                        $db_user, $db_pass,
                        { PrintError => 0,
                          AutoCommit => 1 }
                        ) || die DBI::errstr;
debug_out(10, "Have database connection $h_db");

debug_out(100, "creating temp tables...");

$h_db->do("CREATE TABLE alrep_M(PoolId INT(10) UNSIGNED,MediaType TINYBLOB)") || debug_abort(0, "Can't create temp table alrep_M - another script running?");
unshift  @temp_tables, "alrep_M";
debug_out(45, "Table alrep_M created.");


debug_out(40, "All tables done.");

debug_out(40, "Filling temp tables...");
if ($h_db->do("INSERT INTO alrep_M SELECT Pool.PoolId,Media.MediaType FROM Pool,Media WHERE Pool.PoolId=Media.PoolId GROUP BY PoolId,MediaType")) {
    debug_out(45, "PoolId-MediaType table populated.");
} else {
    debug_abort(0, "Couldn't populate PoolId and MediaType table alrep_M.");
}

debug_out(40, "All tables done.");

debug_out(40, "Getting Pool Names.");
my $h_st = $h_db->prepare("SELECT Name,PoolId FROM Pool ORDER BY Name") ||
    debug_abort(0, "Couldn't get Pool Information.", $h_db->errstr());
$h_st->execute() || debug_abort(0, "Couldn't query Pool information.",
                                $h_db->errstr());
my $pools;
while ($pools=$h_st->fetchrow_hashref()) {
    process_pool($pools->{Name}, $pools->{PoolId})
}
debug_out(10, "All Pool data collected.");
debug_out(7, "Pools analyzed: $#the_pools.");
debug_out(10, "Going to print...");

my $pi;
for $pi (@the_pools) {
    output_pool($pi);
}

debug_out(10, "Program terminates normally.");
do_closedb();
debug_out(10, "Finishing.");
exit 0;

=pod

=head1 NAME

baculareport.pl - a script to produce some bacula reports out of
the catalog database.

=head1 SYNTAX

B<baculareport.pl> B<--help>|B<-h>

B<baculareport.pl> B<--version>|B<-V>

B<baculareport.pl> [B<--host> I<Hostname>] [B<--user>|B<-U> I<Username>]
[B<--database>|B<-D> I<Database>] [B<--password>|B<-P> I<Password>]
[B<--debug> I<Level>] [B<--pool-details>|B<-d>]
[B<--pool-bargraph>|B<--nopool-bargraph>] [B<--subpools>|B<-s>]
[B<--subpool-details>] [B<--subpool-bargraph>] [B<--bar-length>|B<-l>
I<Length>] [B<--cutmarks>|B<-c>]

The long options can be abbreviated, as long as they remain unique.
Short options (and values) can be grouped, for more information see
B<perldoc Getopt::Long>.

=head1 DESCRIPTION

B<baculareport.pl> accesses the catalog used by the backup program bacula
to produce some report about pool and volume usage.

The command line options B<--host> I<host>, B<--user> or B<-U>
I<user>, B<--database> or B<-D> and B<--password> or B<-P> define the
database to query. See below for security considerations concerning
databse passwords.

The I<database> must be given in perl's B<DBI>-syntax, as in
I<mysql:bacula>. Currently, only MySQL is supported, though PostgreSQL
should work with only minor modifications to B<baculareport.pl>.

Output of reports is controlled using the command-line switches
B<--*pool*>, B<--bar-length> and B<--cutmarks> or there one-letter
equivalents.

The report for a pool can contain a one-line overview of the volumes
in that pool, giving the numbers of volumes in different states, the
total bytes stored and an estimate of the available capacity.

The estimated consists of a percentage describing the reliability of
this estimate and the guessed free capacity.

A visual representation of the pools state represented as a bar graph,
together with the number of full, appendable and free volumes is the
default report.

The length of this graph can be set with B<--bar-length> or B<-l>
I<length in characters>.

As a pool can contain volumes of different media type, the report's
output can include the information about those collections of volumes
called subpools in B<baculareport.pl>s documentation.

The subpool overview data presents the same information about the
volumes the pool details have, but includes the media type and excludes
the free capacity guess.

Subpool details report the average amount of data on full volumes,
together with what is estimated to be available on appendable and empty
volumes. A measurement on the reliability of this estimate is given as a
percent value. See below in L<"CAPACITY GUESSING"> for more
information.

Finally, a bar graph representing this subpools fill-level can be printed.
For easier overview it is scaled like the pools bargraph.

B<--cutmarks> or B<-c> prints some marks above each pool report to
make cutting the report easier if you want to file it.

Sample reports are in L<"SAMPLE REPORTS">.

The B<--debug>-option activates debug output. Without understanding the
source code this will not be helpful. See below L<"DEBUG OUTPUT">.

=head1 DATABASE ACCESS AND SECURITY

baculareport.pl needs access to baculas catalog. This might introduce
a security risk if the database access password is published to people who
shouldn't know it, but need to create reports.

The solution is to set up a database account which can only read from
baculas catalog. Use your favorite database administration tool for
this.

Command line passing of the password is also not really secure - anybody
with sufficient access rights can read the command line etc. So, if you use this script on a multi-user machine, you are well advised to

=over 4

=item 1.

I<use a special, limited database account for catalog access>, or

=item 2.

I<modify the hard-coded default database password to the one
you have set.>

=back

This should limit security risks to a minimum.

If B<baculareport.pl> is used by your backup admin only, don't bother
- she has access to all your data anyway. (B<No. Please not. This was
just a joke!>)

=head1 SAMPLE REPORTS

The reports can be customized using the above explained command line switches.
Some examples are:

    bacula volume / pool status report 2005-01-18 23:40
    Volumes Are Full, Other, Append, Empty, aWay or X (error)

    Pool           Diff
      ######################################################----------------
      |0%          |20%          |40%          |60%          |80%      100%|
      48.38GB used                                     Rel: 24% free 13.88GB
      17 F Volumes                                       3 A and 4 E Volumes

    Pool           Full
      #######################################-------------------------------
      |0%          |20%          |40%          |60%          |80%      100%|
      310.66GB used                                   Rel: 58% free 241.64GB
      43 F Volumes                                      2 A and 14 E Volumes

    Pool           Incr
      #######################################################---------------
      |0%          |20%          |40%          |60%          |80%      100%|
      28.51GB used                                Rel: 0% (def.) free 7.61GB
      0 F Volumes                                        3 A and 4 E Volumes

    Pool        TMPDisk
      Nothing to report.

This is the sort of report you get when you use this script without
any special output options. After a short header, for all pools in
the catalog a graphic representation of its usage is
printed. Below that, you find some essential information: The
capacity used, a guess of the remaining capacity (see
L<"CAPACITY GUESSING"> below), and
an overview of the volumes: Here, in pool Incr we have no full
volumes, 3 appendable ones and 4 empty volumes.

In this example, the pool TMPDisk does not contain anything which can
be reported.

Following you have an example with all output options set.

      -                                                 -
  Pool           Incr
    ###################################################----
    |0%          |25%          |50%         |75%      100%|
    10 Volumes (2 F, 0 O, 2 A, 6 E, 0 W, 0 X) Total 59.64GB Rel: 29% avail.: 4.57GB
       Details by Mediatype:
       DDS1 (0 F, 0 O, 1 A, 4 E, 0 W, 0 X) Total 4.53GB
       ####                                                
       |0%         |25%         |50%         |75%     100%|
       Avg, avail. Partly, Empty, Total, Rel.: N/A N/A N/A N/A 0%
       DDS2 (0 F, 0 O, 0 A, 2 E, 0 W, 0 X) Total 0.00B
       Avg, avail. Partly, Empty, Total, Rel.: N/A N/A N/A N/A 0%
       DLTIV (2 F, 0 O, 1 A, 0 E, 0 W, 0 X) Total 55.11GB
       #############################################----   
       |0%         |25%         |50%         |75%     100%|
       Avg, avail. Partly, Empty, Total, Rel.: 19.89GB 4.57GB N/A 4.57GB 96%
      -                                                 -
  Pool        TMPDisk
    Nothing to report.
    1 Volumes (0 F, 0 O, 0 A, 1 E, 0 W, 0 X) Total 0.00B Rel: 0% avail.: 0.00B
       Details by Mediatype:
       File (0 F, 0 O, 0 A, 1 E, 0 W, 0 X) Total 0.00B
       Nothing to report.
       Avg, avail. Partly, Empty, Total, Rel.: N/A N/A N/A N/A 0%

Cut marks are included for easier cutting in case you want to file the
printed report. Then, the length of the bar graphs was changed.

More detail for the pools is shown: Not only the overwiev graphics,
but also a listing of the status of all media in this
pool, followed by the reliability of the guess of available
capacity and the probable available capacity itself.

After this summary you find a similar report for all media types in
this pool. Here, the media type starts the details line. The next
line is a breakdown of the capacity inside this subpool: The
average capacity of the full volumes, followed by the probable
available capacity on appendable and empty volumes. Total is the
probable free capacity on these volumes, and Rel is the
reliability of the capacity guessing.

Note that some of the items are not always displayed: A pool or
subpool with no bytes in it will not have a bar graph, and some of
the statistical data is marked as N/A for not available.

The above output was generated with the following command:

B<< C<< 
 baculareport.pl --password <yestherewasapassword>\
 --pool-bargraph  --pool-details --subpools\
 --subpool-details --subpool-bargraph --bar-length 55\
 --cutmarks >> >>

The following command would have given the same output:

B<< C<<
 baculareport.pl -P <therightpassphrase> -csdl55\
 --subpool-d --subpool-b >> >>

=head1 CAPACITY GUESSING

For empty and appendable volumes, the average capacity of the full
volumes is used as the base for estimating what can be
stored. This usually depends heavily on the type of data to store,
and of course this works only with volumes of the same nominal
capacity.

The reliability of all this guesswork is expressed based on the
standard deviation among the full volumes, scaled to percent. 100%
is a very reliable estimate (Note: NOT absolutely reliable!) while
a small percentage (from personal experience: below 60-70 percent)
means that you shouldn't rely on the reported available data storage.

To determine the overall reliability in a pool, the reliabilites of
the subpools are weighted - a subpool with many volumes has a higer
influence on overall reliability.

Keep in mind that the reported free capacities and reliabilities can
only be a help and don't rely on these figures alone. Keep enough
spare tapes available!

Default capacities for some media types are included now. Consider this
feature a temporarily kludge - At the moment, there is a very simple
media capacity guessing implemented. Search for the function
`get_default_bytes' and modify it to your needs.

In the future, I expect some nominal volume capacity knowledge inside
baculas catalog, and when this is available, that data will be used.

Capacity estimates with defaults in the calculation are marked with
B<(def.)> after the reliability percentage. If you see B<0% (def.)>
only the defaults are used because no full tapes were available.

=head1 DEBUG OUTPUT

Debugging, or more generally verbose output, is activated by the
--debug <level> command switch.

The higher the level, the more output you get.

Currently, levels 10 and up are real debugging output.  Levels above
100 are not used. I<With debug level 100 (and above) the database
password is printed!>

The debug levels used are:

=over 4

=item 1

Some warnings are printed.

=item 10

Program Flow is reported.

=item 15

More detailed Program flow, for example loops.

=item 40

Database actions are printed.

=item 45

Table actions are reported.

=item 48

Even more database activity.

=item 100

All internal state data is printed. Beware: This includes the database
password!

=back

=head1 BUGS

Probably many. If you find one, notify the author. Better: notify me
how to correct it.

Currently this script works only with MySQL and catalog version 8
(probably older versions as well, but that is untested).

=head1 AUTHOR

Arno Lehmann al@its-lehmann.de

=head1 LICENSE

This is copyrighted work: (C) 2005 Arno Lehmann IT-Service Lehmann

Use, modification and (re-)distribution are allowed provided this
license and the names of all contributing authors are included.

No author or contributor gives any warranty on this script. If you
want to use it, you are all on your own. Please read the documentation,
and, if you feel unsure, read and understand the sourcecode.

The terms and idea of the GNU GPL, version 2 or, at you option, any
later version, apply. See http://www.fsf.org.

You can contact the author using the above email address. I will try to
answer any question concerning this script, but still - no promises!

Bacula is (C) copyright 2000-2006 Free Software Foundation Europe e.V.  See http://www.bacula.org.

(Bacula consulting available.)

=cut

sub process_pool {
    my %pool = (BytesTotal=>0,
                VolumesTotal=>0,
                VolumesFull=>0,
                VolumesEmpty=>0,
                VolumesPartly=>0,
                VolumesAway=>0,
                VolumesOther=>0,
                VolumesOff=>0,
                VolumesCleaning=>"Not counted",
                BytesFree=>0,
                GuessReliability=>0,
                AvgFullUsesDefaults=>""
                );
    debug_out(10, "Working on Pool $pools->{Name}.");
    $pool{Name} = shift;
    $pool{Id} = shift;
    my @subpools;

    debug_out(30, "Pool $pool{Name} is Id $pool{Id}.");
    my $h_st = $h_db->prepare("SELECT MediaType FROM alrep_M WHERE
    PoolId = $pool{Id} ORDER BY MediaType") ||
        debug_abort(0,
                    "Can't query Media table.", $h_st->errstr());
    $h_st->execute() ||
        debug_abort(0,
                    "Can't get Media Information", $h_st->errstr());
    while (my $mt=$h_st->fetchrow_hashref()) {
# In this loop, we process one media type in a pool
        my %subpool = (MediaType=>$mt->{MediaType});
        debug_out(45, "Working on MediaType $mt->{MediaType}.");
        my $h_qu =
            $h_db->prepare("SELECT COUNT(*) AS Nr,SUM(VolBytes) AS Bytes," .
                           "STD(VolBytes) AS Std,AVG(VolBytes) AS Avg " .
                           "FROM Media WHERE (PoolId=$pool{Id}) AND " .
                           "(MediaType=" . $h_db->quote($mt->{MediaType}) .
                           ") AND (VolStatus=\'Full\')")
                || debug_abort(0,
                               "Can't query Media Summary Information by MediaType.",
                               $h_db->errstr());
        debug_out(48, "Query active: ", $h_qu->{Active}?"Yes":"No");
        debug_out(45, "Now selecting Summary Information for $pool{Name}:$mt->{MediaType}:Full");
        debug_out(48, "Query: ", $h_qu->{Statement}, "Params: ",
                  $h_qu->{NUM_OF_PARAMS}, " Rows: ", $h_qu->rows);
        $h_qu->execute();
        debug_out(48, "Result:", $h_qu->rows(), "Rows.");
# Don't know why, but otherwise the handle access
# methods result in a warning...
        $^W = 0;
        if (1 == $h_qu->rows()) {
            if (my $qr = $h_qu->fetchrow_hashref) {
                debug_out(45, "Got $qr->{Nr} and $qr->{Bytes}.");
                $subpool{VolumesFull} = $qr->{Nr};
                $subpool{VolumesTotal} += $qr->{Nr};
                $subpool{BytesTotal} = $qr->{Bytes} if (defined($qr->{Bytes}));
                if (defined($qr->{Bytes}) && (0 < $qr->{Bytes}) &&
                    (0 < $qr->{Nr})) {
                    $subpool{AvgFullBytes} = int($qr->{Bytes} / $qr->{Nr});
                } else {
                    $subpool{AvgFullBytes} = get_default_bytes($mt->{MediaType});
                    $subpool{AvgFullUsesDefaults} = 1;
                }
                if (defined($qr->{Std}) &&
                    defined($qr->{Avg}) &&
                    (0 < $qr->{Avg})) {
#                   $subpool{GuessReliability} = 100-(100*$qr->{Std}/$qr->{Avg});
                    $subpool{GuessReliability} =
                        100 -                    # 100 Percent minus...
                            ( 100 *              # Percentage of 
                              ( $qr->{Std}/$qr->{Avg} ) *  # V
                              ( 1 - 1 / $qr->{Nr} )        # ... the more tapes
                                                           # the better the guess
                              );
                } else {
                    $subpool{GuessReliability} = 0;
                }
            } else {
                debug_out(1, "Can't get Media Summary Information by MediaType.",
                            $h_qu->errstr());
                $subpool{VolumesFull} = 0;
                $subpool{BytesTotal} = 0;
                $subpool{GuessReliability} = 0;
                $subpool{AvgFullBytes} = -1;
            }
        } else {
            debug_out(45, "Got nothing: ", (defined($h_qu->errstr()))?$h_qu->errstr():"No error.");
        }
        $^W = 1;
# Here, Full Media are done
        debug_out(15, "Full Media done. Now Empty ones.");
        $h_qu =
            $h_db->prepare("SELECT COUNT(*) AS Nr " .
                           "FROM Media WHERE (PoolId=$pool{Id}) AND " .
                           "(MediaType=" . $h_db->quote($mt->{MediaType}) .
                           ") AND ((VolStatus=\'Purged\') OR " .
                           "(VolStatus=\'Recycle\'))")
                || debug_abort(0,
                               "Can't query Media Summary Information by MediaType.",
                               $h_db->errstr());
        debug_out(48, "Query active: ", $h_qu->{Active}?"Yes":"No");
        debug_out(45, "Now selecting Summary Information for $pool{Name}:$mt->{MediaType}:Recycle OR Purged");
        debug_out(48, "Query: ", $h_qu->{Statement}, "Params: ",
                  $h_qu->{NUM_OF_PARAMS}, " Rows: ", $h_qu->rows);
        $h_qu->execute();
        debug_out(48, "Result:", $h_qu->rows(), "Rows.");
# Don't know why, but otherwise the handle access
# methods result in a warning...
        $^W = 0;
        if (1 == $h_qu->rows()) {
            if (my $qr = $h_qu->fetchrow_hashref) {
                debug_out(45, "Got $qr->{Nr} and $qr->{Bytes}.");
                $subpool{VolumesEmpty} = $qr->{Nr};
                $subpool{VolumesTotal} += $qr->{Nr};
                if (($subpool{AvgFullBytes} > 0) && ($qr->{Nr} > 0)) {
                    $subpool{BytesFreeEmpty} = $qr->{Nr} * $subpool{AvgFullBytes};
                } else {
                    $subpool{BytesFreeEmpty} = -1;
                }
            } else {
                debug_out(1, "Can't get Media Summary Information by MediaType.",
                            $h_qu->errstr());
                $subpool{VolumesEmpty} = 0;
                $subpool{BytesFreeEmpty} = 0;
            }
        } else {
            debug_out(45, "Got nothing: ", (defined($h_qu->errstr()))?$h_qu->errstr():"No error.");
        }
        $^W = 1;
# Here, Empty Volumes are processed.

        debug_out(15, "Empty Media done. Now Partly filled ones.");
        $h_qu =
            $h_db->prepare("SELECT COUNT(*) AS Nr,SUM(VolBytes) AS Bytes " .
                           "FROM Media WHERE (PoolId=$pool{Id}) AND " .
                           "(MediaType=" . $h_db->quote($mt->{MediaType}) .
                           ") AND (VolStatus=\'Append\')")
                || debug_abort(0,
                               "Can't query Media Summary Information by MediaType.",
                               $h_db->errstr());
        debug_out(48, "Query active: ", $h_qu->{Active}?"Yes":"No");
        debug_out(45, "Now selecting Summary Information for $pool{Name}:$mt->{MediaType}:Append");
        debug_out(48, "Query: ", $h_qu->{Statement}, "Params: ",
                  $h_qu->{NUM_OF_PARAMS}, " Rows: ", $h_qu->rows);
        $h_qu->execute();
        debug_out(48, "Result:", $h_qu->rows(), "Rows.");
# Don't know why, but otherwise the handle access
# methods result in a warning...
        $^W = 0;
        if (1 == $h_qu->rows()) {
            if (my $qr = $h_qu->fetchrow_hashref) {
                debug_out(45, "Got $qr->{Nr} and $qr->{Bytes}.");
                $subpool{VolumesPartly} = $qr->{Nr};
                $subpool{VolumesTotal} += $qr->{Nr};
                $subpool{BytesTotal} += $qr->{Bytes};
                if (($subpool{AvgFullBytes} > 0) && ($qr->{Nr} > 0)) {
                    $subpool{BytesFreePartly} = $qr->{Nr} * $subpool{AvgFullBytes} - $qr->{Bytes};
                    $subpool{BytesFreePartly} = $qr->{Nr} if $subpool{BytesFreePartly} < 1;
                } else {
                    $subpool{BytesFreePartly} = -1;
                }
            } else {
                debug_out(1, "Can't get Media Summary Information by MediaType.",
                            $h_qu->errstr());
                $subpool{VolumesPartly} = 0;
                $subpool{BytesFreePartly} = 0;
            }
        } else {
            debug_out(45, "Got nothing: ", (defined($h_qu->errstr()))?$h_qu->errstr():"No error.");
        }
        $^W = 1;
# Here, Partly filled volumes are done

        debug_out(15, "Partly Media done. Now Away ones.");
        $h_qu =
            $h_db->prepare("SELECT COUNT(*) AS Nr,SUM(VolBytes) AS Bytes " .
                           "FROM Media WHERE (PoolId=$pool{Id}) AND " .
                           "(MediaType=" . $h_db->quote($mt->{MediaType}) .
                           ") AND ((VolStatus=\'Archive\') OR " .
                           "(VolStatus=\'Read-Only\'))")
                || debug_abort(0,
                               "Can't query Media Summary Information by MediaType.",
                               $h_db->errstr());
        debug_out(48, "Query active: ", $h_qu->{Active}?"Yes":"No");
        debug_out(45, "Now selecting Summary Information for $pool{Name}:$mt->{MediaType}:Recycle OR Purged");
        debug_out(48, "Query: ", $h_qu->{Statement}, "Params: ",
                  $h_qu->{NUM_OF_PARAMS}, " Rows: ", $h_qu->rows);
        $h_qu->execute();
        debug_out(48, "Result:", $h_qu->rows(), "Rows.");
# Don't know why, but otherwise the handle access
# methods result in a warning...
        $^W = 0;
        if (1 == $h_qu->rows()) {
            if (my $qr = $h_qu->fetchrow_hashref) {
                debug_out(45, "Got $qr->{Nr} and $qr->{Bytes}.");
                $subpool{VolumesAway} = $qr->{Nr};
                $subpool{VolumesTotal} += $qr->{Nr};
                $subpool{BytesTotal} += $qr->{Bytes};
            } else {
                debug_out(1, "Can't get Media Summary Information by MediaType.",
                            $h_qu->errstr());
                $subpool{VolumesAway} = 0;
            }
        } else {
            debug_out(45, "Got nothing: ", (defined($h_qu->errstr()))?$h_qu->errstr():"No error.");
        }
        $^W = 1;
# Here, Away Volumes are processed.

        debug_out(15, "Away Media done. Now Other ones.");
        $h_qu =
            $h_db->prepare("SELECT COUNT(*) AS Nr,SUM(VolBytes) AS Bytes " .
                           "FROM Media WHERE (PoolId=$pool{Id}) AND " .
                           "(MediaType=" . $h_db->quote($mt->{MediaType}) .
                           ") AND ((VolStatus=\'Busy\') OR " .
                           "(VolStatus=\'Used\'))")
                || debug_abort(0,
                               "Can't query Media Summary Information by MediaType.",
                               $h_db->errstr());
        debug_out(48, "Query active: ", $h_qu->{Active}?"Yes":"No");
        debug_out(45, "Now selecting Summary Information for $pool{Name}:$mt->{MediaType}:Recycle OR Purged");
        debug_out(48, "Query: ", $h_qu->{Statement}, "Params: ",
                  $h_qu->{NUM_OF_PARAMS}, " Rows: ", $h_qu->rows);
        $h_qu->execute();
        debug_out(48, "Result:", $h_qu->rows(), "Rows.");
# Don't know why, but otherwise the handle access
# methods result in a warning...
        $^W = 0;
        if (1 == $h_qu->rows()) {
            if (my $qr = $h_qu->fetchrow_hashref) {
                debug_out(45, "Got $qr->{Nr} and $qr->{Bytes}.");
                $subpool{VolumesOther} = $qr->{Nr};
                $subpool{VolumesTotal} += $qr->{Nr};
                $subpool{BytesTotal} += $qr->{Bytes};
            } else {
                debug_out(1, "Can't get Media Summary Information by MediaType.",
                            $h_qu->errstr());
                $subpool{VolumesOther} = 0;
            }
        } else {
            debug_out(45, "Got nothing: ", (defined($h_qu->errstr()))?$h_qu->errstr():"No error.");
        }
        $^W = 1;
# Here, Other Volumes are processed.

        debug_out(15, "Other Media done. Now Off ones.");
        $h_qu =
            $h_db->prepare("SELECT COUNT(*) AS Nr,SUM(VolBytes) AS Bytes " .
                           "FROM Media WHERE (PoolId=$pool{Id}) AND " .
                           "(MediaType=" . $h_db->quote($mt->{MediaType}) .
                           ") AND ((VolStatus=\'Disabled\') OR " .
                           "(VolStatus=\'Error\'))")
                || debug_abort(0,
                               "Can't query Media Summary Information by MediaType.",
                               $h_db->errstr());
        debug_out(48, "Query active: ", $h_qu->{Active}?"Yes":"No");
        debug_out(45, "Now selecting Summary Information for $pool{Name}:$mt->{MediaType}:Recycle OR Purged");
        debug_out(48, "Query: ", $h_qu->{Statement}, "Params: ",
                  $h_qu->{NUM_OF_PARAMS}, " Rows: ", $h_qu->rows);
        $h_qu->execute();
        debug_out(48, "Result:", $h_qu->rows(), "Rows.");
# Don't know why, but otherwise the handle access
# methods result in a warning...
        $^W = 0;
        if (1 == $h_qu->rows()) {
            if (my $qr = $h_qu->fetchrow_hashref) {
                debug_out(45, "Got $qr->{Nr} and $qr->{Bytes}.");
                $subpool{VolumesOff} = $qr->{Nr};
                $subpool{VolumesTotal} += $qr->{Nr};
                            } else {
                debug_out(1, "Can't get Media Summary Information by MediaType.",
                          $h_qu->errstr());
                $subpool{VolumesOff} = 0;
            }
        } else {
            debug_out(45, "Got nothing: ", (defined($h_qu->errstr()))?$h_qu->errstr():"No error.");
        }
        $^W = 1;
# Here, Off Volumes are processed.

        if ((0 < $subpool{BytesFreeEmpty}) ||
            (0 < $subpool{BytesFreePartly})) {
            debug_out(15, "We have a guess.");
            $subpool{BytesFree} = 0;
            $subpool{BytesFree} += $subpool{BytesFreeEmpty} if
                (0 < $subpool{BytesFreeEmpty});
            $subpool{BytesFree} += $subpool{BytesFreePartly} if
                (0 < $subpool{BytesFreePartly});
        } else {
            debug_out(15, "Neither Empty nor Partly BytesFree available - no guess!");
            $subpool{BytesFree} = -1;
        }
        if ($subpool{AvgFullUsesDefaults}) {
            debug_out(15, "Average Full Capacity calculation included defaults.");
            $pool{AvgFullUsesDefaults} = 1;
        }
        $pool{BytesTotal} += $subpool{BytesTotal};
        $pool{VolumesTotal} += $subpool{VolumesTotal};
        $pool{VolumesFull} += $subpool{VolumesFull};
        $pool{VolumesEmpty} += $subpool{VolumesEmpty};
        $pool{VolumesPartly} += $subpool{VolumesPartly};
        $pool{VolumesAway} += $subpool{VolumesAway};
        $pool{VolumesOther} += $subpool{VolumesOther};
        $pool{VolumesOff} += $subpool{VolumesOff};
# not counted!
#       $pool{VolumesCleaning} += $subpool{VolumesCleaning};

        $pool{BytesFree} += $subpool{BytesFree} if ($subpool{BytesFree} > 0);

        debug_out(10, "Now storing sub-pool with MediaType", $subpool{MediaType});
        push @subpools, \%subpool;
    }
    $pool{MediaTypes} = \@subpools;
# GuessReliability
    my $allrels = 0;
    my $subcnt = scalar(@{$pool{MediaTypes}});
    my $guess_includes_defaults = 0;
    debug_out(10, "Summarizing Reliabilities from $subcnt sub-pools.");
    foreach my $rel (@{$pool{MediaTypes}}) {
        $allrels += $rel->{GuessReliability} * $rel->{VolumesTotal};
    }
    debug_out(15, "We have $allrels summed/weighted reliabilites and $pool{VolumesTotal} Volumes.");
    if ($pool{VolumesTotal} > 0) {
        $pool{GuessReliability} = $allrels / $pool{VolumesTotal};
    } else {
        $pool{GuessReliability} = "N/A";
    }
    push @the_pools, \%pool;
}

sub output_pool {
    debug_out(10, "Printing pool data.");
    my $pool = shift;
    $pool->{GuessReliability} += 1000.0 if
        (($pool->{GuessReliability} ne "N/A") &&
         $pool->{AvgFullUsesDefaults});
    printf((($out_cutmarks)?"    -" . " " x ($out_bargraphlen - 6) . "-\n":
           "\n") .
           "Pool%15.15s%s\n", "$pool->{Name}",
           ($debug>=5)?sprintf(" %5.9s", "(" . $pool->{Id} . ")"):"");
    my $poolbarbytes = $pool->{BytesTotal} + $pool->{BytesFree};
    if ($out_bargraph) {
        print bargraph($out_bargraphlen, 2,
                       $poolbarbytes,
                       $pool->{BytesTotal}, $pool->{BytesFree});
    }
    if ($out_pooldetails) {
        print("  $pool->{VolumesTotal} Volumes ($pool->{VolumesFull} F, ",
              "$pool->{VolumesOther} O, $pool->{VolumesPartly} A, ",
              "$pool->{VolumesEmpty} E, $pool->{VolumesAway} W, ",
              "$pool->{VolumesOff} X) Total ",
              human_readable("B", $pool->{BytesTotal}),
              " Rel: ", human_readable("P", $pool->{GuessReliability}),
              " avail.: ", human_readable("B", $pool->{BytesFree}), "\n");
    } else {
        print bargraph_legend($out_bargraphlen, 2,
                              $pool->{BytesTotal} + $pool->{BytesFree},
                              $pool->{BytesTotal}, $pool->{BytesFree},
                              $pool->{VolumesFull}, $pool->{VolumesPartly},
                              $pool->{VolumesEmpty}, $pool->{GuessReliability});
    }
    if ($out_subpools) {
        debug_out(10, "Printing details:", $#{$pool->{MediaTypes}}+1, "MediaTypes");
        if (0 < scalar($pool->{MediaTypes})) {
            print "     Details by Mediatype:\n";
            foreach my $i (@{$pool->{MediaTypes}}) {
                debug_out(15, "Media Type $i->{MediaType}");
                $i->{GuessReliability} += 1000.0 if ($i->{AvgFullUsesDefaults});
                print("     $i->{MediaType} ($i->{VolumesFull} F, ",
                      "$i->{VolumesOther} O, $i->{VolumesPartly} A, ",
                      "$i->{VolumesEmpty} E, $i->{VolumesAway} W, " ,
                      "$i->{VolumesOff} X) Total ",
                      human_readable("B", $i->{BytesTotal}), "\n");
                if ($out_subbargraph) {
                    print bargraph($out_bargraphlen - 3, 5,
                                   $poolbarbytes,
                                   $i->{BytesTotal},
                                   $i->{BytesFree});
                }
                if ($out_subpooldetails) {
                    print "     Avg, avail. Partly, Empty, Total, Rel.: ",
                    ($i->{AvgFullBytes} > 0)?human_readable("B", $i->{AvgFullBytes}):"N/A", " ",
                    ($i->{BytesFreePartly} > 0)?human_readable("B", $i->{BytesFreePartly}):"N/A", " ",
                    ($i->{BytesFreeEmpty} > 0)?human_readable("B", $i->{BytesFreeEmpty}):"N/A", " ",
                    ($i->{BytesFree} > 0)?human_readable("B", $i->{BytesFree}):"N/A", " ",
                    human_readable("P", $i->{GuessReliability}), "\n";
                } else {
                    print bargraph_legend($out_bargraphlen - 3, 5,
                                          $poolbarbytes,
                                          $i->{BytesTotal},
                                          $i->{BytesFree},
                                          $i->{VolumesFull},
                                          $i->{VolumesPartly},
                                          $i->{VolumesEmpty},
                                          $i->{GuessReliability}
                                          ) if ($out_subbargraph);
                }
            }
        }
    }
}

sub bargraph_legend {
    debug_out(15, "bargraph_legend called with ", join(":", @_));
    my ($len, $pad, $b_all, $b_tot, $b_free, $v_total, $v_app,
        $v_empty, $g_r) = @_;
    if ((9 == scalar(@_)) &&
        defined($len) && ($len >= 0) && ($len =~ /^\d+$/) &&
        defined($pad) && ($pad >= 0) && ($pad =~ /^\d+$/) &&
        defined($b_all) && ($b_all =~ /^\d+$/) &&
        defined($b_tot) && ($b_tot =~ /^-?\d+$/) &&
        defined($b_free) && ($b_free =~ /^-?\d+$/) &&
        defined($v_total) && ($v_total =~ /^\d+$/) &&
        defined($v_app) && ($v_app =~ /^\d+$/) &&
        defined($v_empty) && ($v_empty =~ /^\d+$/) &&
        ($g_r =~ /^([+-]?)(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?/)
        ) {
        return "" if ( 0 == $b_all);
        $b_tot = 0 if ($b_tot < 0);
        $b_free = 0 if ($b_free < 0);
        return "" if (0 == ($b_tot + $b_free));
        my ($ll, $lm);
        my $l1 = human_readable("B", $b_tot) . " used ";
        my $l2 = "Rel: " . human_readable("P", $g_r) . " free " . human_readable("B", $b_free);
        $ll = $l1 . " " x ($len - length($l1) - length($l2)) . $l2;
        $l1 = $v_total . " F Volumes ";
        $l2 = $v_app . " A and " . $v_empty . " E Volumes";
        $lm = $l1 . " " x ($len - length($l1) - length($l2)) . $l2;
        return " " x $pad . $ll . "\n" .
            " " x $pad . $lm . "\n";
    } else {
        debug_out(1, "bargraph_legend called without proper parameters");
        return "";
    }
}

sub bargraph {
    debug_out(15, "bargraph called with ", join(":", @_));
    my ($len, $pad, $p_all, $p_full, $p_empty) = @_;
    if ((5 == scalar(@_)) &&
        defined($len) && ($len >= 0) && ($len =~ /^\d+$/) &&
        defined($pad) && ($pad >= 0) && ($pad =~ /^\d+$/) &&
        defined($p_full) && ($p_full =~ /^-?\d+$/) &&
        defined($p_empty) && ($p_empty =~ /^-?\d+$/) &&
        defined($p_all) && ($p_all >= $p_full + $p_empty) &&
        ($p_all =~ /^\d+$/)
        ) {
        $len = 12 if ($len < 12);
        $p_full = 0 if ($p_full < 0);
        $p_empty = 0 if ($p_empty < 0);
        debug_out(15, "bargraph: len $len all $p_all full $p_full empty $p_empty");
        return " " x $pad . "Nothing to report.\n" if (0 == $p_all);
        return "" if (0 == ($p_full + $p_empty));
        my $contperbox = $p_all / $len;
        my $boxfull = sprintf("%u", ($p_full / $contperbox) + 0.5);
        my $boxempty = sprintf("%u", ($p_empty / $contperbox) + 0.5);
        my $boxnon = $len - $boxfull - $boxempty;
        debug_out(15, "bargraph: output $boxfull $boxempty $boxnon");
        $contperbox = sprintf("%f", $len / 100.0);
        my $leg = "|0%";
        my $ticks = sprintf("%u", ($len-12) / 12.5);
        my $be = 0;
        my $now = 4;
        for my $i (1..$ticks) {
            debug_out(15, "Tick loop. Previous pos: $now Previous Tick: ", $i-1);
            my $pct = sprintf("%f", 100.0 / ($ticks+1.0) * $i);
            $be = sprintf("%u", 0.5 + ($pct * $contperbox));
            debug_out(15, "Tick $i ($pct percent) goes to pos $be. Chars per Percent: $contperbox");
            my $bl = $be - $now;
            debug_out(15, "Need $bl blanks to fill up.");
            $leg .= " " x $bl . sprintf("|%2u%%", 0.5 + $pct);
            $now = $be + 4;
        }
        debug_out(15, "Fillup... Now at pos $now and $contperbox char/pct.");
        $be = $len - $now - 4;
        $leg .= " " x $be . "100%|";
        return " " x $pad . "#" x $boxfull . "-" x $boxempty .
            " " x $boxnon . "\n" . " " x $pad . "$leg\n";
    } else {
        debug_out(1, "bargrahp called without proper parameters.");
        return "";
    }
}

sub human_readable {
    debug_out(15, "human_readable called with ", join(":", @_));
    if (2 == scalar(@_)) {
        debug_out(15, "2 Params - let's see what we've got.");
        my ($t, $v) = @_;
      SWITCH: for ($t) {
          /B/ && do {
              debug_out(15, "Working with Bytes.");
              my $d = 'B';
              if ($v > 1024) {
                  $v /= 1024;
                  $d = 'kB';
              }
              if ($v > 1024) {
                  $v /= 1024;
                  $d = 'MB';
              }
              if ($v > 1024) {
                  $v /= 1024;
                  $d = 'GB';
              }
              if ($v > 1024) {
                  $v /= 1024;
                  $d = 'TB';
              }
              return sprintf("%0.2f%s", $v, $d);
              last SWITCH;
          };
          /P/ && do {
              debug_out(15, "Working with Percent value.");
              my $ret = $v;
              if ($v =~ /^([+-]?)(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?/) {
                  if ($v >= 1000.0) {
                      $ret = " (def.)";
                      $v -= 1000.0;
                  } else {
                      $ret = "";
                  }
                  $ret = sprintf("%1.0f%%", $v) . $ret;
              }
              return $ret;
              last SWITCH;
          };
          return $v;
      }
    } else {
        return join("", @_);
    }
}

sub get_default_bytes {
    debug_out(15, "get_default_bytes called with ", join(":", @_));
    if (1 == scalar(@_)) {
        debug_out(15, "1 Param - let's see what we've got.");
      SWITCH: for (@_) {
          /DDS/ && return 2000000000;
          /DDS1/ && return 2000000000;
          /DDS2/ && return 4000000000;
          /DLTIV/ && return 20000000000;
          /DC6525/ && return 525000000;
          /File/ && return 128*1024*1024;
          {
              debug_out(0, "$_ is not a known Media Type. Assuming 1 kBytes");
              return 1024;
          };
      };
    } else {
        debug_out(0, "This is not right...");
        return 999;
    }
}

sub debug_out {
    if ($debug >= shift) {
        print "@_\n";
    }
}

sub debug_abort {
    debug_out(@_);
    do_closedb();
    exit 1;
}

sub do_closedb {
    my $t;
    debug_out(40, "Closing database connection...");
    while ($t=shift @temp_tables) {
        debug_out(40, "Now dropping table $t");
        $h_db->do("DROP TABLE $t") || debug_out(0, "Can't drop $t.");
    }
    $h_db->disconnect();
    debug_out(40, "Database disconnected.");
}

sub do_usage {
    print<<EOF;
$ME (C) 2005 Arno Lehmann, IT-Service Lehmann

produce bacula statistics to stdout

usage: $ME options      more help: perldoc $ME
Options can be abbreviated to uniqueness. Negating --option is done
like --nooption.
  --help     -h  print this help
  --version  -V  print version and license
  --host         database host, default unset (use unix socket)
  --user     -U  database user, default unset
  --database -D  database name, default "mysql:bacula"
                 (notice database driver!)
  --password -P  database password, default unset
  --debug        debug level, default 0. Higer level, more output
  General output control:
  --subpools        -s  no   subpool (Media types in each pool) details
  --subpool-details     no   more detailed information
  --pool-details    -d  no   detailed pool information
  --pool-bargraph       yes  show visual pool usage, can be negated
  --subpool-bargraph    no   show visual subpool usage
  --bar-length      -l  70   length for graphical pool usage display
  --cutmarks        -c  no   print cut marks above the pools
EOF
}

sub do_version {
    print<<EOF;
This is baculareport.pl called as $0
This program was created by Arno Lehmann (al\@its-lehmann.de) in
2005.

You have Version $version.

This program is copyrighted, but everybody is allowed to use, modify
and distribute this program under the following conditions:
- This license and the original copyright holder must not be changed
- The terms and the idea of the GPL apply. If you are unsure, ask
  the copyright holder if your planned usage is ok.
- No warranties, no promises. You are all on your own.
  This program needs access to your bacula catalog. If you don't like
  that idea, don't use it or check the sourcecode!

Although I give no warranties, in case of problems you can contact me.
I will help as good as possible.
Bacula consulting available.

Bacula is a Trademark of John Walker. See www.bacula.org

EOF

}
