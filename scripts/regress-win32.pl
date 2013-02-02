#!/usr/bin/perl -w

=head1 NAME

    regress-win32.pl -- Helper for Windows regression tests

=head2 DESCRIPTION

    This perl script permits to run test Bareos Client Daemon on Windows.
    It allows to:
       - stop/start/upgrade the Bareos Client Daemon
       - compare to subtree with checksums, attribs and ACL
       - create test environments

=head2 USAGE

  X:\> regress-win32.pl [-b basedir] [-i ip_address] [-p c:/bareos]
   or
  X:\> perl regress-win32.pl ...

    -b|--base=path      Where to find regress and bareos directories
    -i|--ip=ip          Restrict access to this tool to this ip address
    -p|--prefix=path    Path to the windows installation
    -h|--help           Print this help

=head2 EXAMPLE

    regress-win32.pl -b z:/git         # will find z:/git/regress z:/git/bareos

    regress-win32.pl -i 192.168.0.1 -b z:

=head2 INSTALL

    This perl script needs a Perl distribution on the Windows Client
    (http://strawberryperl.com)

    You need to have the following subtree on x:
    x:/
      bareos/
      regress/

   This script requires perl to work (http://strawberryperl.com), and by default
   it assumes that Bareos is installed in the standard location. Once it's
   started on the windows, you can do remote commands like:
    - start the service
    - stop the service
    - edit the bareos-fd.conf to change the director and password setting
    - install a new binary version (not tested, no plugin support)
    - create weird files and directories
    - create files with windows attributes
    - compare two directories (with md5)


   To test it, you can follow this procedure
   On the windows box:
    - install perl from http://strawberryperl.com on windows
    - copy or export regress directory somewhere on your windows
      You can use a network share to your regress directory on linux
      Then, copy a link to this script to your desktop
      And double-click on it, and always open .pl file with perl.exe

    - If you export the regress directory to your windows box and you
      make windows binaries available, this script can update bareos version.
      You need to put your binaries on:
        regress/release32 and regress/release64
      or
        regress/build/src/win32/release32 and regress/build/src/win32/release64

    - start the regress/scripts/regress-win32.pl (open it with perl.exe)
    - create $WIN32_FILE
    - make sure that the firewall is well configured or just disabled (needs
   bareos and 8091/tcp)

   On Linux box:
    - edit config file to fill the following variables

   WIN32_CLIENT="win2008-fd"
   # Client FQDN or IP address
   WIN32_ADDR="192.168.0.6"
   # File or Directory to backup.  This is put in the "File" directive
   #   in the FileSet
   WIN32_FILE="c:/tmp"
   # Port of Win32 client
   WIN32_PORT=9102
   # Win32 Client password
   WIN32_PASSWORD="xxx"
   # will be the ip address of the linux box
   WIN32_STORE_ADDR="192.168.0.1"
   # set for autologon
   WIN32_USER=Administrator
   WIN32_PASS=password
   # set for MSSQL
   WIN32_MSSQL_USER=sa
   WIN32_MSSQL_PASS=pass
    - type make setup
    - run ./tests/backup-bareos-test to be sure that everything is ok
    - start ./tests/win32-fd-test

   I'm not very happy with this script, but it works :)

=cut

use strict;
use HTTP::Daemon;
use HTTP::Status;
use HTTP::Response;
use HTTP::Headers;
use File::Copy;
use Pod::Usage;
use Cwd 'chdir';
use File::Find;
use Digest::MD5;
use Getopt::Long ;
use POSIX;
use File::Basename qw/dirname/;

my $base = 'x:';
my $src_ip = '';
my $help;
my $bareos_prefix="c:/Program Files/Bareos";
my $conf = "C:/Documents and Settings/All Users/Application Data/Bareos";
GetOptions("base=s"   => \$base,
           "help"     => \$help,
           "prefix=s" => \$bareos_prefix,
           "ip=s"     => \$src_ip);

if ($help) {
    pod2usage(-verbose => 2,
              -exitval => 0);
}

if (! -d $bareos_prefix) {
    print "regress-win32.pl: Could not find Bareos installation dir $bareos_prefix\n";
    print "regress-win32.pl: Won't be able to upgrade the version or modify the configuration\n";
}

if (-f "$bareos_prefix/bareos-fd.conf" and -f "$conf/bareos-fd.conf") {
    print "regress-win32.pl: Unable to determine bareos-fd location $bareos_prefix or $conf ?\n";

} elsif (-f "$bareos_prefix/bareos-fd.conf") {
    $conf = $bareos_prefix;
}

#if (! -d "$base/bareos" || ! -d "$base/regress") {
#    pod2usage(-verbose => 2,
#              -exitval => 1,
#              -message => "Can't find bareos or regress dir on $base\n");
#}

# stop the fd service
sub stop_fd
{
    return `net stop bareos-fd`;
}

my $arch;
my $bin_path;
sub find_binaries
{
    if ($_ =~ /bareos-fd.exe/i) {
        if ($File::Find::dir =~ /release$arch/) {
            $bin_path = $File::Find::dir;
        }
    }
}

# copy binaries for a new fd
# to work, you need to mount the regress directory
sub install_fd
{
    my ($r) = shift;
    if ($r->url !~ m!^/install$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }

    if (-d "c:/Program Files (x86)") {
        $arch = "64";
    } else {
        $arch = "32";
    }

    # X:/regress/scripts/regress-win32.pl
    # X:/scripts/regress-win32.pl
    # perl script location

    my $dir = dirname(dirname($0));
    print "searching bareos-fd.exe in $dir\n";
    find(\&find_binaries, ("$dir\\build\\src\\win32\\release$arch",
                           "$dir\\release$arch"));

    if (!$bin_path) {
        return "ERR\nCan't find bareos-fd.exe in $dir\n";
    }

    print "Found binaries in $bin_path\n";

    stop_fd();

    system("del \"c:\\Program Files\\bareos\\bareos.dll\"");
    system("del \"c:\\Program Files\\bareos\\bareos-fd.exe\"");
    system("del \"c:\\Program Files\\bareos\\plugins\\vss-fd.dll\"");

    my $ret="Ok\n";

    copy("$bin_path/bareos-fd.exe",
         "c:/Program Files/bareos/bareos-fd.exe") or $ret="ERR\n$!\n";

    copy("$bin_path/bareos.dll",
         "c:/Program Files/bareos/bareos.dll") or $ret="ERR\n$!\n";

    copy("$bin_path/vss-fd.dll",
         "c:/Program Files/bareos/plugins/vss-fd.dll") or $ret="ERR\n$!\n";

    start_fd();
    return "OK\n";
}

# start the fd service
sub start_fd
{
    return `net start bareos-fd`;
}

# initialize the weird directory for runscript test
sub init_weird_runscript_test
{
    my ($r) = shift;

    if ($r->url !~ m!^/init_weird_runscript_test\?source=(\w:/[\w\d\-\./]+)$!) {
        return "ERR\nIncorrect url: ". $r->url . "\n";
    }
    my $source = $1;

    # Create $source if needed
    my $tmp = $source;
    $tmp =~ s:/:\\:g;
    system("mkdir $tmp");

    if (!chdir($source)) {
        return "ERR\nCan't access to $source $!\n";
    }

    if (-d "weird_runscript") {
        system("rmdir /Q /S weird_runscript");
    }

    mkdir("weird_runscript");
    if (!chdir("weird_runscript")) {
        return "ERR\nCan't access to $source $!\n";
    }

    open(FP, ">test.bat")                 or return "ERR\n";
    print FP "\@echo off\n";
    print FP "echo hello \%1\n";
    close(FP);

    copy("test.bat", "test space.bat")    or return "ERR\n";
    copy("test.bat", "test2 space.bat")   or return "ERR\n";
    copy("test.bat", "testé.bat")         or return "ERR\n";

    mkdir("dir space")                    or return "ERR\n";
    copy("test.bat", "dir space")         or return "ERR\n";
    copy("testé.bat","dir space")         or return "ERR\n";
    copy("test2 space.bat", "dir space")  or return "ERR\n";

    mkdir("Évoilà")                       or return "ERR\n";
    copy("test.bat", "Évoilà")            or return "ERR\n";
    copy("testé.bat","Évoilà")            or return "ERR\n";
    copy("test2 space.bat", "Évoilà")     or return "ERR\n";

    mkdir("Éwith space")                  or return "ERR\n";
    copy("test.bat", "Éwith space")       or return "ERR\n";
    copy("testé.bat","Éwith space")       or return "ERR\n";
    copy("test2 space.bat", "Éwith space") or return "ERR\n";
    mkdir("a"x200);
    copy("test.bat", "a"x200);
    system("mklink /J junc " . "a"x200); # TODO: need something for win2003
    link("test.bat", "link.bat");
    return "OK\n";
}

# init the Attrib test by creating some files and settings attributes
sub init_attrib_test
{
    my ($r) = shift;

    if ($r->url !~ m!^/init_attrib_test\?source=(\w:/[\w/]+)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }

    my $source = $1;
    system("mkdir $source");

    if (!chdir($source)) {
        return "ERR\nCan't access to $source $!\n";
    }

    # cleanup the old directory if any
    if (-d "attrib_test") {
        system("rmdir /Q /S attrib_test");
    }

    mkdir("attrib_test");
    chdir("attrib_test");

    mkdir("hidden");
    mkdir("hidden/something");
    system("attrib +H hidden");

    mkdir("readonly");
    mkdir("readonly/something");
    system("attrib +R readonly");

    mkdir("normal");
    mkdir("normal/something");
    system("attrib -R -H -S normal");

    mkdir("system");
    mkdir("system/something");
    system("attrib +S system");

    mkdir("readonly_hidden");
    mkdir("readonly_hidden/something");
    system("attrib +R +H readonly_hidden");

    my $ret = `attrib /S /D`;
    $ret = strip_base($ret, $source);

    return "OK\n$ret\n";
}

sub md5sum
{
    my $file = shift;
    open(FILE, $file) or return "Can't open $file $!";
    binmode(FILE);
    return Digest::MD5->new->addfile(*FILE)->hexdigest;
}

# set $src and $dst before using Find call
my ($src, $dst);
my $error="";
sub wanted
{
    my $f = $File::Find::name;
    $f =~ s!^\Q$src\E/?!!i;

    if (-f "$src/$f") {
        if (! -f "$dst/$f") {
            $error .= "$dst/$f is missing\n";
        } else {
            my $a = md5sum("$src/$f");
            my $b = md5sum("$dst/$f");
            if ($a ne $b) {
                $error .= "$src/$f $a\n$dst/$f $b\n";
            }
        }
    }
}

sub create_schedtask
{
    my ($r) = shift;
    if ($r->url !~ m!^/create_schedtask\?name=([\w\d\-.]+)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }
    my $ret='';
    my ($task,$pass) = ($1, $2);
    my (undef, undef, $version, undef) = POSIX::uname();
    if ($version < 6) {         # win2003
        $ret = `echo pass | SCHTASKS /Create /TN $task /SC ONLOGON  /TR C:\\windows\\system32\\calc.exe /F 2>&1`;
    } else {
        $ret=`SCHTASKS /Create /TN $task /SC ONLOGON /F /TR C:\\windows\\system32\\calc.exe`;
    }

    if ($ret =~ /SUCCESS|has been created/) {
        return "OK\n$ret";
    } else {
        return "ERR\n$ret";
    }
#
# SCHTASKS /Create [/S system [/U username [/P [password]]]]
#     [/RU username [/RP password]] /SC schedule [/MO modifier] [/D day]
#     [/M months] [/I idletime] /TN taskname /TR taskrun [/ST starttime]
#     [/RI interval] [ {/ET endtime | /DU duration} [/K] [/XML xmlfile] [/V1]]
#     [/SD startdate] [/ED enddate] [/IT | /NP] [/Z] [/F]
}

sub del_schedtask
{
    my ($r) = shift;
    if ($r->url !~ m!^/del_schedtask\?name=([\w\d\-.]+)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }
    my ($task) = ($1);
    my $ret=`SCHTASKS /Delete /TN $task /F`;

    if ($ret =~ /SUCCESS/) {
        return "OK\n$ret";
    } else {
        return "ERR\n$ret";
    }
}

sub check_schedtask
{
    my ($r) = shift;
    if ($r->url !~ m!^/check_schedtask\?name=([\w\d\-.]+)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }

    my ($task) = ($1);
    my (undef, undef, $version, undef) = POSIX::uname();
    my $ret;
    if ($version < 6) {         # win2003
        $ret=`SCHTASKS /Query`;
    } else {
        $ret=`SCHTASKS /Query /TN $task`;
    }

    if ($ret =~ /^($task .+)$/m) {
        return "OK\n$1\n";
    } else {
        return "ERR\n$ret";
    }
}

sub set_director_name
{
    my ($r) = shift;

    if ($r->url !~ m!^/set_director_name\?name=([\w\d\.\-]+);pass=([\w\d+\-\.*]+)$!)
    {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }

    my ($name, $pass) = ($1, $2);

    open(ORG, "$conf/bareos-fd.conf") or return "ERR\nORG $!\n";
    open(NEW, ">$conf/bareos-fd.conf.new") or return "ERR\nNEW $!\n";

    my $in_dir=0;               # don't use monitoring section
    my $nb_dir="";
    while (my $l = <ORG>)
    {
        if ($l =~ /^\s*Director\s+{/i) {
            print NEW $l;
            $in_dir = 1;
        } elsif ($l =~ /^(\s*)Name\s*=/ and $in_dir) {
            print NEW "${1}Name=$name$nb_dir\n";
        } elsif ($l =~ /^(\s*)Password\s*=/ and $in_dir) {
            print NEW "${1}Password=$pass\n";
        } elsif ($l =~ /#(\s*Plugin.*)$/) {
            print NEW $1;
        } elsif ($l =~ /\s*}/ and $in_dir) {
            print NEW $l;
            $in_dir = 0;
            $nb_dir++;
        } else {
            print NEW $l;
        }
    }

    close(ORG);
    close(NEW);
    move("$conf/bareos-fd.conf.new", "$conf/bareos-fd.conf")
        and return "OK\n";

    return "ERR\nCan't set the director name\n";
}

# convert \ to / and strip the path
sub strip_base
{
    my ($data, $path) = @_;
    $data =~ s!\\!/!sg;
    $data =~ s!\Q$path!!sig;
    return $data;
}

# Compare two directories, make checksums, compare attribs and ACLs
sub compare
{
    my ($r) = shift;

    if ($r->url !~ m!^/compare\?source=(\w:/[\w/]+);dest=(\w:/[\w/]+)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }

    my ($source, $dest) = ($1, $2);

    if (!Cwd::chdir($source)) {
        return "ERR\nCan't access to $source $!\n";
    }

    my $src_attrib = `attrib /D /S`;
    $src_attrib = strip_base($src_attrib, $source);

    if (!Cwd::chdir($dest)) {
        return "ERR\nCan't access to $dest $!\n";
    }

    my $dest_attrib = `attrib /D /S`;
    $dest_attrib = strip_base($dest_attrib, $dest);

    if (lc($src_attrib) ne lc($dest_attrib)) {
        print "ERR\n$src_attrib\n=========\n$dest_attrib\n";
        return "ERR\n$src_attrib\n=========\n$dest_attrib\n";
    }

    ($src, $dst, $error) = ($source, $dest, '');
    find(\&wanted, $source);
    if ($error) {
        return "ERR\n$error";
    } else {
        return "OK\n";
    }
}

sub cleandir
{
    my ($r) = shift;

    if ($r->url !~ m!^/cleandir\?source=(\w:/[\w/]+)/restore$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }

    my $source = $1;

    if (! -d "$source/restore") {
        return "ERR\nIncorrect path\n";
    }

    if (!chdir($source)) {
        return "ERR\nCan't access to $source $!\n";
    }

    system("rmdir /Q /S restore");

    return "OK\n";
}

sub reboot
{
    Win32::InitiateSystemShutdown('', "\nSystem will now Reboot\!", 2, 0, 1 );
    exit 0;
}

# boot disabled auto
sub set_service
{
    my ($r) = shift;

    if ($r->url !~ m!^/set_service\?srv=([\w-]+);action=(\w+)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }
    my $out = `sc config $1 start= $2`;
    if ($out !~ /SUCCESS/) {
        return "ERR\n$out";
    }
    return "OK\n";
}

# RUNNING, STOPPED
sub get_service
{
    my ($r) = shift;

    if ($r->url !~ m!^/get_service\?srv=([\w-]+);state=(\w+)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }
    my $out = `sc query $1`;
    if ($out !~ /$2/) {
        return "ERR\n$out";
    }
    return "OK\n";
}

sub add_registry_key
{
    my ($r) = shift;
    if ($r->url !~ m!^/add_registry_key\?key=(\w+);val=(\w+)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }
    my ($k, $v) = ($1,$2);
    my $ret = "ERR";
    open(FP, ">tmp.reg")
        or return "ERR\nCan't open tmp.reg $!\n";

    print FP "Windows Registry Editor Version 5.00

[HKEY_LOCAL_MACHINE\\SOFTWARE\\Bareos]
\"$k\"=\"$v\"

";
    close(FP);
    system("regedit /s tmp.reg");

    unlink("tmp2.reg");
    system("regedit /e tmp2.reg HKEY_LOCAL_MACHINE\\SOFTWARE\\Bareos");

    open(FP, "<:encoding(UTF-16LE)", "tmp2.reg")
       or return "ERR\nCan't open tmp2.reg $!\n";
    while (my $l = <FP>) {
       if ($l =~ /"$k"="$v"/) {
          $ret = "OK";
       }
    }
    close(FP);
    unlink("tmp.reg");
    unlink("tmp2.reg");
    return "$ret\n";
}

sub set_auto_logon
{
    my ($r) = shift;
    my $self = $0;              # perl script location
    $self =~ s/\\/\\\\/g;
    my $p = $^X;                # perl.exe location
    $p =~ s/\\/\\\\/g;
    if ($r->url !~ m!^/set_auto_logon\?user=([\w\d\-+\.]+);pass=([\w\d\.\,:*+%\-]*)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }
    my $k = $1;
    my $v = $2 || '';           # password can be empty
    my $ret = "ERR\nCan't find AutoAdminLogon key\n";
    open(FP, ">c:/autologon.reg")
        or return "ERR\nCan't open autologon.reg $!\n";
    print FP "Windows Registry Editor Version 5.00

[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon]
\"DefaultUserName\"=\"$k\"
\"DefaultPassword\"=\"$v\"
\"AutoAdminLogon\"=\"1\"

[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run]
\"regress\"=\"$p $self\"

[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Reliability]
\"ShutdownReasonUI\"=dword:00000000

[HKEY_LOCAL_MACHINE\\SOFTWARE\\Policies\\Microsoft\\Windows NT\\Reliability]
\"ShutdownReasonOn\"=dword:00000000
";
    close(FP);
    system("regedit /s c:\\autologon.reg");

    unlink("tmp2.reg");
    system("regedit /e tmp2.reg \"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon\"");

    open(FP, "<:encoding(UTF-16LE)", "tmp2.reg")
       or return "ERR\nCan't open tmp2.reg $!\n";
    while (my $l = <FP>) {
       if ($l =~ /"AutoAdminLogon"="1"/) {
          $ret = "OK\n";
       }
    }
    close(FP);
    unlink("tmp2.reg");
    return $ret;
}

sub del_registry_key
{
    my ($r) = shift;
    if ($r->url !~ m!^/del_registry_key\?key=(\w+)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }
    my $k = $1;
    my $ret = "OK\n";

    unlink("tmp2.reg");
    open(FP, ">tmp.reg")
        or return "ERR\nCan't open tmp.reg $!\n";
    print FP "Windows Registry Editor Version 5.00

[HKEY_LOCAL_MACHINE\\SOFTWARE\\Bareos]
\"$k\"=-

";
    close(FP);
    system("regedit /s tmp.reg");
    system("regedit /e tmp2.reg HKEY_LOCAL_MACHINE\\SOFTWARE\\Bareos");

    open(FP, "<:encoding(UTF-16LE)", "tmp2.reg")
       or return "ERR\nCan't open tmp2.reg $!\n";
    while (my $l = <FP>) {
       if ($l =~ /"$k"=/) {
          $ret = "ERR\nThe key $k is still present\n";
       }
    }
    close(FP);
    unlink("tmp.reg");
    unlink("tmp2.reg");
    return $ret;
}

sub get_registry_key
{
    my ($r) = shift;
    if ($r->url !~ m!^/get_registry_key\?key=(\w+);val=(\w+)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }
    my ($k, $v) = ($1, $2);
    my $ret = "ERR\nCan't get or verify registry key $k\n";

    unlink("tmp2.reg");
    system("regedit /e tmp2.reg HKEY_LOCAL_MACHINE\\SOFTWARE\\Bareos");
    open(FP, "<:encoding(UTF-16LE)", "tmp2.reg")
       or return "ERR\nCan't open tmp2.reg $!\n";
    while (my $l = <FP>) {
       if ($l =~ /"$k"="$v"/) {
          $ret = "OK\n";
       }
    }
    close(FP);
    unlink("tmp2.reg");

    return $ret;
}

my $mssql_user;
my $mssql_pass;
my $mssql_cred;
my $mssql_bin;
sub find_mssql
{
    if ($_ =~ /sqlcmd.exe/i) {
        $mssql_bin = $File::Find::name;
    }
}

# Verify that we can use SQLCMD.exe
sub check_mssql
{
    my ($r) = shift;
    my $ret = "ERR";
    if ($r->url !~ m!^/check_mssql\?user=(\w*);pass=(.*)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }
    ($mssql_user, $mssql_pass) = ($1, $2);

    unless ($mssql_bin) {
        find(\&find_mssql, 'c:/program files/microsoft sql server/');
    }
    unless ($mssql_bin) {
        find(\&find_mssql, 'c:/program files (x86)/microsoft sql server/');
    }

    if (!$mssql_bin) {
        return "ERR\nCan't find SQLCMD.exe in c:/program files\n";
    }

    print $mssql_bin, "\n";
    $mssql_cred = ($mssql_user)?"-U $mssql_user -P $mssql_pass":"";
    my $res = `"$mssql_bin" $mssql_cred -Q "SELECT 'OK';"`;
    if ($res !~ /OK/) {
        return "ERR\nCan't verify the SQLCMD result\n" .
            "Please verify that MSSQL is accepting connection:\n" .
            "$mssql_bin $mssql_cred -Q \"SELECT 1;\"\n";
    }
    return "OK\n";
}

# Create simple DB, a table and some information in
sub setup_mssql_db
{
    my ($r) = shift;
    my $ret = "ERR";
    if ($r->url !~ m!^/setup_mssql_db\?db=([\w\d]+)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }
    my $db = $1;

    unless ($mssql_bin) {
        return "ERR\nCan't find mssql bin (run check_mssql first)\n";
    }

    my $res = `"$mssql_bin" $mssql_cred -Q "CREATE DATABASE $db;"`;
    $res = `"$mssql_bin" $mssql_cred -d $db -Q "CREATE TABLE table1 (a int, b int);"`;
    $res = `"$mssql_bin" $mssql_cred -d $db -Q "INSERT INTO table1 (a, b) VALUES (1,1);"`;
    $res = `"$mssql_bin" $mssql_cred -d $db -Q "SELECT 'OK' FROM table1;"`;

    if ($res !~ /OK/) {
        return "ERR\nCan't verify the SQLCMD result\n" .
            "Please verify that MSSQL is accepting connection:\n" .
            "$mssql_bin $mssql_cred -Q \"SELECT 1;\"\n";
    }
    return "OK\n";
}

# drop database
sub cleanup_mssql_db
{
    my ($r) = shift;
    my $ret = "ERR";
    if ($r->url !~ m!^/cleanup_mssql_db\?db=([\w\d]+)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }
    my $db = $1;

    unless ($mssql_bin) {
        return "ERR\nCan't find mssql bin\n";
    }

    my $res = `"$mssql_bin" $mssql_cred -Q "DROP DATABASE $db;"`;

    return "OK\n";
}

# truncate the table that is in database
sub truncate_mssql_table
{
    my ($r) = shift;
    my $ret = "ERR";
    if ($r->url !~ m!^/truncate_mssql_table\?db=([\w\d]+)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }
    my $db = $1;

    unless ($mssql_bin) {
        return "ERR\nCan't find mssql bin\n";
    }

    my $res = `"$mssql_bin" $mssql_cred -d $db -Q "TRUNCATE TABLE table1;"`;
    $res = `"$mssql_bin" $mssql_cred -d $db -Q "SELECT 'OK' FROM table1;"`;

    if ($res =~ /OK/) {
        return "ERR\nCan't truncate $db.table1\n";
    }
    return "OK\n";
}

# test that table1 contains some rows
sub test_mssql_content
{
    my ($r) = shift;
    my $ret = "ERR";
    if ($r->url !~ m!^/test_mssql_content\?db=([\w\d]+)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }
    my $db = $1;

    unless ($mssql_bin) {
        return "ERR\nCan't find mssql bin\n";
    }

    my $res = `"$mssql_bin" $mssql_cred -d $db -Q "SELECT 'OK' FROM table1;"`;

    if ($res !~ /OK/) {
        return "ERR\nNo content from $mssql_bin\n$res\n";
    }
    return "OK\n";
}

my $mssql_mdf;
my $mdf_to_find;
sub find_mdf
{
    if ($_ =~ /$mdf_to_find/i) {
        $mssql_mdf = $File::Find::dir;
    }
}

# put a mdf online
sub online_mssql_db
{
    my ($r) = shift;
    if ($r->url !~ m!^/online_mssql_db\?mdf=([\w\d]+);db=([\w\d]+)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }
    my ($mdf, $db) = ($1, $2);
    $mdf_to_find = "$mdf.mdf";

    find(\&find_mdf, 'c:/program files/microsoft sql server/');
    unless ($mssql_mdf) {
        find(\&find_mssql, 'c:/program files (x86)/microsoft sql server/');
    }
    unless ($mssql_mdf) {
        return "ERR\nCan't find $mdf.mdf in c:/program files\n";
    }
    $mssql_mdf =~ s:/:\\:g;

    open(FP, ">c:/mssql.sql");
    print FP "
USE [master]
GO
CREATE DATABASE [$db] ON
( FILENAME = N'$mssql_mdf\\$mdf.mdf' ),
( FILENAME = N'$mssql_mdf\\${mdf}_log.LDF' )
 FOR ATTACH
GO
USE [$db]
GO
SELECT 'OK' FROM table1
GO
";
    close(FP);
    my $res = `"$mssql_bin" $mssql_cred -i c:\\mssql.sql`;
    #unlink("c:/mssql.sql");
    if ($res !~ /OK/) {
        return "ERR\nNo content from $mssql_bin\n";
    }
    return "OK\n";
}

# create a script c:/del.cmd to delete protected files with runscript
sub remove_dir
{
    my ($r) = shift;
    if ($r->url !~ m!^/remove_dir\?file=([\w\d:\/\.\-+*]+);dest=([\w\d\.:\/]+)$!) {
        return "ERR\nIncorrect url: " . $r->url . "\n";
    }
    my ($file, $cmd) = ($1, $2);
    $file =~ s:/:\\:g;

    open(FP, ">$cmd") or return "ERR\nCan't open $file $!\n";
    print FP "DEL /F /S /Q $file\n";
    close(FP);
    return "OK\n";
}

sub get_traces
{
    my ($file) = <"c:/program files/bareos/working/*.trace">;
    if (!$file || ! -f $file) {
        return "ERR\n$!\n";
    }
    return $file;
}

sub truncate_traces
{
    my $f = get_traces();
    unlink($f) or return "ERR\n$!\n";
    return "OK\n";
}

# When adding an action, fill this hash with the right function
my %action_list = (
    nop     => sub { return "OK\n"; },
    stop    => \&stop_fd,
    start   => \&start_fd,
    install => \&install_fd,
    compare => \&compare,
    init_attrib_test => \&init_attrib_test,
    init_weird_runscript_test => \&init_weird_runscript_test,
    set_director_name => \&set_director_name,
    cleandir => \&cleandir,
    add_registry_key => \&add_registry_key,
    del_registry_key => \&del_registry_key,
    get_registry_key => \&get_registry_key,
    quit => sub {  exit 0; },
    reboot => \&reboot,
    set_service => \&set_service,
    get_service => \&get_service,
    set_auto_logon => \&set_auto_logon,
    remove_dir => \&remove_dir,
    reload => \&reload,
    create_schedtask => \&create_schedtask,
    del_schedtask => \&del_schedtask,
    check_schedtask => \&check_schedtask,
    get_traces => \&get_traces,
    truncate_traces => \&truncate_traces,

    check_mssql => \&check_mssql,
    setup_mssql_db => \&setup_mssql_db,
    cleanup_mssql_db => \&cleanup_mssql_db,
    truncate_mssql_table => \&truncate_mssql_table,
    test_mssql_content => \&test_mssql_content,
    online_mssql_db => \&online_mssql_db,
    );

my $reload=0;
sub reload
{
    $reload=1;
    return "OK\n";
}

# handle client request
sub handle_client
{
    my ($c, $ip) = @_ ;
    my $action;
    my $r = $c->get_request ;

    if (!$r) {
        $c->send_error(RC_FORBIDDEN) ;
        return;
    }
    if ($r->url->path !~ m!^/(\w+)!) {
        $c->send_error(RC_NOT_FOUND) ;
        return;
    }
    $action = $1;

    if (($r->method eq 'GET')
        and $action_list{$action})
    {
        print "Exec $action:\n";

        my $ret = $action_list{$action}($r);
        if ($action eq 'get_traces' && $ret !~ /ERR/) {
            print "Sending $ret\n";
            $c->send_file_response($ret);

        } else {
            my $h = HTTP::Headers->new('Content-Type' => 'text/plain') ;
            my $r = HTTP::Response->new(HTTP::Status::RC_OK,
                                        'OK', $h, $ret) ;
            print $ret;
            $c->send_response($r) ;
        }
    } else {
        print "$action not found, probably a version problem\n";
        $c->send_error(RC_NOT_FOUND) ;
    }

    $c->close;
}

print "Starting regress-win32.pl daemon...\n";
my $d = HTTP::Daemon->new ( LocalPort =>  8091,
                            ReuseAddr => 1)
    || die "Error: Can't bind $!" ;

my $olddir = Cwd::cwd();
while (1) {
    my $c = $d->accept ;
    my $ip = $c->peerhost;
    if (!$ip) {
        $c->send_error(RC_FORBIDDEN) ;
    } elsif ($src_ip && $ip ne $src_ip) {
        $c->send_error(RC_FORBIDDEN) ;
    } elsif ($c) {
        handle_client($c, $ip) ;
    } else {
        $c->send_error(RC_FORBIDDEN) ;
    }
    close($c) ;
    undef $c;
    chdir($olddir);

    # When we have the reload command, just close the http daemon
    # and exec ourself
    if ($reload) {
        $d->close();
        undef $d;

        exec("$^X $0");
    }
}
