#!/usr/bin/perl -w
# Fixes various things within tex files.

use strict;

my %args;


sub get_includes {
	# Get a list of include files from the top-level tex file.
	my (@list,$file);
	
	foreach my $filename (@_) {
		$filename or next;
		# Start with the top-level latex file so it gets checked too.
		push (@list,$filename);

		# Get a list of all the html files in the directory.
		open IF,"<$filename" or die "Cannot open input file $filename";
		while (<IF>) {
			chomp;
			push @list,"$1.tex" if (/\\include\{(.*?)\}/);
		}

		close IF;
	}
	return @list;
}

sub convert_files {
	my (@files) = @_;
	my ($linecnt,$filedata,$output,$itemcnt,$indentcnt,$cnt);
	
	$cnt = 0;
	foreach my $file (@files) {
		# Open the file and load the whole thing into $filedata. A bit wasteful but
		#   easier to deal with, and we don't have a problem with speed here.
		$filedata = "";
		open IF,"<$file" or die "Cannot open input file $file";
		while (<IF>) {
			$filedata .= $_;
		}
		close IF;
		
		# We look for a line that starts with \item, and indent the two next lines (if not blank)
		#  by three spaces.
		my $linecnt = 3;
		$indentcnt = 0;
		$output = "";
		# Process a line at a time.
		foreach (split(/\n/,$filedata)) {
			$_ .= "\n"; # Put back the return.
			# If this line is less than the third line past the \item command,
			#  and the line isn't blank and doesn't start with whitespace
			#  add three spaces to the start of the line. Keep track of the number
			#  of lines changed.
			if ($linecnt < 3 and !/^\\item/) {
				if (/^[^\n\s]/) {
					$output .= "   " . $_;
					$indentcnt++;
				} else {
					$output .= $_;
				}
				$linecnt++;
			} else {
				$linecnt = 3;
				$output .= $_;
			}
			/^\\item / and $linecnt = 1;
		}

		
		# This is an item line.  We need to process it too. If inside a \begin{description} environment, convert 
		#  \item {\bf xxx} to \item [xxx] or \item [{xxx}] (if xxx contains '[' or ']'.
		$itemcnt = 0;
		$filedata = $output;
		$output = "";
		my ($before,$descrip,$this,$between);

		# Find any \begin{description} environment
		while ($filedata =~ /(\\begin[\s\n]*\{[\s\n]*description[\s\n]*\})(.*?)(\\end[\s\n]*\{[\s\n]*description[\s\n]*\})/s) {
			$output .= $` . $1;
			$filedata = $3 . $';
			$descrip = $2;

			# Search for \item {\bf xxx}
			while ($descrip =~ /\\item[\s\n]*\{[\s\n]*\\bf[\s\n]*/s) {
				$descrip = $';
				$output .= $`;
				($between,$descrip) = find_matching_brace($descrip);
				if (!$descrip) {
					$linecnt = $output =~ tr/\n/\n/;
					print STDERR "Missing matching curly brace at line $linecnt in $file\n" if (!$descrip);
				}

				# Now do the replacement.
				$between = '{' . $between . '}' if ($between =~ /\[|\]/);
				$output .= "\\item \[$between\]";	
				$itemcnt++;
			}
			$output .= $descrip;
		}
		$output .= $filedata;
	
		# If any hyphens or \item commnads were converted, save the file.
		if ($indentcnt or $itemcnt) {
			open OF,">$file" or die "Cannot open output file $file";
			print OF $output;
			close OF;
			print "$indentcnt indent", ($indentcnt == 1) ? "" : "s"," added in $file\n";
			print "$itemcnt item", ($itemcnt == 1) ? "" : "s"," Changed in $file\n";
		}

		$cnt += $indentcnt + $itemcnt;
	}
	return $cnt;
}

sub find_matching_brace {
	# Finds text up to the next matching brace.  Assumes that the input text doesn't contain
	#  the opening brace, but we want to find text up to a matching closing one.
	# Returns the text between the matching braces, followed by the rest of the text following
	#   (which does not include the matching brace).
	# 
	my $str = shift;
	my ($this,$temp);
	my $cnt = 1;

	while ($cnt) {
		# Ignore verbatim constructs involving curly braces, or if the character preceding
		#  the curly brace is a backslash.
		if ($str =~ /\\verb\*?\{.*?\{|\\verb\*?\}.*?\}|\{|\}/s) {
			$this .= $`;
			$str = $';
			$temp = $&;

			if ((substr($this,-1,1) eq '\\') or 
				$temp =~ /^\\verb/) {
					$this .= $temp;
					next;
			}
				
			$cnt +=  ($temp eq '{') ? 1 : -1;
			# If this isn't the matching curly brace ($cnt > 0), include the brace.
			$this .= $temp if ($cnt);
		} else {
			# No matching curly brace found.
			return ($this . $str,'');
		}
	}
	return ($this,$str);
}

sub check_arguments {
	# Checks command-line arguments for ones starting with --  puts them into
	#   a hash called %args and removes them from @ARGV.
	my $args = shift;
	my $i;

	for ($i = 0; $i < $#ARGV; $i++) {
		$ARGV[$i] =~ /^\-+/ or next;
		$ARGV[$i] =~ s/^\-+//;
		$args{$ARGV[$i]} = "";
		delete ($ARGV[$i]);
		
	}
}

##################################################################
#                       MAIN                                  ####
##################################################################

my @includes;
my $cnt;

check_arguments(\%args);
die "No Files given to Check\n" if ($#ARGV < 0);

# Examine the file pointed to by the first argument to get a list of 
#  includes to test.
@includes = get_includes(@ARGV);

$cnt = convert_files(@includes);
print "No lines changed\n" unless $cnt;
