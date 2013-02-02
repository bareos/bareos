#!/usr/bin/perl -w

=head1 NAME

    diff.pl -- Helper to diff files (rights, acl and content)

=head2 USAGE

    diff.pl -s source -d dest [-e exclude ] [--acl | --attr | --wattr]

=cut

use strict;
use Cwd 'chdir';
use File::Find;
no warnings 'File::Find';
use Digest::MD5;
use Getopt::Long ;
use Pod::Usage;
use Data::Dumper;
use Cwd;
use POSIX qw/strftime/;

my ($src, $dst, $help, $acl, $attr, $wattr,
    $dest_attrib, $src_attrib, $mtimedir);
my %src_attr;
my %dst_attr;
my @exclude;
my $hash;
my $ret=0;

GetOptions("src=s"   => \$src,        # source directory
           "dst=s"   => \$dst,        # dest directory
           "acl"     => \$acl,        # acl test
           "attr"    => \$attr,       # attributes test
           "wattr"   => \$wattr,      # windows attributes
           "mtime-dir" => \$mtimedir, # check mtime on directories
           "exclude=s@" => \@exclude, # exclude some files
           "help"    => \$help,
    ) or pod2usage(-verbose => 1,
                   -exitval => 1);
if (!$src or !$dst) {
   pod2usage(-verbose => 1,
             -exitval => 1);
}

if ($help) {
    pod2usage(-verbose => 2,
              -exitval => 0);
}
my $md5 = Digest::MD5->new;

my $dir = getcwd;

chdir($src) or die "ERROR: Can't access to $src";
$hash = \%src_attr;
find(\&wanted_src, '.');

if ($wattr) {
    $src_attrib = `attrib /D /S`;
    $src_attrib = strip_base($src_attrib, $src);
}

chdir ($dir);

chdir($dst) or die "ERROR: Can't access to $dst";
$hash = \%dst_attr;
find(\&wanted_src, '.');

if ($wattr) {
    $dest_attrib = `attrib /D /S`;
    $dest_attrib = strip_base($dest_attrib, $dst);

    if (lc($src_attrib) ne lc($dest_attrib)) {
        $ret++;
        print "diff.pl ERROR: Differences between windows attributes\n",
              "$src_attrib\n=========\n$dest_attrib\n";
    }
}

#print Data::Dumper::Dumper(\%src_attr);
#print Data::Dumper::Dumper(\%dst_attr);

foreach my $f (keys %src_attr)
{
    if (!defined $dst_attr{$f}) {
        $ret++;
        print "diff.pl ERROR: Can't find $f in dst\n";

    } else {
        compare($src_attr{$f}, $dst_attr{$f});
    }
    delete $src_attr{$f};
    delete $dst_attr{$f};
}

foreach my $f (keys %dst_attr)
{
    $ret++;
    print "diff.pl ERROR: Can't find $f in src\n";
}

if ($ret) {
    print "diff.pl ERROR: found $ret error(s)\n";
}

exit $ret;

# convert \ to / and strip the path
sub strip_base
{
    my ($data, $path) = @_;
    $data =~ s!\\!/!sg;
    $data =~ s!\Q$path!!sig;
    return $data;
}

sub compare
{
    my ($h1, $h2) = @_;
    my ($f1, $f2) = ($h1->{file}, $h2->{file});
    my %attr = %$h2;
    foreach my $k (keys %$h1) {
        if (!exists $h2->{$k}) {
            $ret++;
            print "diff.pl ERROR: Can't find $k for dest $f2 ($k=$h1->{$k})\n";
        }
        if (!defined $h2->{$k}) {
            $ret++;
            print "diff.pl ERROR: $k not found in destination ", $h1->{file}, "\n";
            print Data::Dumper::Dumper($h1, $h2);
        } elsif ($h2->{$k} ne $h1->{$k}) {
            $ret++;
            my ($val1, $val2) = ($h1->{$k}, $h2->{$k});
            if ($k =~ /time/) {
                ($val1, $val2) =
                    (map { strftime('%F %T', localtime($_)) } ($val1, $val2));
            }
            print "diff.pl ERROR: src and dst $f2 differ on $k ($val1 != $val2)\n";
        }
        delete $attr{$k};
    }

    foreach my $k (keys %attr) {
        $ret++;
        print "diff.pl ERROR: Found $k on dst file and not on src ($k=$h2->{$k})\n";
    }
}

sub wanted_src
{
    my $f = $_;
    if (grep ($f, @exclude)) {
        return;
    }
    if (-l $f) {
        my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
            $atime,$mtime,$ctime,$blksize,$blocks) = lstat($f);

        my $target = readlink($f);
        $hash->{$File::Find::name} = {
            nlink => $nlink,
            uid => $uid,
            gid => $gid,
            mtime => 0,
            target => $target,
            type => 'l',
            file => $File::Find::name,
        };
        return;
    }

    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
        $atime,$mtime,$ctime,$blksize,$blocks) = stat($f);

    if (-f $f)  {
        $hash->{$File::Find::name} = {
            mode => $mode,
            nlink => $nlink,
            uid => $uid,
            gid => $gid,
            size => $size,
            mtime => $mtime,
            type => 'f',
            file => $File::Find::name,
        };
        $md5->reset;
        open(FILE, '<', $f) or die "ERROR: Can't open '$f': $!";
        binmode(FILE);
        $hash->{$File::Find::name}->{md5} = $md5->addfile(*FILE)->hexdigest;
        close(FILE);

    } elsif (-d $f) {
        $hash->{$File::Find::name} = {
            mode => $mode,
            uid => $uid,
            gid => $gid,
            mtime => ($mtimedir)?$mtime:0,
            type => 'd',
            file =>  $File::Find::name,
        };

    } elsif (-b $f or -c $f) { # dev
        $hash->{$File::Find::name} = {
            mode => $mode,
            uid => $uid,
            gid => $gid,
            mtime => $mtime,
            rdev => $rdev,
            type => (-b $f)?'block':'char',
            file =>  $File::Find::name,
        };

    } elsif (-p $f) { # named pipe
        $hash->{$File::Find::name} = {
            mode => $mode,
            uid => $uid,
            gid => $gid,
            mtime => $mtime,
            type => 'pipe',
            file =>  $File::Find::name,
        };

    } else {                # other than file and directory
        return;
    }

    my $fe = $f;
    $fe =~ s/"/\\"/g;
    if ($acl) {
        $hash->{$File::Find::name}->{acl} = `getfacl "$fe" 2>/dev/null`;
    }
    if ($attr) {
        $hash->{$File::Find::name}->{attr} = `getfattr "$fe" 2>/dev/null`;
    }
}
