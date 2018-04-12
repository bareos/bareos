#!/usr/bin/perl -w

use strict;
use Getopt::Long;

# Converts the image given in the first argument into
#  the format of the second argument. Decides how to do it
#  based on the filenames of the source and destination
#

my $usage =
"Usage: image_convert.pl [--scale scale_filename] [--res resolution] source_file destination_file\n ";

# A global hash to hold command-line arguments.
my $cliArgs = {};
my $DEFAULT_SCALE = 0.75;
my $DEFAULT_MAXX = 5;
my $DEFAULT_MAXY = 8;
my $DEFAULT_RESOLUTION = 72;

sub max { return $_[0] > $_[1] ? $_[0] : $_[1]; }
sub min { return $_[0] < $_[1] ? $_[0] : $_[1]; }

sub getIMGdetails {
	# Takes the image filename as the only argument, returns the image
	#  size in inches.
	# If the identify program is available:
	#  Determine the current geometry and resolution of the image.
	#  Return the image size in inches.
	# Otherwize return 0,0.
	my ($img) = shift;

	# # If the identify program is not available, bail.
	(`which identify 2>\&1` =~ /^which:/) and
		die "identify program (part of Imagemagick) not available -- aborting\n";

	my $attrib = {};

	# Determine the size of the existing image.
	my $response = `identify -verbose $img`;
	if ($response =~ /^identify: no decode delegate/ or
		!$response) {
		return 0;
	}
	($attrib->{format}) =
		$response =~ /^\s*Format:\s+(\S*)\s+/m;
	($attrib->{hdots},$attrib->{vdots}) =
		$response =~ /^\s*Geometry:\s*([\d\.]*)x([\d\.]*)/m;
	($attrib->{hres},$attrib->{vres}) =
		$response =~ /^\s*Resolution:\s*([\d\.]*)x([\d\.]*)/m;
	$attrib->{hsize} = $attrib->{hdots} / $attrib->{hres};
	$attrib->{vsize} = $attrib->{vdots} / $attrib->{vres};
	return $attrib;
}

sub doConversion {
	my ($cli,$scales) = @_;
	my ($attrib,$ourScale,$newsizex,$newsizey);

	# Get the input file details.
	$attrib = getIMGdetails($cli->{infile});
	if (ref($attrib) ne 'HASH') {
		# Identify failed.
		die "Failed to Identify Input Image File $cli->{infile}.  Aborting\n";
	}

	# If the scale factor is defined in the file of scale factors,
	#  use that.  Otherwise use the default scale provided on the command line,
	#  If none was provided, use 0.75.
	if (defined($scales->{$cli->{infile}})) {
		$ourScale = $scales->{$cli->{infile}};
	} else {
		if (defined $cli->{defaultscale}) {
			$ourScale = $cli->{defaultscale};
		} else { $ourScale = $DEFAULT_SCALE; }

		# If after scaling the image is still too big, scale it to the max
		#  size. Otherwise, scale it according to the scale factor.
		if ($attrib->{hsize} * $ourScale > $cli->{maxx} or
				$attrib->{vsize} * $ourScale > $cli->{maxy}) {
			$ourScale /= max($attrib->{hsize} * $ourScale / $cli->{maxx},
				$attrib->{vsize} * $ourScale / $cli->{maxy});
		}
	}

	$newsizex = $attrib->{hsize} * $ourScale;
	$newsizey = $attrib->{vsize} * $ourScale;

	# Get strings ready for the convert command.
	my $cdots = sprintf "%dx%d",$newsizex * $cli->{resolution},
		$newsizey * $cli->{resolution};
	my $cres = sprintf "%dx%d",$cli->{resolution},$cli->{resolution};

	# Do the conversion...
	printf "Scaling by %.5f : %s -> %s\n",$ourScale,$cli->{infile},$cli->{outfile};
	print STDERR `convert $cli->{infile} -resize $cdots -density $cres $cli->{outfile}`;

#	if ($cli->{infile} =~ /.*\.png$/ and $cli->{outfile} =~ /.*\.pdf$/) {
#		printf "Scaling %s to %s with scale %.3f\n",$cli->{infile},$cli->{outfile},$ourScale;
#		print STDERR `pngtopnm $cli->{infile} | pnmtops -scale=$ourScale -noturn -nosetpage | epstopdf --filter > $cli->{outfile}`;
#		pngtopnm $${i}.png | pnmtops -scale=0.85 --noturn -nosetpage | epstopdf --filter >$${i}.pdf ;
#	} elsif ($cli->{infile} =~ /.*\.png$/ and $cli->{outfile} =~ /.*\.eps$/) {
#		printf "Scaling %s to %s with scale %.3f\n",$cli->{infile},$cli->{outfile},$ourScale;
#		print STDERR `pngtopnm $cli->{infile} | pnmtops -scale=$ourScale -noturn -nosetpage > $cli->{outfile}`;
#		pngtopnm $${i}.png | pnmtops -scale=0.65 --noturn -nosetpage >$${i}.eps;
#	} elsif ($cli->{infile} =~ /.*\.jpg$/ and $cli->{outfile} =~ /.*\.png$/) {
#		printf "Converting %s to %s\n",$cli->{infile},$cli->{outfile};
#		print STDERR `jpegtopnm $cli->{infile} | pnmtopng > $cli->{outfile}`;
#		jpegtopnm $${i}.jpg | pnmtopng >$${i}.png ;
#	} elsif ($cli->{infile} =~ /.*\.gif$/ and $cli->{outfile} =~ /.*\.png$/) {
#		printf "Converting %s to %s\n",$cli->{infile},$cli->{outfile};
#		print STDERR `giftopnm $cli->{infile} | pnmtopng > $cli->{outfile}`;
#		giftopnm $${i}.gif | pnmtopng >$${i}.png ;
#	} else {
#		die "Unsupported Conversion: $cli->{infile} to $cli->{outfile}\n";
#	}

	# Save the image scale for putting into the file of scales.
	$scales->{$cli->{infile}} = $ourScale;
}

