################################################################
use strict;

=head1 LICENSE

   Bareos® - The Network Backup Solution

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.

   The main author of Bareos is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation plus additions
   that are listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bareos® is a registered trademark of Kern Sibbald.
   The licensor of Bareos is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zurich,
   Switzerland, email:ftf@fsfeurope.org.

=cut

package scripts::functions;
# Export all functions needed to be used by a simple
# perl -Mscripts::functions -e '' script
use Exporter;
our @ISA = qw(Exporter);

our @EXPORT = qw(update_some_files create_many_files check_multiple_copies
                  update_client $HOST $BASEPORT add_to_backup_list
                  check_volume_size create_many_dirs cleanup start_bareos
                  stop_bareos get_resource set_maximum_concurrent_jobs get_time
                  add_attribute check_prune_list check_min_volume_size
                  check_max_volume_size $estat $bstat $rstat $zstat $cwd $bin
                  $scripts $conf $rscripts $tmp $working $dstat extract_resource
                  $db_name $db_user $db_password $src $tmpsrc
                  remote_init remote_config remote_stop remote_diff );


use File::Copy qw/copy/;

our ($cwd, $bin, $scripts, $conf, $rscripts, $tmp, $working, $estat, $dstat,
     $bstat, $zstat, $rstat, $debug,
     $REMOTE_CLIENT, $REMOTE_ADDR, $REMOTE_FILE, $REMOTE_PORT, $REMOTE_PASSWORD,
     $REMOTE_STORE_ADDR, $REGRESS_DEBUG,
     $db_name, $db_user, $db_password, $src, $tmpsrc, $HOST, $BASEPORT);

END {
    if ($estat || $rstat || $zstat || $bstat || $dstat) {
        exit 1;
    }
}

BEGIN {
    # start by loading the ./config file
    my ($envar, $enval);
    if (! -f "./config") {
        die "Could not find ./config file\n";
    }
    # load the ./config file in a subshell doesn't allow to use "env" to display all variable
    open(IN, ". ./config; set |") or die "Could not run shell: $!\n";
    while ( my $l = <IN> ) {
        chomp ($l);
        if ($l =~ /^([\w\d]+)='?([^']+)'?/) {
            next if ($1 eq 'SHELLOPTS'); # is in read-only
            ($envar,$enval) = ($1, $2);
            $ENV{$envar} = $enval;
        }
    }
    close(IN);
    $cwd = `pwd`;
    chomp($cwd);

    # set internal variable name and update environment variable
    $ENV{db_name}     = $db_name     = $ENV{db_name}     || 'regress';
    $ENV{db_user}     = $db_user     = $ENV{db_user}     || 'regress';
    $ENV{db_password} = $db_password = $ENV{db_password} || '';

    $ENV{bin}      = $bin      =  $ENV{bin}      || "$cwd/bin";
    $ENV{tmp}      = $tmp      =  $ENV{tmp}      || "$cwd/tmp";
    $ENV{src}      = $src      =  $ENV{src}      || "$cwd/src";
    $ENV{conf}     = $conf     =  $ENV{conf}     || $bin;
    $ENV{scripts}  = $scripts  =  $ENV{scripts}  || $bin;
    $ENV{tmpsrc}   = $tmpsrc   =  $ENV{tmpsrc}   || "$cwd/tmp/build";
    $ENV{working}  = $working  =  $ENV{working}  || "$cwd/working";
    $ENV{rscripts} = $rscripts =  $ENV{rscripts} || "$cwd/scripts";
    $ENV{HOST}     = $HOST     =  $ENV{HOST}     || "localhost";
    $ENV{BASEPORT} = $BASEPORT =  $ENV{BASEPORT} || "8101";
    $ENV{REGRESS_DEBUG} = $debug         = $ENV{REGRESS_DEBUG} || 0;
    $ENV{REMOTE_CLIENT} = $REMOTE_CLIENT = $ENV{REMOTE_CLIENT} || 'remote-fd';
    $ENV{REMOTE_ADDR}   = $REMOTE_ADDR   = $ENV{REMOTE_ADDR}   || undef;
    $ENV{REMOTE_FILE}   = $REMOTE_FILE   = $ENV{REMOTE_FILE}   || "/tmp";
    $ENV{REMOTE_PORT}   = $REMOTE_PORT   = $ENV{REMOTE_PORT}   || 9102;
    $ENV{REMOTE_PASSWORD} = $REMOTE_PASSWORD = $ENV{REMOTE_PASSWORD} || "xxx";
    $ENV{REMOTE_STORE_ADDR}=$REMOTE_STORE_ADDR=$ENV{REMOTE_STORE_ADDR} || undef;

    $estat = $rstat = $bstat = $zstat = 0;
}

