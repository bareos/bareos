#!/usr/bin/perl -w
# Finds potential problems in tex files, and issues warnings to the console
#   about what it finds.  Takes a list of files as its only arguments,
#   and does checks on all the files listed.  The assumption is that these are
#   valid (or close to valid) LaTeX files.  It follows \include statements
#   recursively to pick up any included tex files.
#
#
#
# Currently the following checks are made:
#
#   -- Multiple hyphens not inside a verbatim environment (or \verb).  These
#      should be placed inside a \verb{} contruct so they will not be converted
#      to single hyphen by latex and latex2html.


# Original creation 3-8-05 by Karl Cunningham  karlc -at- keckec -dot- com
#
#

use strict;

# The following builds the test string to identify and change multiple
#   hyphens in the tex files.  Several constructs are identified but only
#   multiple hyphens are changed; the others are fed to the output 
#   unchanged.
my $b = '\\\\begin\\*?\\s*\\{\\s*';  # \begin{
my $e = '\\\\end\\*?\\s*\\{\\s*';    # \end{
my $c = '\\s*\\}';                   # closing curly brace

# This captures entire verbatim environments.  These are passed to the output
#   file unchanged.
my $verbatimenv = $b . "verbatim" . $c . ".*?" . $e . "verbatim" . $c;  

# This captures \verb{..{ constructs.  They are passed to the output unchanged.
my $verb = '\\\\verb\\*?(.).*?\\1';

# This captures multiple hyphens with a leading and trailing space.  These are not changed.
my $hyphsp = '\\s\\-{2,}\\s';

# This identifies other multiple hyphens.
my $hyphens = '\\-{2,}';

# This identifies \hyperpage{..} commands, which should be ignored.
my $hyperpage = '\\\\hyperpage\\*?\\{.*?\\}';

# This builds the actual test string from the above strings.
#my $teststr = "$verbatimenv|$verb|$tocentry|$hyphens";
my $teststr = "$verbatimenv|$verb|$hyphsp|$hyperpage|$hyphens";


sub get_includes {
	# Get a list of include files from the top-level tex file.  The first
	#   argument is a pointer to the list of files found. The rest of the 
	#   arguments is a list of filenames to check for includes.
	my $files = shift;
	my ($fileline,$includefile,$includes);

	while (my $filename = shift) {
		# Get a list of all the html files in the directory.
		open my $if,"<$filename" or die "Cannot open input file $filename\n";
		$fileline = 0;
		$includes = 0;
		while (<$if>) {
			chomp;
			$fileline++;
			# If a file is found in an include, process it.
			if (($includefile) = /\\include\s*\{(.*?)\}/) {
				$includes++;
				# Append .tex to the filename
				$includefile .= '.tex';

				# If the include file has already been processed, issue a warning 
				#   and don't do it again.
				my $found = 0;
				foreach (@$files) {
					if ($_ eq $includefile) {
						$found = 1;
						last;
					}
				}
				if ($found) {
					print "$includefile found at line $fileline in $filename was previously included\n";
				} else {
					# The file has not been previously found.  Save it and
					# 	recursively process it.
					push (@$files,$includefile);
					get_includes($files,$includefile);
				}
			}
		}
		close IF;
	}
}


sub check_hyphens {
	my (@files) = @_;
	my ($filedata,$this,$linecnt,$before);
	
	# Build the test string to check for the various environments.
	#   We only do the conversion if the multiple hyphens are outside of a 
	#   verbatim environment (either \begin{verbatim}...\end{verbatim} or 
	#   \verb{--}).  Capture those environments and pass them to the output
	#   unchanged.

	foreach my $file (@files) {
		# Open the file and load the whole thing into $filedata. A bit wasteful but
		#   easier to deal with, and we don't have a problem with speed here.
		$filedata = "";
		open IF,"<$file" or die "Cannot open input file $file";
		while (<IF>) {
			$filedata .= $_;
		}
		close IF;
		
		# Set up to process the file data.
		$linecnt = 1;

		# Go through the file data from beginning to end.  For each match, save what
		#   came before it and what matched.  $filedata now becomes only what came 
		#   after the match.
		#   Chech the match to see if it starts with a multiple-hyphen.  If so
		#     warn the user.  Keep track of line numbers so they can be output
		#     with the warning message.
		while ($filedata =~ /$teststr/os) {
			$this = $&;
			$before = $`;
			$filedata = $';
			$linecnt += $before =~ tr/\n/\n/;

			# Check if the multiple hyphen is present outside of one of the 
			#   acceptable constructs.
			if ($this =~ /^\-+/) {
				print "Possible unwanted multiple hyphen found in line ",
					"$linecnt of file $file\n";
			}
			$linecnt += $this =~ tr/\n/\n/;
		}
	}
}
##################################################################
#                       MAIN                                  ####
##################################################################

my (@includes,$cnt);

# Examine the file pointed to by the first argument to get a list of 
#  includes to test.
get_includes(\@includes,@ARGV);

check_hyphens(@includes);