sub loadImgScales {
	# Loads the image scales from the specified file into the
	#  global hash %imgScales
	my $scaleFile = shift;
	my ($imagename,$scale,$cnt);
	my %imgScales;

	if (!open (IF,"<$scaleFile")) {
		warn "File of Image Scales not Found.  Will create one when needed.\n";
	} else {
		$cnt = 0;
		while (<IF>) {
			$cnt++;
			chomp;
			my ($imagename,$scale) = split /\s+/;
			if (!defined $imagename or !defined $scale or
				$scale !~ /^[\-\+\d\.]+$/ or $scale < 0.01 or $scale > 100) {
				print STDERR "Invalid Image Scale Entry at line $cnt in $scaleFile\n";
			} else {
				$imgScales{$imagename} = $scale;
			}
		}
		close IF;
	}
	return { %imgScales };
}

sub writeImgScales {
	my ($fileName,$scales) = @_;

	open OF,">$fileName" or die "Cannot open scale file $fileName for output\n";
	foreach (sort(keys(%{$scales}))) { printf OF "%s %.5f\n",$_,$scales->{$_}; }
	close OF;
}


sub getCliArgs {
	# Sets up the parameters and gets the command-line
	#  options. $args is a pointer to a hash.
	my $args = shift;
	my ($infile,$outfile,@filenames);

	# Establish defaults:
	$args->{defaultscale} = $DEFAULT_SCALE;
	$args->{maxx} = $DEFAULT_MAXX;
	$args->{maxy} = $DEFAULT_MAXY;
	$args->{resolution} = $DEFAULT_RESOLUTION;
	GetOptions( "scalefile=s" => \$args->{scalefile},
				"defaultscale=f" => \$args->{defaultscale},
				"resolution=i" => \$args->{resolution},
				"maxx=f" => \$args->{maxx},
				"maxy=f" => \$args->{maxy},
			) or die $usage;
	# There should be just the input and output filenames and
	#  nothing more in @filenames
	$args->{infile} = shift @ARGV;
	$args->{outfile} = shift @ARGV;
	if (!defined $args->{outfile}) { die $usage; }
}

###############################################
############## MAIN ###########################
###############################################

my ($scales);

# Get the command-line arguments into the global hash.
getCliArgs($cliArgs);

#foreach (sort(keys(%$cliArgs))) {
#	print "$_ $cliArgs->{$_}\n";
#}

if (! -e $cliArgs->{infile}) {die "Input File not Found: $cliArgs->{infile}\n";}

# Load in the image conversion scales from the file of scale factors.
if (defined $cliArgs->{scalefile}) {
	if (-e $cliArgs->{scalefile}) {
		$scales = loadImgScales($cliArgs->{scalefile});
	} else {
		warn "File of Image Scales not Found.  Will create one when needed.\n";
	}
}


# Process the file.
doConversion($cliArgs,$scales);

# Write the file of scale factors.
writeImgScales($cliArgs->{scalefile},$scales);