sub cleanup
{
    system("$rscripts/cleanup");
    return $? == 0;
}

sub start_bareos
{
    my $ret;
    $ENV{LANG}='C';
    system("$scripts/bareos start");
    $ret = $? == 0;
    open(FP, ">$tmp/bcmd");
    print FP "sql\ntruncate client_group;\ntruncate client_group_member;\nupdate Media set LocationId=0;\ntruncate location;\n\n";
    close(FP);
    system("cat $tmp/bcmd | $bin/bconsole -c $conf >/dev/null");
    return $ret;
}

sub stop_bareos
{
    $ENV{LANG}='C';
    system("$scripts/bareos stop");
    return $? == 0;
}

sub get_resource
{
    my ($file, $type, $name) = @_;
    my $ret;
    open(FP, $file) or die "Can't open $file";
    my $content = join("", <FP>);

    if ($content =~ m/(^$type \{[^\}]+?Name\s*=\s*"?$name"?[^\}]+?^\})/ms) {
        $ret = $1;
    }

    close(FP);
    return $ret;
}

sub extract_resource
{
    my $ret = get_resource(@_);
    if ($ret) {
        print $ret, "\n";
    }
}

sub check_min_volume_size
{
    my ($size, @vol) = @_;
    my $ret=0;

    foreach my $v (@vol) {
        if (! -f "$tmp/$v") {
            print "ERR: $tmp/$v not accessible\n";
            $ret++;
            next;
        }
        if (-s "$tmp/$v" < $size) {
            print "ERR: $tmp/$v too small\n";
            $ret++;
        }
    }
    $estat+=$ret;
    return $ret;
}

sub check_max_volume_size
{
    my ($size, @vol) = @_;
    my $ret=0;

    foreach my $v (@vol) {
        if (! -f "$tmp/$v") {
            print "ERR: $tmp/$v not accessible\n";
            $ret++;
            next;
        }
        if (-s "$tmp/$v" > $size) {
            print "ERR: $tmp/$v too big\n";
            $ret++;
        }
    }
    $estat+=$ret;
    return $ret;
}

sub add_to_backup_list
{
    open(FP, ">>$tmp/file-list") or die "Can't open $tmp/file-list for update $!";
    print FP join("\n", @_);
    close(FP);
}

# update client definition for the current test
# it permits to test remote client
sub update_client
{
    my ($new_passwd, $new_address, $new_port) = @_;
    my $in_client=0;

    open(FP, "$conf/bareos-dir.conf") or die "can't open source $!";
    open(NEW, ">$tmp/bareos-dir.conf.$$") or die "can't open dest $!";
    while (my $l = <FP>) {
        if (!$in_client && $l =~ /^Client \{/) {
            $in_client=1;
        }

        if ($in_client && $l =~ /Address/i) {
            $l = "Address = $new_address\n";
        }

        if ($in_client && $l =~ /FDPort/i) {
            $l = "FDPort = $new_port\n";
        }

        if ($in_client && $l =~ /Password/i) {
            $l = "Password = \"$new_passwd\"\n";
        }

        if ($in_client && $l =~ /^}/) {
            $in_client=0;
        }
        print NEW $l;
    }
    close(FP);
    close(NEW);
    my $ret = copy("$tmp/bareos-dir.conf.$$", "$conf/bareos-dir.conf");
    unlink("$tmp/bareos-dir.conf.$$");
    return $ret;
}

# open a directory and update all files
sub update_some_files
{
    my ($dest)=@_;
    my $t=rand();
    my $f;
    my $nb=0;
    print "Update files in $dest\n";
    opendir(DIR, $dest) || die "$!";
    map {
        $f = "$dest/$_";
        if (-f $f) {
            open(FP, ">$f") or die "$f $!";
            print FP "$t update $f\n";
            close(FP);
            $nb++;
        }
    } readdir(DIR);
    closedir DIR;
    print "$nb files updated\n";
}

