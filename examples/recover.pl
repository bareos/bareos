#!/usr/bin/perl -w

=head1 NAME

recover.pl - a script to provide an interface for restore files similar
to Legatto Networker's recover program.

=cut

use strict;
use Getopt::Std;
use DBI;
use Term::ReadKey;
use Term::ReadLine;
use Fcntl ':mode';
use Time::ParseDate;
use Date::Format;
use Text::ParseWords;

# Location of config file.
my $CONF_FILE = "$ENV{HOME}/.recoverrc"; 
my $HIST_FILE = "$ENV{HOME}/.recover.hist";

########################################################################
### Queries needed to gather files from directory.
########################################################################

my %queries = (
	'postgres' => {
		'dir' =>
			"(
				select
					distinct on (name)
					Filename.name,
					Path.path,
					File.lstat,
					File.fileid,
					File.fileindex,
					Job.jobtdate - ? as visible,
					Job.jobid
				from
					Path,
					File,
					Filename,
					Job
				where
					clientid = ? and
					Job.name = ? and
					Job.jobtdate <= ? and
					Path.path = ? and
					File.pathid = Path.pathid and
					Filename.filenameid = File.filenameid and
					Filename.name != '' and
					File.jobid = Job.jobid
				order by
					name,
					jobid desc
			)
			union
			(
				select
					distinct on (name)
					substring(Path.path from ? + 1) as name,
					substring(Path.path from 1 for ?) as path,
					File.lstat,
					File.fileid,
					File.fileindex,
					Job.jobtdate - ? as visible,
					Job.jobid
				from
					Path,
					File,
					Filename,
					Job
				where
					clientid = ? and
					Job.name = ? and
					Job.jobtdate <= ? and
					File.jobid = Job.jobid and
					Filename.name = '' and
					Filename.filenameid = File.filenameid and
					File.pathid = Path.pathid and
					Path.path ~ ('^' || ? || '[^/]*/\$')
				order by
					name,
					jobid desc
			)
			order by
				name
		",
		'sel' =>
			"(
				select
					distinct on (name)
					Path.path || Filename.name as name,
					File.fileid,
					File.lstat,
					File.fileindex,
					Job.jobid
				from
					Path,
					File,
					Filename,
					Job
				where
					clientid = ? and
					Job.name = ? and
					Job.jobtdate <= ? and
					Job.jobtdate >= ? and
					Path.path like ? || '%' and
					File.pathid = Path.pathid and
					Filename.filenameid = File.filenameid and
					Filename.name != '' and
					File.jobid = Job.jobid
				order by
					name, jobid desc
			)
			union
			(
				select
					distinct on (name)
					Path.path as name,
					File.fileid,
					File.lstat,
					File.fileindex,
					Job.jobid
				from
					Path,
					File,
					Filename,
					Job
				where
					clientid = ? and
					Job.name = ? and
					Job.jobtdate <= ? and
					Job.jobtdate >= ? and
					File.jobid = Job.jobid and
					Filename.name = '' and
					Filename.filenameid = File.filenameid and
					File.pathid = Path.pathid and
					Path.path like ? || '%'
				order by
					name, jobid desc
			)
		",
		'cache' =>
			"select
				distinct on (path, name)
				Path.path,
				Filename.name,
				File.fileid,
				File.lstat,
				File.fileindex,
				Job.jobtdate - ? as visible,
				Job.jobid
			from
				Path,
				File,
				Filename,
				Job
			where
				clientid = ? and
				Job.name = ? and
				Job.jobtdate <= ? and
				Job.jobtdate >= ? and
				File.pathid = Path.pathid and
				File.filenameid = Filename.filenameid and
				File.jobid = Job.jobid
			order by
				path, name, jobid desc
		",
		'ver' =>
			"select
				Path.path,
				Filename.name,
				File.fileid,
				File.fileindex,
				File.lstat,
				Job.jobtdate,
				Job.jobid,
				Job.jobtdate - ? as visible,
				Media.volumename
			from
				Job, Path, Filename, File, JobMedia, Media
			where
				File.pathid = Path.pathid and
				File.filenameid = Filename.filenameid and
				File.jobid = Job.jobid and
				File.Jobid = JobMedia.jobid and
				File.fileindex >= JobMedia.firstindex and
				File.fileindex <= JobMedia.lastindex and
				Job.jobtdate <= ? and
				JobMedia.mediaid = Media.mediaid and
				Path.path = ? and
				Filename.name = ? and
				Job.clientid = ? and
				Job.name = ?
			order by job
		"
	},
	'mysql' => {
		'dir' =>
			"
			(
				select
					distinct(Filename.name),
					Path.path,
					File.lstat,
					File.fileid,
					File.fileindex,
					Job.jobtdate - ? as visible,
					Job.jobid
				from
					Path,
					File,
					Filename,
					Job
				where
					clientid = ? and
					Job.name = ? and
					Job.jobtdate <= ? and
					Path.path = ? and
					File.pathid = Path.pathid and
					Filename.filenameid = File.filenameid and
					Filename.name != '' and
					File.jobid = Job.jobid
				group by
					name
				order by
					name,
					jobid desc
			)
			union
			(
				select
					distinct(substring(Path.path from ? + 1)) as name,
					substring(Path.path from 1 for ?) as path,
					File.lstat,
					File.fileid,
					File.fileindex,
					Job.jobtdate - ? as visible,
					Job.jobid
				from
					Path,
					File,
					Filename,
					Job
				where
					clientid = ? and
					Job.name = ? and
					Job.jobtdate <= ? and
					File.jobid = Job.jobid and
					Filename.name = '' and
					Filename.filenameid = File.filenameid and
					File.pathid = Path.pathid and
					Path.path rlike concat('^', ?, '[^/]*/\$')
				group by
					name
				order by
					name,
					jobid desc
			)
			order by
				name
		",
		'sel' =>
			"
			(
			select
				distinct(concat(Path.path, Filename.name)) as name,
				File.fileid,
				File.lstat,
				File.fileindex,
				Job.jobid
			from
				Path,
				File,
				Filename,
				Job
			where
				Job.clientid = ? and
				Job.name = ? and
				Job.jobtdate <= ? and
				Job.jobtdate >= ? and
				Path.path like concat(?, '%') and
				File.pathid = Path.pathid and
				Filename.filenameid = File.filenameid and
				Filename.name != '' and
				File.jobid = Job.jobid
			group by
				path, name
			order by
				name,
				jobid desc
			)
			union
			(
			select
				distinct(Path.path) as name,
				File.fileid,
				File.lstat,
				File.fileindex,
				Job.jobid
			from
				Path,
				File,
				Filename,
				Job
			where
				Job.clientid = ? and
				Job.name = ? and
				Job.jobtdate <= ? and
				Job.jobtdate >= ? and
				File.jobid = Job.jobid and
				Filename.name = '' and
				Filename.filenameid = File.filenameid and
				File.pathid = Path.pathid and
				Path.path like concat(?, '%')
			group by
				path
			order by
				name,
				jobid desc
			)
		",
		'cache' =>
			"select
				distinct path,
				Filename.name,
				File.fileid,
				File.lstat,
				File.fileindex,
				Job.jobtdate - ? as visible,
				Job.jobid
			from
				Path,
				File,
				Filename,
				Job
			where
				clientid = ? and
				Job.name = ? and
				Job.jobtdate <= ? and
				Job.jobtdate >= ? and
				File.pathid = Path.pathid and
				File.filenameid = Filename.filenameid and
				File.jobid = Job.jobid
			group by
				path, name
			order by
				path, name, jobid desc
		",
		'ver' =>
			"select
				Path.path,
				Filename.name,
				File.fileid,
				File.fileindex,
				File.lstat,
				Job.jobtdate,
				Job.jobid,
				Job.jobtdate - ? as visible,
				Media.volumename
			from
				Job, Path, Filename, File, JobMedia, Media
			where
				File.pathid = Path.pathid and
				File.filenameid = Filename.filenameid and
				File.jobid = Job.jobid and
				File.Jobid = JobMedia.jobid and
				File.fileindex >= JobMedia.firstindex and
				File.fileindex <= JobMedia.lastindex and
				Job.jobtdate <= ? and
				JobMedia.mediaid = Media.mediaid and
				Path.path = ? and
				Filename.name = ? and
				Job.clientid = ? and
				Job.name = ?
			order by job
		"
	}
);

############################################################################
### Command lists for help and file completion
############################################################################

my %COMMANDS = (
	'add' => '(add files) - Add files recursively to restore list',
	'bootstrap' => 'print bootstrap file',
	'cd' => '(cd dir) - Change working directory',
	'changetime', '(changetime date/time) - Change database view to date',
	'client' => '(client client-name) - change client to view',
	'debug' => 'toggle debug flag',
	'delete' => 'Remove files from restore list.',
	'help' => 'Display this list',
	'history', 'Print command history',
	'info', '(info files) - Print stat and tape information about files',
	'ls' => '(ls [opts] files) - List files in current directory',
	'pwd' => 'Print current working directory',
	'quit' => 'Exit program',
	'recover', 'Create table for bconsole to use in recover',
	'relocate', '(relocate dir) - specify new location for recovered files',
	'show', '(show item) - Display information about item',
	'verbose' => 'toggle verbose flag',
	'versions', '(versions files) - Show all versions of file on tape',
	'volumes', 'Show volumes needed for restore.'
);