# create big number of files in a given directory
# Inputs: dest  destination directory
#         nb    number of file to create
# Example:
# perl -Mscripts::functions -e 'create_many_files("$cwd/files", 100000)'
sub create_many_files
{
    my ($dest, $nb) = @_;
    my $base;
    my $dir=$dest;
    $nb = $nb || 750000;
    mkdir $dest;
    $base = chr($nb % 26 + 65); # We use a base directory A-Z

    # already done
    if (-f "$dest/$base/a${base}a${nb}aaa${base}") {
        print "Files already created\n";
        return;
    }

    # auto flush stdout for dots
    $| = 1;
    print "Create $nb files into $dest\n";
    for(my $i=0; $i < 26; $i++) {
        $base = chr($i + 65);
        mkdir("$dest/$base") if (! -d "$dest/$base");
    }
    for(my $i=0; $i<=$nb; $i++) {
        $base = chr($i % 26 + 65);
        open(FP, ">$dest/$base/a${base}a${i}aaa$base") or die "$dest/$base $!";
        print FP "$i\n";
        close(FP);

        open(FP, ">$dir/b${base}a${i}csq$base") or die "$dir $!";
        print FP "$base $i\n";
        close(FP);

        if (!($i % 100)) {
            $dir = "$dest/$base/$base$i$base";
            mkdir $dir;
        }
        print "." if (!($i % 10000));
    }
    print "\n";
}

# create big number of dirs in a given directory
# Inputs: dest  destination directory
#         nb    number of dirs to create
# Example:
# perl -Mscripts::functions -e 'create_many_dirs("$cwd/files", 100000)'
sub create_many_dirs
{
    my ($dest, $nb) = @_;
    my ($base, $base2);
    my $dir=$dest;
    $nb = $nb || 750000;
    mkdir $dest;
    $base = chr($nb % 26 + 65); # We use a base directory A-Z
    $base2 = chr(($nb+10) % 26 + 65);
    # already done
    if (-d "$dest/$base/$base2/$base/a${base}a${nb}aaa${base}") {
        print "Files already created\n";
        return;
    }

    # auto flush stdout for dots
    $| = 1;
    print "Create $nb dirs into $dest\n";
    for(my $i=0; $i < 26; $i++) {
        $base = chr($i + 65);
        $base2 = chr(($i+10) % 26 + 65);
        mkdir("$dest/$base");
        mkdir("$dest/$base/$base2");
        mkdir("$dest/$base/$base2/$base$base2");
        mkdir("$dest/$base/$base2/$base$base2/$base$base2");
        mkdir("$dest/$base/$base2/$base$base2/$base$base2/$base2$base");
    }
    for(my $i=0; $i<=$nb; $i++) {
        $base = chr($i % 26 + 65);
        $base2 = chr(($i+10) % 26 + 65);
        mkdir("$dest/$base/$base2/$base$base2/$base$base2/$base2$base/a${base}a${i}aaa$base");
        print "." if (!($i % 10000));
    }
    print "\n";
}

sub check_encoding
{
    if (grep {/Wanted SQL_ASCII, got UTF8/}
        `${bin}/bareos-dir -d50 -t -c ${conf}/bareos-dir.conf 2>&1`)
    {
        print "Found database encoding problem, please modify the ",
              "database encoding (SQL_ASCII)\n";
        exit 1;
    }
}

# You can change the maximum concurrent jobs for any config file
# If specified, you can change only one Resource or one type of
# resource at the time (optional)
#  set_maximum_concurrent_jobs('$conf/bareos-dir.conf', 100);
#  set_maximum_concurrent_jobs('$conf/bareos-dir.conf', 100, 'Director');
#  set_maximum_concurrent_jobs('$conf/bareos-dir.conf', 100, 'Device', 'Drive-0');
sub set_maximum_concurrent_jobs
{
    my ($file, $nb, $obj, $name) = @_;

    die "Can't get new maximumconcurrentjobs"
        unless ($nb);

    add_attribute($file, "Maximum Concurrent Jobs", $nb, $obj, $name);
}

# You can add option to a resource
#  add_attribute('$conf/bareos-dir.conf', 'FDTimeout', 1600, 'Director');
#  add_attribute('$conf/bareos-dir.conf', 'FDTimeout', 1600, 'Storage', 'FileStorage');
sub add_attribute
{
    my ($file, $attr, $value, $obj, $name) = @_;
    my ($cur_obj, $cur_name, $done);

    open(FP, ">$tmp/1.$$") or die "Can't write to $tmp/1.$$";
    open(SRC, $file) or die "Can't open $file";
    while (my $l = <SRC>)
    {
        if ($l =~ /^#/) {
            print FP $l;
            next;
        }

        if ($l =~ /^(\w+) \{/) {
            $cur_obj = $1;
            $done=0;
        }

        if ($l =~ /^\s*\Q$attr\E/i) {
            if (!$obj || $cur_obj eq $obj) {
                if (!$name || $cur_name eq $name) {
                    $l =~ s/\Q$attr\E\s*=\s*.+/$attr = $value/ig;
                    $done=1
                }
            }
        }

        if ($l =~ /^\s*Name\s*=\s*"?([\w\d\.-]+)"?/i) {
            $cur_name = $1;
        }

        if ($l =~ /^}/) {
            if (!$done) {
                if ($cur_obj eq $obj) {
                    if (!$name || $cur_name eq $name) {
                        $l = "  $attr = $value\n$l";
                    }
                }
            }
            $cur_name = $cur_obj = undef;
        }
        print FP $l;
    }
    close(SRC);
    close(FP);
    copy("$tmp/1.$$", $file) or die "Can't copy $tmp/1.$$ to $file";
}

# This test the list jobs output to check differences
# Input: read file argument
#        check if all jobids in argument are present in the first
#        'list jobs' and not present in the second
# Output: exit(1) if something goes wrong and print error
sub check_prune_list
{
    my $f = shift;
    my %to_check = map { $_ => 1} @_;
    my %seen;
    my $in_list_jobs=0;
    my $nb_list_job=0;
    my $nb = scalar(@_);
    open(FP, $f) or die "Can't open $f $!";
    while (my $l = <FP>)          # read all files to check
    {
        if ($l =~ /list jobs/) {
            $in_list_jobs=1;
            $nb_list_job++;

            if ($nb_list_job == 2) {
                foreach my $jobid (keys %to_check) {
                    if (!$seen{$jobid}) {
                        print "ERROR: in $f, can't find $jobid in first 'list jobs'\n";
                        exit 1;
                    }
                }
            }
            next;
        }
        if ($nb_list_job == 0) {
            next;
        }
        if ($l =~ /Pruned (\d+) Job for client/) {
            if ($1 != $nb) {
                print "ERROR: in $f, Prune command returns $1 jobs, want $nb\n";
                exit 1;
            }
        }

        if ($l =~ /No Jobs found to prune/) {
           if ($nb != 0) {
                print "ERROR: in $f, Prune command returns 0 job, want $nb\n";
                exit 1;
            }
        }

        # list jobs ouput:
        # | 1 | NightlySave | 2010-06-16 22:43:05 | B | F | 27 | 4173577 | T |
        if ($l =~ /^\|\s+(\d+)/) {
            if ($nb_list_job == 1) {
                $seen{$1}=1;
            } else {
                delete $seen{$1};
            }
        }
    }
    close(FP);
    foreach my $jobid (keys %to_check) {
        if (!$seen{$jobid}) {
            print "******* listing of $f *********\n";
            system("cat $f");
            print "******* end listing of $f *********\n";
            print "ERROR: in $f, $jobid is still present in the 2nd 'list jobs'\n";
            exit 1;
        }
    }
    if ($nb_list_job != 2) {
        print "ERROR: in $f, not enough 'list jobs'\n";
        exit 1;
    }
    exit 0;
}

# This test ensure that 'list copies' displays only each copy one time
#
# Input: read stream from stdin or with file list argument
#        check the number of copies with the ARGV[1]
# Output: exit(1) if something goes wrong and print error
sub check_multiple_copies
{
    my ($nb_to_found) = @_;

    my $in_list_copies=0;       # are we or not in a list copies block
    my $nb_found=0;             # count the number of copies found
    my $ret = 0;
    my %seen;

    while (my $l = <>)          # read all files to check
    {
        if ($l =~ /list copies/) {
            $in_list_copies=1;
            %seen = ();
            next;
        }

        # not in a list copies anymore
        if ($in_list_copies && $l =~ /^ /) {
            $in_list_copies=0;
            next;
        }

        # list copies ouput:
        # |     3 | Backup.2009-09-28 |  9 | DiskChangerMedia |
        if ($in_list_copies && $l =~ /^\|\s+\d+/) {
            my (undef, $jobid, undef, $copyid, undef) = split(/\s*\|\s*/, $l);
            if (exists $seen{$jobid}) {
                print "ERROR: $jobid/$copyid already known as $seen{$jobid}\n";
                $ret = 1;
            } else {
                $seen{$jobid}=$copyid;
                $nb_found++;
            }
        }
    }

    # test the number of copies against the given arg
    if ($nb_to_found && ($nb_to_found != $nb_found)) {
        print "ERROR: Found wrong number of copies ",
              "($nb_to_found != $nb_found)\n";
        exit 1;
    }

    exit $ret;
}