my %SHOW = (
	'cache' => 'Display cached directories',
	'catalog' => 'Display name of current catalog from config file',
	'client' => 'Display current client',
	'clients' => 'Display clients available in this catalog',
	'restore' => 'Display information about pending restore',
	'volumes' => 'Show volumes needed for restore.'
);

##############################################################################
### Read config and command line.
##############################################################################

my %catalogs;
my $catalog;	# Current catalog

## Globals

my %restore;
my $rnum = 0;
my $rbytes = 0;
my $debug = 0;
my $verbose = 0;
my $rtime;
my $cwd;
my $lwd;
my $files;
my $restore_to = '/';
my $start_dir;
my $preload;
my $dircache = {};
my $usecache = 1;

=head1 SYNTAX

B<recover.pl> [B<-b> I<db connect string>] [B<-c> I<client> B<-j> I<jobname>]
[B<-i> I<initial diretory>] [B<-p>] [B<-t> I<timespec>]

B<recover.pl> [B<-h>]

Most of the command line arguments can be specified in the init file
B<$HOME/.recoverrc> (see CONFIG FILE FORMAT below). The command
line arguments will override the options in the init file. If no
I<catalogname> is specified, the first one found in the init file will
be used.

=head1 DESCRIPTION

B<recover.pl> will read the specified catalog and provide a shell like
environment from which a time based view of the specified client/jobname
and be exampled and selected for restoration.

The command line option B<-b> specified the DBI compatible connect
script to use when connecting to the catalog database. The B<-c> and
B<-j> options specify the client and jobname respectively to view from
the catalog database. The B<-i> option will set the initial directory
you are viewing to the specified directory. if B<-i> is not specified,
it will default to /. You can set the initial time to view the catalog
from using the B<-t> option.

The B<-p> option will pre-load the entire catalog into memory. This
could take a lot of memory, so use it with caution.

The B<-d> option turns on debugging and the B<-v> option turns on
verbose output.

By specifying a I<catalogname>, the default options for connecting to
the catalog database will be taken from the section of the init file
specified by that name.

The B<-h> option will display this document.

In order for this program to have a chance of not being painfully slow,
the following indexs should be added to your database.

B<CREATE INDEX file_pathid_idx on file(pathid);>

B<CREATE INDEX file_filenameid_idx on file(filenameid);>

=cut

my $vars = {};
getopts("c:b:hi:j:pt:vd", $vars) || die "Usage: bad arguments\n";

if ($vars->{'h'}) {
	system("perldoc $0");
	exit;
}

$preload = $vars->{'p'} if ($vars->{'p'});
$debug = $vars->{'d'} if ($vars->{'d'});
$verbose = $vars->{'v'} if ($vars->{'v'});

# Set initial time to view the catalog

if ($vars->{'t'}) {
	$rtime = parsedate($vars->{'t'}, FUZZY => 1, PREFER_PAST => 1);
}
else {
	$rtime = time();
}

my $dbconnect;
my $username = "";
my $password = "";
my $db;
my $client;
my $jobname;
my $jobs;
my $ftime;

my $cstr;

# Read config file (if available).

&read_config($CONF_FILE);

# Set defaults

$catalog = $ARGV[0] if (@ARGV);

if ($catalog) {
	$cstr = ${catalogs{$catalog}}->{'client'}
		if (${catalogs{$catalog}}->{'client'});

	$jobname = $catalogs{$catalog}->{'jobname'}
		if ($catalogs{$catalog}->{'jobname'});

	$dbconnect = $catalogs{$catalog}->{'dbconnect'}
		if ($catalogs{$catalog}->{'dbconnect'});

	$username = $catalogs{$catalog}->{'username'}
		if ($catalogs{$catalog}->{'username'});

	$password = $catalogs{$catalog}->{'password'}
		if ($catalogs{$catalog}->{'password'});

	$start_dir = $catalogs{$catalog}->{'cd'}
		if ($catalogs{$catalog}->{'cd'});

	$preload = $catalogs{$catalog}->{'preload'}
		if ($catalogs{$catalog}->{'preload'} && !defined($vars->{'p'}));

	$verbose = $catalogs{$catalog}->{'verbose'}
		if ($catalogs{$catalog}->{'verbose'} && !defined($vars->{'v'}));

	$debug = $catalogs{$catalog}->{'debug'}
		if ($catalogs{$catalog}->{'debug'} && !defined($vars->{'d'}));
}

#### Command line overries config file

$start_dir = $vars->{'i'} if ($vars->{'i'});
$start_dir = '/' if (!$start_dir);

$start_dir .= '/' if (substr($start_dir, length($start_dir) - 1, 1) ne '/');

if ($vars->{'b'}) {
	$dbconnect = $vars->{'b'};
}

die "You must supply a db connect string.\n" if (!defined($dbconnect));

if ($dbconnect =~ /^dbi:Pg/) {
	$db = 'postgres';
}
elsif ($dbconnect =~ /^dbi:mysql/) {
	$db = 'mysql';
}
else {
	die "Unknown database type specified in $dbconnect\n";
}

# Initialize database connection

print STDERR "DBG: Connect using: $dbconnect\n" if ($debug);

my $dbh = DBI->connect($dbconnect, $username, $password) ||
        die "Can't open bacula database\nDatabase connect string '$dbconnect'";

die "Client id required.\n" if (!($cstr || $vars->{'c'}));

$cstr = $vars->{'c'} if ($vars->{'c'});
$client = &lookup_client($cstr);

# Set job information
$jobname = $vars->{'j'} if ($vars->{'j'});

die "You need to specify a job name.\n" if (!$jobname);

&setjob;

die "Failed to set client\n" if (!$client);

# Prepare our query
my $dir_sth = $dbh->prepare($queries{$db}->{'dir'})
	|| die "Can't prepare $queries{$db}->{'dir'}\n";

my $sel_sth = $dbh->prepare($queries{$db}->{'sel'})
	|| die "Can't prepare $queries{$db}->{'sel'}\n";

my $ver_sth = $dbh->prepare($queries{$db}->{'ver'})
	|| die "Can't prepare $queries{$db}->{'ver'}\n";

my $clients;

# Initialize readline.
my $term = new Term::ReadLine('Bacula Recover');
$term->ornaments(0);

my $readline = $term->ReadLine;
my $tty_attribs = $term->Attribs;

# Needed for base64 decode

my @base64_digits = (
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
);
my @base64_map = (0) x 128;

for (my $i=0; $i<64; $i++) {
	$base64_map[ord($base64_digits[$i])] = $i;
}

##############################################################################
### Support routines
##############################################################################

=head1 FILES

B<$HOME/.recoverrc> Configuration file for B<recover.pl>.

=head1 CONFIG FILE FORMAT

The config file will allow you to specify the defaults for your
catalog(s). Each catalog definition starts with B<[>I<catalogname>B<]>.
Blank lines and lines starting with # are ignored.

The first catalog specified will be used as the default catalog.

All values are specified in I<item> B<=> I<value> format. You can
specify the following I<item>s for each catalog.

=cut