use POSIX qw/strftime/;
sub get_time
{
    my ($sec) = @_;
    print strftime('%F %T', localtime(time+$sec)), "\n";
}

sub debug
{
    if ($debug) {
        print join("\n", @_), "\n";
    }
}

sub remote_config
{
    open(FP, ">$REMOTE_FILE/bareos-fd.conf") or
        die "ERROR: Can't open $REMOTE_FILE/bareos-fd.conf $!";
    print FP "
Director {
  Name = $HOST-dir
  Password = \"$REMOTE_PASSWORD\"
}
FileDaemon {
  Name = remote-fd
  FDport = $REMOTE_PORT
  WorkingDirectory = $REMOTE_FILE/working
  Pid Directory = $REMOTE_FILE/working
  Maximum Concurrent Jobs = 20
}
Messages {
  Name = Standard
  director = $HOST-dir = all, !skipped, !restored
}
";
    close(FP);
    system("mkdir -p '$REMOTE_FILE/working' '$REMOTE_FILE/save'");
    system("rm -rf '$REMOTE_FILE/restore'");
    my $pid = fork();
    if (!$pid) {
        close(STDIN);  open(STDIN, "/dev/null");
        close(STDOUT); open(STDOUT, ">/dev/null");
        close(STDERR); open(STDERR, ">/dev/null");
        exec("/opt/bareos/bin/bareos-fd -c $REMOTE_FILE/bareos-fd.conf");
        exit 1;
    }
    sleep(2);
    $pid = `cat $REMOTE_FILE/working/bareos-fd.$REMOTE_PORT.pid`;
    chomp($pid);

    # create files and tweak rights
    create_many_files("$REMOTE_FILE/save", 5000);
    chdir("$REMOTE_FILE/save");
    my $d = 'A';
    my $r = 0700;
    for my $g ( split(' ', $( )) {
        chmod $r++, $d;
        chown $<, $g, $d++;
    }

    # create a simple script to execute
    open(FP, ">test.sh") or die "Can't open test.sh $!";
    print FP "#!/bin/sh\n";
    print FP "echo this is a script";
    close(FP);
    chmod 0755, "test.sh";

    # create a hardlink
    link("test.sh", "link-test.sh");

    # create long filename
    mkdir("b" x 255) or print "can't create long dir $!\n";
    copy("test.sh", ("b" x 255) . '/' . ("a" x 255)) or print "can't create long dir $!\n";

    # play with some symlinks
    symlink("test.sh", "sym-test.sh");
    symlink("$REMOTE_FILE/save/test.sh", "sym-abs-test.sh");

    if ($pid) {
        system("ps $pid");
        $estat = ($? != 0);
    } else {
        $estat = 1;
    }
}

sub remote_diff
{
    debug("Doing diff between save and restore");
    system("ssh $REMOTE_ADDR " .
     "$REMOTE_FILE/scripts/diff.pl -s $REMOTE_FILE/save -d $REMOTE_FILE/restore/$REMOTE_FILE/save");
    $dstat = ($? != 0);
}

sub remote_stop
{
    debug("Kill remote bareos-fd");
    system("ssh $REMOTE_ADDR " .
             "'test -f $REMOTE_FILE/working/bareos-fd.$REMOTE_PORT.pid && " .
              "kill `cat $REMOTE_FILE/working/bareos-fd.$REMOTE_PORT.pid`'");
}

sub remote_init
{
    system("ssh $REMOTE_ADDR mkdir -p '$REMOTE_FILE/scripts/'");
    system("scp -q scripts/functions.pm scripts/diff.pl $REMOTE_ADDR:$REMOTE_FILE/scripts/");
    system("scp -q config $REMOTE_ADDR:$REMOTE_FILE/");
    debug("INFO: Configuring remote client");
    system("ssh $REMOTE_ADDR 'cd $REMOTE_FILE && perl -Mscripts::functions -e remote_config'");
}
1;