sub read_config {
	my $conf_file = shift;
	my $c;

	# No nothing if config file can't be read.

	if (-r $conf_file) {
		open(CONF, "<$conf_file") || die "$!: Can't open $conf_file\n";

		while (<CONF>) {
			chomp;
			# Skip comments and blank links
			next if (/^\s*#/);
			next if (/^\s*$/);

			if (/^\[(\w+)\]$/) {
				$c = $1;
				$catalog = $c if (!$catalog);

				if ($catalogs{$c}) {
					die "Duplicate catalog definition in $conf_file\n";
				}

				$catalogs{$c} = {};
			}
			elsif (!$c) {
				die "Conf file must start with catalog definition [catname]\n";
			}
			else {

				if (/^(\w+)\s*=\s*(.*)/) {
					my $item = $1;
					my $value = $2;

=head2 client

The name of the default client to view when connecting to this
catalog. This can be changed later with the B<client> command.

=cut

					if ($item eq 'client') {
						$catalogs{$c}->{'client'} = $value;
					}

=head2 dbconnect

The DBI compatible database string to use to connect to this catalog.

=over 4

=item B<example:>

dbi:Pg:dbname=bacula;host=backuphost

=back

=cut
					elsif ($item eq 'dbconnect') {
						$catalogs{$c}->{'dbconnect'} = $value;
					}

=head2 jobname

The name of the default job to view when connecting to the catalog. This
can be changed later with the B<client> command.

=cut
					elsif ($item eq 'jobname') {
						$catalogs{$c}->{'jobname'} = $value;
					}

=head2 password

The password to use when connecing to the catalog database.

=cut
					elsif ($item eq 'password') {
						$catalogs{$c}->{'password'} = $value;
					}

=head2 preload

Set the preload flag. A preload flag of 1 or on will load the entire
catalog when recover.pl is start. This is a memory hog, so use with
caution.

=cut
					elsif ($item eq 'preload') {

						if ($value =~ /^(1|on)$/i) {
							$catalogs{$c}->{'preload'} = 1;
						}
						elsif ($value =~ /^(0|off)$/i) {
							$catalogs{$c}->{'preload'} = 0;
						}
						else {
							die "$value: Unknown value for preload.\n";
						}

					}

=head2 username

The username to use when connecing to the catalog database.

=cut
					elsif ($item eq 'username') {
						$catalogs{$c}->{'username'} = $value;
					}
					else {
						die "Unknown opton $item in $conf_file.\n";
					}

				}
				else {
					die "Bad line $_ in $conf_file.\n";
				}

			}

		}

		close(CONF);
	}

}

sub create_file_entry {
	my $name = shift;
	my $fileid = shift;
	my $fileindex = shift;
	my $jobid = shift;
	my $visible = shift;
	my $lstat = shift;

	print STDERR "DBG: name = $name\n" if ($debug);
	print STDERR "DBG: fileid = $fileid\n" if ($debug);
	print STDERR "DBG: fileindex = $fileindex\n" if ($debug);
	print STDERR "DBG: jobid = $jobid\n" if ($debug);
	print STDERR "DBG: visible = $visible\n" if ($debug);
	print STDERR "DBG: lstat = $lstat\n" if ($debug);

	my $data = {
		fileid => $fileid,
		fileindex => $fileindex,
		jobid => $jobid,
		visible => ($visible >= 0) ? 1 : 0
	};

	# decode file stat
	my @stat = ();

	foreach my $s (split(' ', $lstat)) {
		print STDERR "DBG: Add $s to stat array.\n" if ($debug);
		push(@stat, from_base64($s));
	}

	$data->{'lstat'} = {
		'st_dev' => $stat[0],
		'st_ino' => $stat[1],
		'st_mode' => $stat[2],
		'st_nlink' => $stat[3],
		'st_uid' => $stat[4],
		'st_gid' => $stat[5],
		'st_rdev' => $stat[6],
		'st_size' => $stat[7],
		'st_blksize' => $stat[8],
		'st_blocks' => $stat[9],
		'st_atime' => $stat[10],
		'st_mtime' => $stat[11],
		'st_ctime' => $stat[12],
		'LinkFI' => $stat[13],
		'st_flags' => $stat[14],
		'data_stream' => $stat[15]
	};

	# Create mode string.
	my $sstr = &mode2str($stat[2]);
	$data->{'lstat'}->{'statstr'} = $sstr;
	return $data;
}
# Read directory data, return hash reference.

sub fetch_dir {
	my $dir = shift;

	return $dircache->{$dir} if ($dircache->{$dir});

	print "$dir not cached, fetching from database.\n" if ($verbose);
	my $data = {};
	my $fmax = 0;

	my $dl = length($dir);

	print STDERR "? - 1: ftime = $ftime\n" if ($debug);
	print STDERR "? - 2: client = $client\n" if ($debug);
	print STDERR "? - 3: jobname = $jobname\n" if ($debug);
	print STDERR "? - 4: rtime = $rtime\n" if ($debug);
	print STDERR "? - 5: dir = $dir\n" if ($debug);
	print STDERR "? - 6, 7: dl = $dl, $dl\n" if ($debug);
	print STDERR "? - 8: ftime = $ftime\n" if ($debug);
	print STDERR "? - 9: client = $client\n" if ($debug);
	print STDERR "? - 10: jobname = $jobname\n" if ($debug);
	print STDERR "? - 11: rtime = $rtime\n" if ($debug);
	print STDERR "? - 12: dir = $dir\n" if ($debug);

	print STDERR "DBG: Execute - $queries{$db}->{'dir'}\n" if ($debug);
	$dir_sth->execute(
		$ftime,
		$client,
		$jobname,
		$rtime,
		$dir,
		$dl, $dl,
		$ftime,
		$client,
		$jobname,
		$rtime,
		$dir
	) || die "Can't execute $queries{$db}->{'dir'}\n";

	while (my $ref = $dir_sth->fetchrow_hashref) {
		my $file = $$ref{name};
		print STDERR "DBG: File $file found in database.\n" if ($debug);
		my $l = length($file);
		$fmax = $l if ($l > $fmax);

		$data->{$file} = &create_file_entry(
			$file,
			$ref->{'fileid'},
			$ref->{'fileindex'},
			$ref->{'jobid'},
			$ref->{'visible'},
			$ref->{'lstat'}
		);
	}

	return undef if (!$fmax);

	$dircache->{$dir} = $data if ($usecache);
	return $data;
}

sub cache_catalog {
	print "Loading entire catalog, please wait...\n";
	my $sth = $dbh->prepare($queries{$db}->{'cache'})
		|| die "Can't prepare $queries{$db}->{'cache'}\n";
	print STDERR "DBG: Execute - $queries{$db}->{'cache'}\n" if ($debug);
	$sth->execute($ftime, $client, $jobname, $rtime, $ftime)
		|| die "Can't execute $queries{$db}->{'cache'}\n";

	print "Query complete, building catalog cache...\n" if ($verbose);

	while (my $ref = $sth->fetchrow_hashref) {
		my $dir = $ref->{path};
		my $file = $ref->{name};
		print STDERR "DBG: File $dir$file found in database.\n" if ($debug);

		next if ($dir eq '/' and $file eq '');	# Skip data for /

		# Rearrange directory

		if ($file eq '' and $dir =~ m|(.*/)([^/]+/)$|) {
			$dir = $1;
			$file = $2;
		}

		my $data = &create_file_entry(
			$file,
			$ref->{'fileid'},
			$ref->{'fileindex'},
			$ref->{'jobid'},
			$ref->{'visible'},
			$ref->{'lstat'}
		);

		$dircache->{$dir} = {} if (!$dircache->{$dir});
		$dircache->{$dir}->{$file} = $data;
	}

	$sth->finish();
}

# Break a path up into dir and file.

sub path_parts {
	my $path = shift;
	my $fqdir;
	my $dir;
	my $file;

	if (substr($path, 0, 1) eq '/') {

		# Find dir vs. file
		if ($path =~ m|^(/.*/)([^/]*$)|) {
			$fqdir = $dir = $1;
			$file = $2;
		}
		else { # Must be in /
			$fqdir = $dir = '/';
			$file = substr($path, 1);
		}

		print STDERR "DBG: / Dir - $dir; file = $file\n" if ($debug);
	}
	# relative path
	elsif ($path =~ m|^(.*/)([^/]*)$|) {
		$fqdir = "$cwd$1";
		$dir = $1;
		$file = $2;
		print STDERR "DBG: Dir - $dir; file = $file\n" if ($debug);
	}
	# File is in our current directory.
	else {
		$fqdir = $cwd;
		$dir = '';
		$file = $path;
		print STDERR "DBG: Set dir to $dir\n" if ($debug);
	}
	
	return ($fqdir, $dir, $file);
}

sub lookup_client {
	my $c = shift;

	if (!$clients) {
		$clients = {};
		my $query = "select clientid, name from Client";
		my $sth = $dbh->prepare($query) || die "Can't prepare $query\n";
		$sth->execute || die "Can't execute $query\n";

		while (my $ref = $sth->fetchrow_hashref) {
			$clients->{$ref->{'name'}} = $ref->{'clientid'};
		}

		$sth->finish;
	}

	if ($c !~ /^\d+$/) {

		if ($clients->{$c}) {
			$c = $clients->{$c};
		}
		else {
			warn "Could not find client $c\n";
			$c = $client;
		}

	}

	return $c;
}

sub setjob {

	if (!$jobs) {
		$jobs = {};
		my $query = "select distinct name from Job order by name";
		my $sth = $dbh->prepare($query) || die "Can't prepare $query\n";
		$sth->execute || die "Can't execute $query\n";

		while (my $ref = $sth->fetchrow_hashref) {
			$jobs->{$$ref{'name'}} = $$ref{'name'};
		}

		$sth->finish;
	}

	my $query = "select
		jobtdate
	from
		Job
	where
		jobtdate <= $rtime and
		name = '$jobname' and
		level = 'F'
	order by jobtdate desc
	limit 1
	";

	my $sth = $dbh->prepare($query) || die "Can't prepare $query\n";
	$sth->execute || die "Can't execute $query\n";

	if ($sth->rows == 1) {	
		my $ref = $sth->fetchrow_hashref;
		$ftime = $$ref{jobtdate};
	}
	else {
		warn "Could not find full backup. Setting full time to 0.\n";
		$ftime = 0;
	}

	$sth->finish;
}

sub select_files {
	my $mark = shift;
	my $opts = shift;
	my $dir = shift;
	my @flist = @_;

	if (!@flist) {

		if ($cwd eq '/') {
			my $finfo = &fetch_dir('/');
			@flist = keys %$finfo;
		}
		else {
			@flist = ($cwd);
		}

	}

	foreach my $f (@flist) {
		$f =~ s|/+$||;
		my $path = (substr($f, 0, 1) eq '/') ? $f : "$dir$f";
		my ($fqdir, $dir, $file) = &path_parts($path);
		my $finfo = &fetch_dir($fqdir);

		if (!$finfo->{$file}) {

			if (!$finfo->{"$file/"}) {
				warn "$f: File not found.\n";
				next;
			}

			$file .= '/';
		}

		my $info = $finfo->{$file};

		my $fid = $info->{'fileid'};
		my $fidx = $info->{'fileindex'};
		my $jid = $info->{'jobid'};
		my $size = $info->{'lstat'}->{'st_size'};

		if ($opts->{'all'} || $info->{'visible'}) {
			print STDERR "DBG: $file - $size bytes\n"
				if ($debug);

			if ($mark) {

				if (!$restore{$fid}) {
					print "Adding $fqdir$file\n" if (!$opts->{'quiet'});
					$restore{$fid} = [$jid, $fidx];
					$rnum++;
					$rbytes += $size;
				}

			}
			else {

				if ($restore{$fid}) {
					print "Removing $fqdir$file\n" if (!$opts->{'quiet'});
					delete $restore{$fid};
					$rnum--;
					$rbytes -= $size;
				}

			}

			if ($file =~ m|/$|) {

				# Use preloaded files if we already retrieved them.
				if ($preload) {
					my $newdir = "$dir$file";
					my $finfo = &fetch_dir($newdir);
					&select_files($mark, $opts, $newdir, keys %$finfo);
					next;
				}
				else {
					my $newdir = "$fqdir$file";
					my $begin = ($opts->{'all'}) ? 0 : $ftime;

					print STDERR "DBG: Execute - $queries{$db}->{'sel'}\n"
						if ($debug);

					$sel_sth->execute(
						$client,
						$jobname,
						$rtime,
						$begin,
						$newdir,
						$client,
						$jobname,
						$rtime,
						$begin,
						$newdir
					) || die "Can't execute $queries{$db}->{'sel'}\n";

					while (my $ref = $sel_sth->fetchrow_hashref) {
						my $file = $$ref{'name'};
						my $fid = $$ref{'fileid'};
						my $fidx = $$ref{'fileindex'};
						my $jid = $$ref{'jobid'};
						my @stat_enc = split(' ', $$ref{'lstat'});
						my $size = &from_base64($stat_enc[7]);

						if ($mark) {

							if (!$restore{$fid}) {
								print "Adding $file\n" if (!$opts->{'quiet'});
								$restore{$fid} = [$jid, $fidx];
								$rnum++;
								$rbytes += $size;
							}

						}
						else {

							if ($restore{$fid}) {
								print "Removing $file\n" if (!$opts->{'quiet'});
								delete $restore{$fid};
								$rnum--;
								$rbytes -= $size;
							}

						}

					}

				}

			}

		}

	}

}

# Expand shell wildcards

sub expand_files {
	my $path = shift;
	my ($fqdir, $dir, $file) = &path_parts($path);
	my $finfo = &fetch_dir($fqdir);
	return ($path) if (!$finfo);

	my $pat = "^$file\$";

	# Add / for dir match
	my $dpat = $file;
	$dpat =~ s|/+$||;
	$dpat = "^$dpat/\$";

	my @match;

	$pat =~ s/\./\\./g;
	$dpat =~ s/\./\\./g;
	$pat =~ s/\?/./g;
	$dpat =~ s/\?/./g;
	$pat =~ s/\*/.*/g;
	$dpat =~ s/\*/.*/g;

	foreach my $f (sort keys %$finfo) {

		if ($f =~ /$pat/) {
			push (@match, ($fqdir eq $cwd) ? $f : "$fqdir$f");
		}
		elsif ($f =~ /$dpat/) {
			push (@match, ($fqdir eq $cwd) ? $f : "$fqdir$f");
		}

	}

	return ($path) if (!@match);
	return @match;
}

sub expand_dirs {
	my $path = shift;
	my ($fqdir, $dir, $file) = &path_parts($path, 1);

	print STDERR "Expand $path\n" if ($debug);

	my $finfo = &fetch_dir($fqdir);
	return ($path) if (!$finfo);

	$file =~ s|/+$||;

	my $pat = "^$file/\$";
	my @match;

	$pat =~ s/\./\\./g;
	$pat =~ s/\?/./g;
	$pat =~ s/\*/.*/g;

	foreach my $f (sort keys %$finfo) {
		print STDERR "Match $f to $pat\n" if ($debug);
		push (@match, ($fqdir eq $cwd) ? $f : "$fqdir$f") if ($f =~ /$pat/);
	}

	return ($path) if (!@match);
	return @match;
}

sub mode2str {
	my $mode = shift;
	my $sstr = '';

	if (S_ISDIR($mode)) {
		$sstr = 'd';
	}
	elsif (S_ISCHR($mode)) {
		$sstr = 'c';
	}
	elsif (S_ISBLK($mode)) {
		$sstr = 'b';
	}
	elsif (S_ISREG($mode)) {
		$sstr = '-';
	}
	elsif (S_ISFIFO($mode)) {
		$sstr = 'f';
	}
	elsif (S_ISLNK($mode)) {
		$sstr = 'l';
	}
	elsif (S_ISSOCK($mode)) {
		$sstr = 's';
	}
	else {
		$sstr = '?';
	}

	$sstr .= ($mode&S_IRUSR) ? 'r' : '-';
	$sstr .= ($mode&S_IWUSR) ? 'w' : '-';
	$sstr .= ($mode&S_IXUSR) ?
		(($mode&S_ISUID) ? 's' : 'x') :
		(($mode&S_ISUID) ? 'S' : '-');
	$sstr .= ($mode&S_IRGRP) ? 'r' : '-';
	$sstr .= ($mode&S_IWGRP) ? 'w' : '-';
	$sstr .= ($mode&S_IXGRP) ?
		(($mode&S_ISGID) ? 's' : 'x') :
		(($mode&S_ISGID) ? 'S' : '-');
	$sstr .= ($mode&S_IROTH) ? 'r' : '-';
	$sstr .= ($mode&S_IWOTH) ? 'w' : '-';
	$sstr .= ($mode&S_IXOTH) ?
		(($mode&S_ISVTX) ? 't' : 'x') :
		(($mode&S_ISVTX) ? 'T' : '-');

	return $sstr;
}

# Base 64 decoder
# Algorithm copied from bacula source

sub from_base64 {
	my $where = shift;
	my $val = 0;
	my $i = 0;
	my $neg = 0;

	if (substr($where, 0, 1) eq '-') {
		$neg = 1;
		$where = substr($where, 1);
	}

	while ($where ne '') {
		$val <<= 6;
		my $d = substr($where, 0, 1);
		#print STDERR "\n$d - " . ord($d) . " - " . $base64_map[ord($d)] . "\n";
		$val += $base64_map[ord(substr($where, 0, 1))];
		$where = substr($where, 1);
	}

	return $val;
}

### Command completion code

sub get_match {
	my @m = @_;
	my $r = '';

	for (my $i = 0, my $matched = 1; $i < length($m[0]) && $matched; $i++) {
		my $c = substr($m[0], $i, 1);

		for (my $j = 1; $j < @m; $j++) {

			if ($c ne substr($m[$j], $i, 1)) {
				$matched = 0;
				last;
			}

		}

		$r .= $c if ($matched);
	}

	return $r;
}

sub complete {
	my $text = shift;
	my $line = shift;
	my $start = shift;
	my $end = shift;

	$tty_attribs->{'completion_append_character'} = ' ';
	$tty_attribs->{completion_entry_function} = \&nocomplete;
	print STDERR "\nDBG: text - $text; line - $line; start - $start; end = $end\n"
		if ($debug);

	# Complete command if we are at start of line.

	if ($start == 0 || substr($line, 0, $start) =~ /^\s*$/) {
		my @list = grep (/^$text/, sort keys %COMMANDS);
		return () if (!@list);
		my $match = (@list > 1) ? &get_match(@list) : '';
		return $match, @list;
	}
	else {
		# Count arguments
		my $cstr = $line;
		$cstr =~ s/^\s+//;	# Remove leading spaces

		my ($cmd, @args) = shellwords($cstr);
		return () if (!defined($cmd));

		# Complete dirs for cd
		if ($cmd eq 'cd') {
			return () if (@args > 1);
			return &complete_files($text, 1);
		}
		# Complete files/dirs for info and ls
		elsif ($cmd =~ /^(add|delete|info|ls|mark|unmark|versions)$/) {
			return &complete_files($text, 0);
		}
		# Complete clients for client
		elsif ($cmd eq 'client') {
			return () if (@args > 2);
			my $pat = $text;
			$pat =~ s/\./\\./g;
			my @flist;

			print STDERR "DBG: " . (@args) . " arguments found.\n" if ($debug);

			if (@args < 1 || (@args == 1 and $line =~ /[^\s]$/)) {
				@flist = grep (/^$pat/, sort keys %$clients);
			}
			else {
				@flist = grep (/^$pat/, sort keys %$jobs);
			}

			return () if (!@flist);
			my $match = (@flist > 1) ? &get_match(@flist) : '';

			#return $match, map {s/ /\\ /g; $_} @flist;
			return $match, @flist;
		}
		# Complete show options for show
		elsif ($cmd eq 'show') {
			return () if (@args > 1);
			# attempt to suggest match.
			my @list = grep (/^$text/, sort keys %SHOW);
			return () if (!@list);
			my $match = (@list > 1) ? &get_match(@list) : '';
			return $match, @list;
		}
		elsif ($cmd =~ /^(bsr|bootstrap|relocate)$/) {
			$tty_attribs->{completion_entry_function} =
				$tty_attribs->{filename_completion_function};
		}

	}

	return ();
}

sub complete_files {
	my $path = shift;
	my $dironly = shift;
	my $finfo;
	my @flist;

	my ($fqdir, $dir, $pat) = &path_parts($path, 1);

	$pat =~ s/([.\[\]\\])/\\$1/g;
	# First check for absolute name.

	$finfo = &fetch_dir($fqdir);
	print STDERR "DBG: " . join(', ', keys %$finfo) . "\n" if ($debug);
	return () if (!$finfo);		# Nothing if dir not found.

	if ($dironly) {
		@flist = grep (m|^$pat.*/$|, sort keys %$finfo);
	}
	else {
		@flist = grep (/^$pat/, sort keys %$finfo);
	}

	return undef if (!@flist);

	print STDERR "DBG: Files found\n" if ($debug);

	if (@flist == 1 && $flist[0] =~ m|/$|) {
		$tty_attribs->{'completion_append_character'} = '';
	}

	@flist = map {s/ /\\ /g; ($fqdir eq $cwd) ? $_ : "$dir$_"} @flist;
	my $match = (@flist > 1) ? &get_match(@flist) : '';

	print STDERR "DBG: Dir - $dir; cwd - $cwd\n" if ($debug);
	# Fill in dir if necessary.
	return $match, @flist;
}

sub nocomplete {
	return ();
}

# subroutine to create printf format for long listing of ls

sub long_fmt {
	my $flist = shift;
	my $fmax = 0;
	my $lmax = 0;
	my $umax = 0;
	my $gmax = 0;
	my $smax = 0;

	foreach my $f (@$flist) {
		my $file = $f->[0];
		my $info = $f->[1];
		my $lstat = $info->{'lstat'};

		my $l = length($file);
		$fmax = $l if ($l > $fmax);

		$l = length($lstat->{'st_nlink'});
		$lmax = $l if ($l > $lmax);
		$l = length($lstat->{'st_uid'});
		$umax = $l if ($l > $umax);
		$l = length($lstat->{'st_gid'});
		$gmax = $l if ($l > $gmax);
		$l = length($lstat->{'st_size'});
		$smax = $l if ($l > $smax);
	}

	return "%s %${lmax}d %${umax}d %${gmax}d %${smax}d %s %s\n";
}

sub print_by_cols {
	my @list = @_;
	my $l = @list;
	my $w = $term->get_screen_size;
	my @wds = (1);
	my $m = $w/3 + 1;
	my $max_cols = ($m < @list) ? $w : @list;
	my $fpc = 1;
	my $cols = 1;

	print STDERR "Need to print $l files\n" if ($debug);

	while($max_cols > 1) {
		my $used = 0;

		# Initialize array of widths
		@wds = 0 x $max_cols;

		for ($cols = 0; $cols < $max_cols && $used < $w; $cols++) {
			my $cw = 0;

			for (my $j = $cols*$fpc; $j < ($cols + 1)*$fpc && $j < $l; $j++ ) {
				my $fl = length($list[$j]->[0]);
				$cw = $fl if ($fl > $cw);
			}

			$wds[$cols] = $cw;
			$used += $cw;
			print STDERR "DBG: Total so far is $used\n" if ($debug);

			if ($used >= $w) {
				$cols++;
				last;
			}

			$used += 3;
		}

		print STDERR "DBG: $cols of $max_cols columns uses $used space.\n"
			if ($debug);

		print STDERR "DBG: Print $fpc files per column\n"
			if ($debug);

		last if ($used <= $w && $cols == $max_cols);
		$fpc = int($l/$cols);
		$fpc++ if ($l % $cols);
		$max_cols = $cols - 1;
	}

	if ($max_cols == 1) {
		$cols = 1;
		$fpc = $l;
	}

	print STDERR "Print out $fpc rows with $cols columns\n"
		if ($debug);

	for (my $i = 0; $i < $fpc; $i++) {

		for (my $j = $i; $j < $fpc*$cols; $j += $fpc) {
			my $cw = $wds[($j - $i)/$fpc];
			my $fmt = "%s%-${cw}s";
			my $file;
			my $r;

			if ($j < @list) {
				$file = $list[$j]->[0];
				my $fdata = $list[$j]->[1];
				$r = ($restore{$fdata->{'fileid'}}) ? '+' : ' ';
			}
			else {
				$file = '';
				$r = ' ';
			}

			print '  ' if ($i != $j);
			printf $fmt, $r, $file;
		}

		print "\n";
	}

}

sub ls_date {
	my $seconds = shift;
	my $date;

	if (abs(time() - $seconds) > 15724800) {
		$date = time2str('%b %e  %Y', $seconds);
	}
	else {
		$date = time2str('%b %e %R', $seconds);
	}

	return $date;
}

# subroutine to load entire bacula database.
=head1 SHELL

Once running, B<recover.pl> will present the user with a shell like
environment where file can be exampled and selected for recover. The
shell will provide command history and editing and if you have the
Gnu readline module installed on your system, it will also provide
command completion. When interacting with files, wildcards should work
as expected.

The following commands are understood.

=cut

sub parse_command {
	my $cstr = shift;
	my @command;
	my $cmd;
	my @args;

	# Nop on blank or commented lines
	return ('nop') if ($cstr =~ /^\s*$/);
	return ('nop') if ($cstr =~ /^\s*#/);

	# Get rid of leading white space to make shellwords work better
	$cstr =~ s/^\s*//;

	($cmd, @args) = shellwords($cstr);

	if (!defined($cmd)) {
		warn "Could not warse $cstr\n";
		return ('nop');
	}

=head2 add [I<filelist>]

Mark I<filelist> for recovery. If I<filelist> is not specified, mark all
files in the current directory. B<mark> is an alias for this command.

=cut
	elsif ($cmd eq 'add' || $cmd eq 'mark') {
		my $options = {};
		@ARGV = @args;

		# Parse ls options
		my $vars = {};
		getopts("aq", $vars) || return ('error', 'Add: Usage add [-q|-a] files');
		$options->{'all'} = $vars->{'a'};
		$options->{'quiet'} =$vars->{'q'}; 


		@command = ('add', $options);

		foreach my $a (@ARGV) {
			push(@command, &expand_files($a));
		}

	}

=head2 bootstrap I<bootstrapfile>

Create a bootstrap file suitable for use with the bacula B<bextract>
command. B<bsr> is an alias for this command.

=cut
	elsif ($cmd eq 'bootstrap' || $cmd eq 'bsr') {
		return ('error', 'bootstrap takes single argument (file to write to)')
			if (@args != 1);
		@command = ('bootstrap', $args[0]);
	}

=head2 cd I<directory>

Allows you to set your current directory. This command understands . for
the current directory and .. for the parent. Also, cd - will change you
back to the previous directory you were in.

=cut
	elsif ($cmd eq 'cd') {
		# Cd with no args goes to /
		@args = ('/') if (!@args);

		if (@args != 1) {
			return ('error', 'Bad cd. cd requires 1 and only 1 argument.');
		}

		my $todir = $args[0];

		# cd - should cd to previous directory. It is handled later.
		return ('cd', '-') if ($todir eq '-');

		# Expand wilecards
		my @e = expand_dirs($todir);

		if (@e > 1) {
			return ('error', 'Bad cd. Wildcard expands to more than 1 dir.');
		}

		$todir = $e[0];

		print STDERR "Initial target is $todir\n" if ($debug);

		# remove prepended .

		while ($todir =~ m|^\./(.*)|) {
			$todir = $1;
			$todir = '.' if (!$todir);
		}

		# If only . is left, replace with current directory.
		$todir = $cwd if ($todir eq '.');
		print STDERR "target after . processing is $todir\n" if ($debug);

		# Now deal with ..
		my $prefix = $cwd;

		while ($todir =~ m|^\.\./(.*)|) {
			$todir = $1;
			print STDERR "DBG: ../ found, new todir - $todir\n" if ($debug);
			$prefix =~ s|/[^/]*/$|/|;
		}

		if ($todir eq '..') {
			$prefix =~ s|/[^/]*/$|/|;
			$todir = '';
		}

		print STDERR "target after .. processing is $todir\n" if ($debug);
		print STDERR "DBG: Final prefix - $prefix\n" if ($debug);

		$todir = "$prefix$todir" if ($prefix ne $cwd);

		print STDERR "DBG: todir after .. handling - $todir\n" if ($debug);

		# Turn relative directories into absolute directories.

		if (substr($todir, 0, 1) ne '/') {
			print STDERR "DBG: $todir has no leading /, prepend $cwd\n" if ($debug);
			$todir = "$cwd$todir";
		}

		# Make sure we have a trailing /

		if (substr($todir, length($todir) - 1) ne '/') {
			print STDERR "DBG: No trailing /, append /\n" if ($debug);
			$todir .= '/';
		}

		@command = ('cd', $todir);
	}

=head2 changetime I<timespec>

This command changes the time used in generating the view of the
filesystem. Files that were backed up before the specified time
(optionally until the next full backup) will be the only files seen.

The time can be specifed in almost any reasonable way. Here are a few
examples:

=over 4

=item 1/1/2006

=item yesterday

=item sunday

=item 5 days ago

=item last month

=back

=cut
	elsif ($cmd eq 'changetime') {
		@command = ($cmd, join(' ', @args));
	}

=head2 client I<clientname> I<jobname>

Specify the client and jobname to view.

=cut
	elsif ($cmd eq 'client') {

		if (@args != 2) {
			return ('error', 'client takes a two arguments client-name job-name');
		}

		@command = ('client', @args);
	}

=head2 debug

Toggle debug flag.

=cut
	elsif ($cmd eq 'debug') {
		@command = ('debug');
	}

=head2 delete [I<filelist>]

Un-mark file that were previous marked for recovery.  If I<filelist> is
not specified, mark all files in the current directory. B<unmark> is an
alias for this command.

=cut
	elsif ($cmd eq 'delete' || $cmd eq 'unmark') {
		@command = ('delete');

		foreach my $a (@args) {
			push(@command, &expand_files($a));
		}

	}

=head2 help

Show list of command with brief description of what they do.

=cut
	elsif ($cmd eq 'help') {
		@command = ('help');
	}

=head2 history

Display command line history. B<h> is an alias for this command.

=cut
	elsif ($cmd eq 'h' || $cmd eq 'history') {
		@command = ('history');
	}

=head2 info [I<filelist>]

Display information about the specified files. The format of the
information provided is reminiscent of the bootstrap file.

=cut
	elsif ($cmd eq 'info') {
		push(@command, 'info');

		foreach my $a (@args) {
			push(@command, &expand_files($a));
		}

	}

=head2 ls [I<filelist>]

This command will list the specified files (defaults to all files in
the current directory). Files are sorted alphabetically be default. It
understand the following options.

=over 4

=item -a

Causes ls to list files even if they are only on backups preceding the
closest full backup to the currently selected date/time.

=item -l

List files in long format (like unix ls command).

=item -r

reverse direction of sort.

=item -S

Sort files by size.

=item -t

Sort files by time

=back

=cut
	elsif ($cmd eq 'ls' || $cmd eq 'dir' || $cmd eq 'll') {
		my $options = {};
		@ARGV = @args;

		# Parse ls options
		my $vars = {};
		getopts("altSr", $vars) || return ('error', 'Bad ls usage.');
		$options->{'all'} = $vars->{'a'};
		$options->{'long'} = $vars->{'l'};
		$options->{'long'} = 1 if ($cmd eq 'dir' || $cmd eq 'll');

		$options->{'sort'} = 'time' if ($vars->{'t'});

		return ('error', 'Only one sort at a time allowed.')
			if ($options->{'sort'} && ($vars->{'S'}));

		$options->{'sort'} = 'size' if ($vars->{'S'});
		$options->{'sort'} = 'alpha' if (!$options->{'sort'});

		$options->{'sort'} = 'r' . $options->{'sort'} if ($vars->{'r'});

		@command = ('ls', $options);

		foreach my $a (@ARGV) {
			push(@command, &expand_files($a));
		}

	}

=head2 pwd

Show current directory.

=cut
	elsif ($cmd eq 'pwd') {
		@command = ('pwd');
	}

=head2 quit

Exit program.

B<q>, B<exit> and B<x> are all aliases for this command.

=cut
	elsif ($cmd eq 'quit' || $cmd eq 'q' || $cmd eq 'exit' || $cmd eq 'x') {
		@command = ('quit');
	}

=head2 recover

This command creates a table in the bacula catalog that case be used to
restore the selected files. It will also display the command to enter
into bconsole to start the restore.

=cut
	elsif ($cmd eq 'recover') {
		@command = ('recover');
	}

=head2 relocate I<directory>

Specify the directory to restore files to. Defaults to /.

=cut
	elsif ($cmd eq 'relocate') {
		return ('error', 'relocate required a single directory to relocate to')
			if (@args != 1);

		my $todir = $args[0];
		$todir = `pwd` . $todir if (substr($todir, 0, 1) ne '/');
		@command = ('relocate', $todir);
	}

=head2 show I<item>

Show various information about B<recover.pl>. The following items can be specified.

=over 4

=item cache

Display's a list of cached directories.

=item catalog

Displays the name of the catalog we are talking to.

=item client

Display current client and job named that are being viewed.

=item restore

Display the number of files and size to be restored.

=item volumes

Display the volumes that will be required to perform a restore on the
selected files.

=back

=cut
	elsif ($cmd eq 'show') {
		return ('error', 'show takes a single argument') if (@args != 1);
		@command = ('show', $args[0]);
	}

=head2 verbose

Toggle verbose flag.

=cut
	elsif ($cmd eq 'verbose') {
		@command = ('verbose');
	}

=head2 versions [I<filelist>]

View all version of specified files available from the current
time. B<ver> is an alias for this command.

=cut
	elsif ($cmd eq 'versions' || $cmd eq 'ver') {
		push(@command, 'versions');

		foreach my $a (@args) {
			push(@command, &expand_files($a));
		}

	}

=head2 volumes

Display the volumes that will be required to perform a restore on the
selected files.

=cut
	elsif ($cmd eq 'volumes') {
		@command = ('volumes');
	}
	else {
		@command = ('error', "$cmd: Unknown command");
	}

	return @command;
}

##############################################################################
### Command processing
##############################################################################

# Add files to restore list.

sub cmd_add {
	my $opts = shift;
	my @flist = @_;

	my $save_rnum = $rnum;
	&select_files(1, $opts, $cwd, @flist);
	print "" . ($rnum - $save_rnum) . " files marked for restore\n";
}

sub cmd_bootstrap {
	my $bsrfile = shift;
	my %jobs;
	my @media;
	my %bootstrap;

	# Get list of job ids to restore from.

	foreach my $fid (keys %restore) {
		$jobs{$restore{$fid}->[0]} = 1;
	}

	my $jlist = join(', ', sort keys %jobs);

	if (!$jlist) {
		print "Nothing to restore.\n";
		return;
	}

	# Read in media info

	my $query = "select
		Job.jobid,
		volumename,
		mediatype,
		volsessionid,
		volsessiontime,
		firstindex,
		lastindex,
		startfile as volfile,
		JobMedia.startblock,
		JobMedia.endblock,
		volindex
	from
		Job,
		Media,
		JobMedia
	where
		Job.jobid in ($jlist) and
		Job.jobid = JobMedia.jobid and
		JobMedia.mediaid = Media.mediaid
	order by
		volumename,
		volsessionid,
		volindex
	";

	my $sth = $dbh->prepare($query) || die "Can't prepare $query\n";
	$sth->execute || die "Can't execute $query\n";

	while (my $ref = $sth->fetchrow_hashref) {
		push(@media, {
			'jobid' => $ref->{'jobid'},
			'volumename' => $ref->{'volumename'},
			'mediatype' => $ref->{'mediatype'},
			'volsessionid' => $ref->{'volsessionid'},
			'volsessiontime' => $ref->{'volsessiontime'},
			'firstindex' => $ref->{'firstindex'},
			'lastindex' => $ref->{'lastindex'},
			'volfile' => $ref->{'volfile'},
			'startblock' => $ref->{'startblock'},
			'endblock' => $ref->{'endblock'},
			'volindex' => $ref->{'volindex'}
		});
	}

# Gather bootstrap info
#
#  key - jobid.volumename.volumesession.volindex
#     job
#     name
#     type
#     session
#     time
#     file
#     startblock
#     endblock
#     array of file indexes.

	for my $info (values %restore) {
		my $jobid = $info->[0];
		my $fidx = $info->[1];

		foreach my $m (@media) {

			if ($jobid == $m->{'jobid'} && $fidx >= $m->{'firstindex'} && $fidx <= $m->{'lastindex'}) {
				my $key = "$jobid.";
				$key .= "$m->{volumename}.$m->{volsessionid}.$m->{volindex}";

				$bootstrap{$key} = {
					'job' => $jobid,
					'name' => $m->{'volumename'},
					'type' => $m->{'mediatype'},
					'session' => $m->{'volsessionid'},
					'index' => $m->{'volindex'},
					'time' => $m->{'volsessiontime'},
					'file' => $m->{'volfile'},
					'startblock' => $m->{'startblock'},
					'endblock' => $m->{'endblock'}
				}
				if (!$bootstrap{$key});

				$bootstrap{$key}->{'files'} = []
					if (!$bootstrap{$key}->{'files'});
				push(@{$bootstrap{$key}->{'files'}}, $fidx);
			}

		}

	}

	# print bootstrap

	print STDERR "DBG: Keys = " . join(', ', keys %bootstrap) . "\n"
		if ($debug);

	my @keys = sort {
		return $bootstrap{$a}->{'time'} <=> $bootstrap{$b}->{'time'}
			if ($bootstrap{$a}->{'time'} != $bootstrap{$b}->{'time'});
		return $bootstrap{$a}->{'name'} cmp $bootstrap{$b}->{'name'}
			if ($bootstrap{$a}->{'name'} ne $bootstrap{$b}->{'name'});
		return $bootstrap{$a}->{'session'} <=> $bootstrap{$b}->{'session'}
			if ($bootstrap{$a}->{'session'} != $bootstrap{$b}->{'session'});
		return $bootstrap{$a}->{'index'} <=> $bootstrap{$b}->{'index'};
	} keys %bootstrap;

	if (!open(BSR, ">$bsrfile")) {
		warn "$bsrfile: $|\n";
		return;
	}

	foreach my $key (@keys) {
		my $info = $bootstrap{$key};
		print BSR "Volume=\"$info->{name}\"\n";
		print BSR "MediaType=\"$info->{type}\"\n";
		print BSR "VolSessionId=$info->{session}\n";
		print BSR "VolSessionTime=$info->{time}\n";
		print BSR "VolFile=$info->{file}\n";
		print BSR "VolBlock=$info->{startblock}-$info->{endblock}\n";

		my @fids = sort { $a <=> $b} @{$bootstrap{$key}->{'files'}};
		my $first;
		my $prev;

		for (my $i = 0; $i < @fids; $i++) {
			$first = $fids[$i] if (!$first);

			if ($prev) {

				if ($fids[$i] != $prev + 1) {
					print BSR "FileIndex=$first";
					print BSR "-$prev" if ($first != $prev);
					print BSR "\n";
					$first = $fids[$i];
				}

			}

			$prev = $fids[$i];
		}

		print BSR "FileIndex=$first";
		print BSR "-$prev" if ($first != $prev);
		print BSR "\n";
		print BSR "Count=" . (@fids) . "\n";
	}

	close(BSR);
}

# Change directory

sub cmd_cd {
	my $dir = shift;

	my $save = $files;

	$dir = $lwd if ($dir eq '-' && defined($lwd));

	if ($dir ne '-') {
		$files = &fetch_dir($dir);
	}
	else {
		warn "Previous director not defined.\n";
	}

	if ($files) {
		$lwd = $cwd;
		$cwd = $dir;
	}
	else {
		print STDERR "Could not locate directory $dir\n";
		$files = $save;
	}

	$cwd = '/' if (!$cwd);
}

sub cmd_changetime {
	my $tstr = shift;

	if (!$tstr) {
		print "Time currently set to " . localtime($rtime) . "\n";
		return;
	}

	my $newtime = parsedate($tstr, FUZZY => 1, PREFER_PAST => 1);

	if (defined($newtime)) {
		print STDERR "Time evaluated to $newtime\n" if ($debug);
		$rtime = $newtime;
		print "Setting date/time to " . localtime($rtime) . "\n";
		&setjob;

		# Clean cache.
		$dircache = {};
		&cache_catalog if ($preload);

		# Get directory based on new time.
		$files = &fetch_dir($cwd);
	}
	else {
		print STDERR "Could not parse $tstr as date/time\n";
	}

}

# Change client

sub cmd_client {
	my $c = shift;
	$jobname = shift;		# Set global job name

	# Lookup client id.
	$client = &lookup_client($c);

	# Clear cache, we changed machines/jobs
	$dircache = {};
	&cache_catalog if ($preload);

	# Find last full backup time.
	&setjob;

	# Get current directory on new client.
	$files = &fetch_dir($cwd);

	# Clear restore info
	$rnum = 0;
	$rbytes = 0;
	%restore = ();
}

sub cmd_debug {
	$debug = 1 - $debug;
}

sub cmd_delete {
	my @flist = @_;
	my $opts = {quiet=>1};

	my $save_rnum = $rnum;
	&select_files(0, $opts, $cwd, @flist);
	print "" . ($save_rnum - $rnum) . " files un-marked for restore\n";
}

sub cmd_help {

	foreach my $h (sort keys %COMMANDS) {
		printf "%-12s %s\n", $h, $COMMANDS{$h};
	}

}

sub cmd_history {

	foreach my $h ($term->GetHistory) {
		print "$h\n";
	}

}

# Print catalog/tape info about files

sub cmd_info {
	my @flist = @_;
	@flist = ($cwd) if (!@flist);

	foreach my $f (@flist) {
		$f =~ s|/+$||;
		my ($fqdir, $dir, $file) = &path_parts($f);
		my $finfo = &fetch_dir($fqdir);

		if (!$finfo->{$file}) {

			if (!$finfo->{"$file/"}) {
				warn "$f: File not found.\n";
				next;
			}

			$file .= '/';
		}

		my $fileid = $finfo->{$file}->{fileid};
		my $fileindex = $finfo->{$file}->{fileindex};
		my $jobid = $finfo->{$file}->{jobid};

		print "#$f -\n";
		print "#FileID   : $finfo->{$file}->{fileid}\n";
		print "#JobID    : $jobid\n";
		print "#Visible  : $finfo->{$file}->{visible}\n";

		my $query = "select
			volumename,
			mediatype,
			volsessionid,
			volsessiontime,
			startfile,
			JobMedia.startblock,
			JobMedia.endblock
		from
			Job,
			Media,
			JobMedia
		where
			Job.jobid = $jobid and
			Job.jobid = JobMedia.jobid and
			$fileindex >= firstindex and
			$fileindex <= lastindex and
			JobMedia.mediaid = Media.mediaid
		";

		my $sth = $dbh->prepare($query) || die "Can't prepare $query\n";
		$sth->execute || die "Can't execute $query\n";

		while (my $ref = $sth->fetchrow_hashref) {
			print "Volume=\"$ref->{volumename}\"\n";
			print "MediaType=\"$ref->{mediatype}\"\n";
			print "VolSessionId=$ref->{volsessionid}\n";
			print "VolSessionTime=$ref->{volsessiontime}\n";
			print "VolFile=$ref->{startfile}\n";
			print "VolBlock=$ref->{startblock}-$ref->{endblock}\n";
			print "FileIndex=$finfo->{$file}->{fileindex}\n";
			print "Count=1\n";
		}

		$sth->finish;
	}

}

# List files.

sub cmd_ls {
	my $opts = shift;
	my @flist = @_;
	my @keys;

	print STDERR "DBG: " . (@flist) . " files to list.\n" if ($debug);

	if (!@flist) {
		@flist = keys %$files;
	}

	# Sort files as specified.

	if ($opts->{sort} eq 'alpha') {
		print STDERR "DBG: Sort by alpha\n" if ($debug);
		@keys = sort @flist;
	}
	elsif ($opts->{sort} eq 'ralpha') {
		print STDERR "DBG: Sort by reverse alpha\n" if ($debug);
		@keys = sort {$b cmp $a} @flist;
	}
	elsif ($opts->{sort} eq 'time') {
		print STDERR "DBG: Sort by time\n" if ($debug);
		@keys = sort {
			return $a cmp $b
				if ($files->{$b}->{'lstat'}->{'st_mtime'} ==
					$files->{$a}->{'lstat'}->{'st_mtime'});
			$files->{$b}->{'lstat'}->{'st_mtime'} <=>
			$files->{$a}->{'lstat'}->{'st_mtime'}
		} @flist;
	}
	elsif ($opts->{sort} eq 'rtime') {
		print STDERR "DBG: Sort by reverse time\n" if ($debug);
		@keys = sort {
			return $b cmp $a
				if ($files->{$a}->{'lstat'}->{'st_mtime'} ==
					$files->{$b}->{'lstat'}->{'st_mtime'});
			$files->{$a}->{'lstat'}->{'st_mtime'} <=>
			$files->{$b}->{'lstat'}->{'st_mtime'}
		} @flist;
	}
	elsif ($opts->{sort} eq 'size') {
		print STDERR "DBG: Sort by size\n" if ($debug);
		@keys = sort {
			return $a cmp $b
				if ($files->{$a}->{'lstat'}->{'st_size'} ==
					$files->{$b}->{'lstat'}->{'st_size'});
			$files->{$b}->{'lstat'}->{'st_size'} <=>
			$files->{$a}->{'lstat'}->{'st_size'}
		} @flist;
	}
	elsif ($opts->{sort} eq 'rsize') {
		print STDERR "DBG: Sort by reverse size\n" if ($debug);
		@keys = sort {
			return $b cmp $a
				if ($files->{$a}->{'lstat'}->{'st_size'} ==
					$files->{$b}->{'lstat'}->{'st_size'});
			$files->{$a}->{'lstat'}->{'st_size'} <=>
			$files->{$b}->{'lstat'}->{'st_size'}
		} @flist;
	}
	else {
		print STDERR "DBG: $opts->{sort}, no sort\n" if ($debug);
		@keys = @flist;
	}

	@flist = ();

	foreach my $f (@keys) {
		print STDERR "DBG: list $f\n" if ($debug);
		$f =~ s|/+$||;
		my ($fqdir, $dir, $file) = &path_parts($f);
		my $finfo = &fetch_dir($fqdir);

		if (!$finfo->{$file}) {

			if (!$finfo->{"$file/"}) {
				warn "$f: File not found.\n";
				next;
			}

			$file .= '/';
		}

		my $fdata = $finfo->{$file};

		if ($opts->{'all'} || $fdata->{'visible'}) {
			push(@flist, ["$dir$file", $fdata]);
		}

	}

	if ($opts->{'long'}) {
		my $lfmt = &long_fmt(\@flist) if ($opts->{'long'});

		foreach my $f (@flist) {
			my $file = $f->[0];
			my $fdata = $f->[1];
			my $r = ($restore{$fdata->{'fileid'}}) ? '+' : ' ';
			my $lstat = $fdata->{'lstat'};

			printf $lfmt, $lstat->{'statstr'}, $lstat->{'st_nlink'},
				$lstat->{'st_uid'}, $lstat->{'st_gid'}, $lstat->{'st_size'},
				ls_date($lstat->{'st_mtime'}), "$r$file";
		}
	}
	else {
		&print_by_cols(@flist);
	}

}

sub cmd_pwd {
	print "$cwd\n";
}

# Create restore data for bconsole

sub cmd_recover {
	my $query = "create table recover (jobid int, fileindex int)";

	$dbh->do($query)
		|| warn "Could not create recover table. Hope it's already there.\n";

	if ($db eq 'postgres') {
		$query = "COPY recover FROM STDIN";

		$dbh->do($query) || die "Can't execute $query\n";

		foreach my $finfo (values %restore) {
			$dbh->pg_putline("$finfo->[0]\t$finfo->[1]\n");
		}

		$dbh->pg_endcopy;
	}
	else {

		foreach my $finfo (values %restore) {
			$query = "insert into recover (
				'jobid', 'fileindex'
			)
			values (
				$finfo->[0], $finfo->[1]
			)";
			$dbh->do($query) || die "Can't execute $query\n";
		}

	}

	$query = "GRANT all on recover to bacula";
	$dbh->do($query) || die "Can't execute $query\n";

	$query = "select name from Client where clientid = $client";
	my $sth = $dbh->prepare($query) || die "Can't prepare $query\n";
	$sth->execute || die "Can't execute $query\n";

	my $ref = $sth->fetchrow_hashref;
	print "Restore prepared. Run bconsole and enter the following command\n";
	print "restore client=$$ref{name} where=$restore_to file=\?recover\n";
	$sth->finish;
}

sub cmd_relocate {
	$restore_to = shift;
}

# Display information about recover's state

sub cmd_show {
	my $what = shift;

	if ($what eq 'clients') {

		foreach my $c (sort keys %$clients) {
			print "$c\n";
		}

	}
	elsif ($what eq 'catalog') {
		print "$catalog\n";
	}
	elsif ($what eq 'client') {
		my $query = "select name from Client where clientid = $client";
		my $sth = $dbh->prepare($query) || die "Can't prepare $query\n";
		$sth->execute || die "Can't execute $query\n";

		my $ref = $sth->fetchrow_hashref;
		print "$$ref{name}; $jobname\n";
		$sth->finish;
	}
	elsif ($what eq 'cache') {
		print "The following directories are cached\n";

		foreach my $d (sort keys %$dircache) {
			print "$d\n";
		}

	}
	elsif ($what eq 'restore') {
		print "There are $rnum files marked for restore.\n";

		print STDERR "DBG: Bytes = $rbytes\n" if ($debug);

		if ($rbytes < 1024) {
			print "The restore will require $rbytes bytes.\n";
		}
		elsif ($rbytes < 1024*1024) {
			my $rk = $rbytes/1024;
			printf "The restore will require %.2f KB.\n", $rk;
		}
		elsif ($rbytes < 1024*1024*1024) {
			my $rm = $rbytes/1024/1024;
			printf "The restore will require %.2f MB.\n", $rm;
		}
		else {
			my $rg = $rbytes/1024/1024/1024;
			printf "The restore will require %.2f GB.\n", $rg;
		}

		print "Restores will be placed in $restore_to\n";
	}
	elsif ($what eq 'volumes') {
		&cmd_volumes;
	}
	elsif ($what eq 'qinfo') {
		my $dl = length($cwd);
		print "? - 1: ftime = $ftime\n";
		print "? - 2: client = $client\n";
		print "? - 3: jobname = $jobname\n";
		print "? - 4: rtime = $rtime\n";
		print "? - 5: dir = $cwd\n";
		print "? - 6, 7: dl = $dl\n";
		print "? - 8: ftime = $ftime\n";
		print "? - 9: client = $client\n";
		print "? - 10: jobname = $jobname\n";
		print "? - 11: rtime = $rtime\n";
		print "? - 12: dir = $cwd\n";
	}
	else {
		warn "Don't know how to show $what\n";
	}

}

sub cmd_verbose {
	$verbose = 1 - $verbose;
}

sub cmd_versions {
	my @flist = @_;

	@flist = ($cwd) if (!@flist);

	foreach my $f (@flist) {
		my $path;
		my $data = {};

		print STDERR "DBG: Get versions for $f\n" if ($debug);

		$f =~ s|/+$||;
		my ($fqdir, $dir, $file) = &path_parts($f);
		my $finfo = &fetch_dir($fqdir);

		if (!$finfo->{$file}) {

			if (!$finfo->{"$file/"}) {
				warn "$f: File not found.\n";
				next;
			}

			$file .= '/';
		}

		if ($file =~ m|/$|) {
			$path = "$fqdir$file";
			$file = '';
		}
		else {
			$path = $fqdir;
		}

		print STDERR "DBG: Use $ftime, $path, $file, $client, $jobname\n"
			if ($debug);

		$ver_sth->execute($ftime, $rtime, $path, $file, $client, $jobname)
			|| die "Can't execute $queries{$db}->{'ver'}\n";

		# Gather stats

		while (my $ref = $ver_sth->fetchrow_hashref) {
			my $f = "$ref->{name};$ref->{jobtdate}";
			$data->{$f} = &create_file_entry(
				$f,
				$ref->{'fileid'},
				$ref->{'fileindex'},
				$ref->{'jobid'},
				$ref->{'visible'},
				$ref->{'lstat'}
			);

			$data->{$f}->{'jobtdate'} = $ref->{'jobtdate'};
			$data->{$f}->{'volume'} = $ref->{'volumename'};
		}

		my @keys = sort {
			$data->{$a}->{'jobtdate'} <=>
			$data->{$b}->{'jobtdate'}
		} keys %$data;

		my @list = ();

		foreach my $f (@keys) {
			push(@list, [$file, $data->{$f}]);
		}

		my $lfmt = &long_fmt(\@list);
		print "\nVersions of \`$path$file' earlier than ";
		print localtime($rtime) . ":\n\n";

		foreach my $f (@keys) {
			my $lstat = $data->{$f}->{'lstat'};
			printf $lfmt, $lstat->{'statstr'}, $lstat->{'st_nlink'},
				$lstat->{'st_uid'}, $lstat->{'st_gid'}, $lstat->{'st_size'},
				time2str('%c', $lstat->{'st_mtime'}), $file;
			print "save time: " . localtime($data->{$f}->{'jobtdate'}) . "\n";
			print " location: $data->{$f}->{volume}\n\n";
		}

	}

}

# List volumes needed for restore.

sub cmd_volumes {
	my %media;
	my @jobmedia;
	my %volumes;

	# Get media.
	my $query = "select mediaid, volumename from Media";
	my $sth = $dbh->prepare($query) || die "Can't prepare $query\n";

	$sth->execute || die "Can't execute $query\n";

	while (my $ref = $sth->fetchrow_hashref) {
		$media{$$ref{'mediaid'}} = $$ref{'volumename'};
	}

	$sth->finish();

	# Get media usage.
	$query = "select mediaid, jobid, firstindex, lastindex from JobMedia";
	$sth = $dbh->prepare($query) || die "Can't prepare $query\n";

	$sth->execute || die "Can't execute $query\n";

	while (my $ref = $sth->fetchrow_hashref) {
		push(@jobmedia, {
			'mediaid' => $$ref{'mediaid'},
			'jobid' => $$ref{'jobid'},
			'firstindex' => $$ref{'firstindex'},
			'lastindex' => $$ref{'lastindex'}
		});
	}

	$sth->finish();

	# Find needed volumes

	foreach my $fileid (keys %restore) {
		my ($jobid, $idx) = @{$restore{$fileid}};

		foreach my $jm (@jobmedia) {
			next if ($jm->{'jobid'}) != $jobid;

			if ($idx >= $jm->{'firstindex'} && $idx <= $jm->{'lastindex'}) {
				$volumes{$media{$jm->{'mediaid'}}} = 1;
			}

		}

	}

	print "The following volumes are needed for restore.\n";

	foreach my $v (sort keys %volumes) {
		print "$v\n";
	}

}

sub cmd_error {
	my $msg = shift;
	print STDERR "$msg\n";
}

##############################################################################
### Start of program
##############################################################################

&cache_catalog if ($preload);

print "Using $readline for command processing\n" if ($verbose);

# Initialize command completion

# Add binding for Perl readline. Issue warning.
if ($readline eq 'Term::ReadLine::Gnu') {
	$term->ReadHistory($HIST_FILE);
	print STDERR "DBG: FCD - $tty_attribs->{filename_completion_desired}\n"
		if ($debug);
	$tty_attribs->{attempted_completion_function} = \&complete;
	$tty_attribs->{attempted_completion_function} = \&complete;
	print STDERR "DBG: Quote chars = '$tty_attribs->{filename_quote_characters}'\n" if ($debug);
}
elsif ($readline eq 'Term::ReadLine::Perl') {
	readline::rl_bind('TAB', 'ViComplete');
	warn "Command completion disabled. $readline is seriously broken\n";
}
else {
	warn "Can't deal with $readline, Command completion disabled.\n";
}

&cmd_cd($start_dir);

while (defined($cstr = $term->readline('recover> '))) {
	print "\n" if ($readline eq 'Term::ReadLine::Perl');
	my @command = parse_command($cstr);
	last if ($command[0] eq 'quit');
	next if ($command[0] eq 'nop');

	print STDERR "Execute $command[0] command.\n" if ($debug);

	my $cmd = \&{"cmd_$command[0]"};

	# The following line will call the subroutine named cmd_ prepended to
	# the name of the command returned by parse_command.

	&$cmd(@command[1..$#command]);
};

$dir_sth->finish();
$sel_sth->finish();
$ver_sth->finish();
$dbh->disconnect();

print "\n" if (!defined($cstr));

$term->WriteHistory($HIST_FILE) if ($readline eq 'Term::ReadLine::Gnu');

=head1 DEPENDENCIES

The following CPAN modules are required to run this program.

DBI, Term::ReadKey, Time::ParseDate, Date::Format, Text::ParseWords

Additionally, you will only get command line completion if you also have

Term::ReadLine::Gnu

=head1 AUTHOR

Karl Hakimian <hakimian@aha.com>

=head1 LICENSE

Copyright (C) 2006 Karl Hakimian

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

=cut
