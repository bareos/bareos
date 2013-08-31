# This module does multiple indices, supporting the style of the LaTex 'index'
#  package.

# Version Information:
#  16-Feb-2005 -- Original Creation.  Karl E. Cunningham
#  14-Mar-2005 -- Clarified and Consolodated some of the code.
#                   Changed to smoothly handle single and multiple indices.

# Two LaTeX index formats are supported...
#  --- SINGLE INDEX ---
#  \usepackage{makeidx}
#  \makeindex
#  \index{entry1}
#  \index{entry2}
#  \index{entry3}
#    ...
#  \printindex
#
#  --- MULTIPLE INDICES ---
#
#  \usepackage{makeidx}
#  \usepackage{index}
#  \makeindex   -- latex2html doesn't care but LaTeX does.
#  \newindex{ref1}{ext1}{ext2}{title1}
#  \newindex{ref2}{ext1}{ext2}{title2}
#  \newindex{ref3}{ext1}{ext2}{title3}
#  \index[ref1]{entry1}
#  \index[ref1]{entry2}
#  \index[ref3]{entry3}
#  \index[ref2]{entry4}
#  \index{entry5}
#  \index[ref3]{entry6}
#    ...
#  \printindex[ref1]
#  \printindex[ref2]
#  \printindex[ref3]
#  \printindex
#  ___________________
#
#  For the multiple-index  style, each index is identified by the ref argument to \newindex, \index, 
#   and \printindex. A default index is allowed, which is indicated by omitting the optional
#   argument.  The default index does not require a \newindex command.  As \index commands 
#   are encountered, their entries are stored according
#   to the ref argument.  When the \printindex command is encountered, the stored index
#   entries for that argument are retrieved and printed. The title for each index is taken
#   from the last argument in the \newindex command.
#  While processing \index and \printindex commands, if no argument is given the index entries
#   are built into a default index.  The title of the default index is simply "Index".  
#   This makes the difference between single- and multiple-index processing trivial.
# 
#  Another method can be used by omitting the \printindex command and just using \include to 
#   pull in index files created by the makeindex program. These files will start with
#   \begin{theindex}.  This command is used to determine where to print the index.  Using this
#   approach, the indices will be output in the same order as the newindex commands were 
#   originally found (see below).  Using a combination of \printindex and \include{indexfile} has not
#   been tested and may produce undesireable results.
#
#  The index data are stored in a hash for later sorting and output.  As \printindex
#   commands are handled, the order in which they were found in the tex filea is saved,
#   associated with the ref argument to \printindex.
#
#  We use the original %index hash to store the index data into.  We append a \002 followed by the
#    name of the index to isolate the entries in different indices from each other.  This is necessary
#    so that different indices can have entries with the same name.  For the default index, the \002 is
#    appended without the name.
#
#  Since the index order in the output cannot be determined if the \include{indexfile} 
#   command is used, the order will be assumed from the order in which the \newindex
#   commands were originally seen in the TeX files.  This order is saved as well as the
#   order determined from any printindex{ref} commands.  If \printindex commnads are used
#   to specify the index output, that order will be used.  If the \include{idxfile} command
#   is used, the order of the original newindex commands will be used.  In this case the
#   default index will be printed last since it doesn't have a corresponding \newindex
#   command and its order cannot be determined.  Mixing \printindex and \include{idxfile} 
#   commands in the same file is likely to produce less than satisfactory results.
#   
#  
#  The hash containing index data is named %indices.  It contains the following data:
#{
#	'title' => { 
#                $ref1 => $indextitle ,
#                $ref2 => $indextitle ,
#                ...
#              },
#	'newcmdorder' => [ ref1, ref2, ..., * ], # asterisk indicates the position of the default index.
#	'printindorder' => [ ref1, ref2, ..., * ], # asterisk indicates the position of the default index.
#}
	

# Globals to handle multiple indices.
my %indices;

# This tells the system to use up to 7 words in index entries.
$WORDS_IN_INDEX = 10;

# KEC 2-18-05
# Handles the \newindex command.  This is called if the \newindex command is 
#  encountered in the LaTex source.  Gets the index ref and title from the arguments.
#  Saves the index ref and title.
#  Note that we are called once to handle multiple \newindex commands that are 
#    newline-separated.
sub do_cmd_newindex {
	my $data = shift;
	# The data is sent to us as fields delimited by their ID #'s.  We extract the 
	#  fields.
	foreach my $line (split("\n",$data)) {
		my @fields = split (/(?:\<\#\d+?\#\>)+/,$line);

		# The index name and title are the second and fourth fields in the data.
		if ($line =~ /^</ or $line =~ /^\\newindex/) {
			my ($indexref,$indextitle) = ($fields[1],$fields[4]);
			$indices{'title'}{$indexref} = $indextitle;
			push (@{$indices{'newcmdorder'}},$indexref);
		}
	}
}


# KEC -- Copied from makeidx.perl and modified to do multiple indices.
# Processes an \index entry from the LaTex file.  
#  Gets the optional argument from the index command, which is the name of the index
#    into which to place the entry.
#  Drops the brackets from the index_name
#  Puts the index entry into the html stream
#  Creates the tokenized index entry (which also saves the index entry info
sub do_real_index {
	local($_) = @_;
	local($pat,$idx_entry,$index_name);
	# catches opt-arg from \index commands for index.sty
	$index_name = &get_next_optional_argument;
	$index_name = "" unless defined $index_name;
	# Drop leading and trailing brackets from the index name.
	$index_name =~ s/^\[|\]$//g;

	$idx_entry = &missing_braces unless (
		(s/$next_pair_pr_rx/$pat=$1;$idx_entry=$2;''/e)
		||(s/$next_pair_rx/$pat=$1;$idx_entry=$2;''/e));

	if ($index_name and defined $idx_entry and 
		!defined $indices{'title'}{$index_name}) {
			print STDERR "\nInvalid Index Name: \\index \[$index_name\]\{$idx_entry\}\n";
	}

	$idx_entry = &named_index_entry($pat, $idx_entry,$index_name);
	$idx_entry.$_;
}

# Creates and saves an index entry in the index hashes.
#  Modified to do multiple indices. 
#    Creates an index_key that allows index entries to have the same characteristics but be in
#      different indices. This index_key is the regular key with the index name appended.
#    Save the index order for the entry in the %index_order hash.
sub named_index_entry {
    local($br_id, $str, $index_name) = @_;
	my ($index_key);
    # escape the quoting etc characters
    # ! -> \001
    # @ -> \002
    # | -> \003
    $* = 1; $str =~ s/\n\s*/ /g; $* = 0; # remove any newlines
    # protect \001 occurring with images
    $str =~ s/\001/\016/g;			# 0x1 to 0xF
    $str =~ s/\\\\/\011/g; 			# Double backslash -> 0xB
    $str =~ s/\\;SPMquot;/\012/g; 	# \;SPMquot; -> 0xC
    $str =~ s/;SPMquot;!/\013/g; 	# ;SPMquot; -> 0xD
    $str =~ s/!/\001/g; 			# Exclamation point -> 0x1
    $str =~ s/\013/!/g; 			# 0xD -> Exclaimation point
    $str =~ s/;SPMquot;@/\015/g; 	# ;SPMquot;@ to 0xF
    $str =~ s/@/\002/g; 			# At sign -> 0x2
    $str =~ s/\015/@/g; 			# 0xF to At sign
    $str =~ s/;SPMquot;\|/\017/g; 	# ;SMPquot;| to 0x11
    $str =~ s/\|/\003/g; 			# Vertical line to 0x3
    $str =~ s/\017/|/g; 			# 0x11 to vertical line
    $str =~ s/;SPMquot;(.)/\1/g; 	# ;SPMquot; -> whatever the next character is
    $str =~ s/\012/;SPMquot;/g; 	# 0x12 to ;SPMquot;
    $str =~ s/\011/\\\\/g; 			# 0x11 to double backslash
    local($key_part, $pageref) = split("\003", $str, 2);

	# For any keys of the form: blablabla!blablabla, which want to be split at the
	#   exclamation point, replace the ! with a comma and a space.  We don't do it
	#   that way for this index.
	$key_part =~ s/\001/, /g;
    local(@keys) = split("\001", $key_part);
    # If TITLE is not yet available use $before.
    $TITLE = $saved_title if (($saved_title)&&(!($TITLE)||($TITLE eq $default_title)));
    $TITLE = $before unless $TITLE;
    # Save the reference
    local($words) = '';
    if ($SHOW_SECTION_NUMBERS) { $words = &make_idxnum; }
		elsif ($SHORT_INDEX) { $words = &make_shortidxname; }
		else { $words = &make_idxname; }
    local($super_key) = '';
    local($sort_key, $printable_key, $cur_key);
    foreach $key (@keys) {
		$key =~ s/\016/\001/g; # revert protected \001s
		($sort_key, $printable_key) = split("\002", $key);
		#
		# RRM:  16 May 1996
		# any \label in the printable-key will have already
		# created a label where the \index occurred.
		# This has to be removed, so that the desired label 
		# will be found on the Index page instead. 
		#
		if ($printable_key =~ /tex2html_anchor_mark/ ) {
			$printable_key =~ s/><tex2html_anchor_mark><\/A><A//g;
			local($tmpA,$tmpB) = split("NAME=\"", $printable_key);
			($tmpA,$tmpB) = split("\"", $tmpB);
			$ref_files{$tmpA}='';
			$index_labels{$tmpA} = 1;
		}
		#
		# resolve and clean-up the hyperlink index-entries 
		# so they can be saved in an  index.pl  file
		#
		if ($printable_key =~ /$cross_ref_mark/ ) {
			local($label,$id,$ref_label);
	#	    $printable_key =~ s/$cross_ref_mark#(\w+)#(\w+)>$cross_ref_mark/
			$printable_key =~ s/$cross_ref_mark#([^#]+)#([^>]+)>$cross_ref_mark/
				do { ($label,$id) = ($1,$2);
				$ref_label = $external_labels{$label} unless
				($ref_label = $ref_files{$label});
				'"' . "$ref_label#$label" . '">' .
				&get_ref_mark($label,$id)}
			/geo;
		}
		$printable_key =~ s/<\#[^\#>]*\#>//go;
		#RRM
		# recognise \char combinations, for a \backslash
		#
		$printable_key =~ s/\&\#;\'134/\\/g;		# restore \\s
		$printable_key =~ s/\&\#;\`<BR> /\\/g;	#  ditto
		$printable_key =~ s/\&\#;*SPMquot;92/\\/g;	#  ditto
		#
		#	$sort_key .= "@$printable_key" if !($printable_key);	# RRM
		$sort_key .= "@$printable_key" if !($sort_key);	# RRM
		$sort_key =~ tr/A-Z/a-z/;
		if ($super_key) {
			$cur_key = $super_key . "\001" . $sort_key;
			$sub_index{$super_key} .= $cur_key . "\004";
		} else {
			$cur_key = $sort_key;
		}

		# Append the $index_name to the current key with a \002 delimiter.  This will 
		#  allow the same index entry to appear in more than one index.
		$index_key = $cur_key . "\002$index_name";

		$index{$index_key} .= "";  

		#
		# RRM,  15 June 1996
		# if there is no printable key, but one is known from
		# a previous index-entry, then use it.
		#
		if (!($printable_key) && ($printable_key{$index_key}))
			{ $printable_key = $printable_key{$index_key}; } 
#		if (!($printable_key) && ($printable_key{$cur_key}))
#			{ $printable_key = $printable_key{$cur_key}; } 
		#
		# do not overwrite the printable_key if it contains an anchor
		#
		if (!($printable_key{$index_key} =~ /tex2html_anchor_mark/ ))
			{ $printable_key{$index_key} = $printable_key || $key; }
#		if (!($printable_key{$cur_key} =~ /tex2html_anchor_mark/ ))
#			{ $printable_key{$cur_key} = $printable_key || $key; }

		$super_key = $cur_key;
    }
	#
	# RRM
	# page-ranges, from  |(  and  |)  and  |see
	#
    if ($pageref) {
		if ($pageref eq "\(" ) { 
			$pageref = ''; 
			$next .= " from ";
		} elsif ($pageref eq "\)" ) { 
			$pageref = ''; 
			local($next) = $index{$index_key};
#			local($next) = $index{$cur_key};
	#	    $next =~ s/[\|] *$//;
			$next =~ s/(\n )?\| $//;
			$index{$index_key} = "$next to ";
#			$index{$cur_key} = "$next to ";
		}
    }

    if ($pageref) {
		$pageref =~ s/\s*$//g;	# remove trailing spaces
		if (!$pageref) { $pageref = ' ' }
		$pageref =~ s/see/<i>see <\/i> /g;
		#
		# RRM:  27 Dec 1996
		# check if $pageref corresponds to a style command.
		# If so, apply it to the $words.
		#
		local($tmp) = "do_cmd_$pageref";
		if (defined &$tmp) {
			$words = &$tmp("<#0#>$words<#0#>");
			$words =~ s/<\#[^\#]*\#>//go;
			$pageref = '';
		}
    }
	#
	# RRM:  25 May 1996
	# any \label in the pageref section will have already
	# created a label where the \index occurred.
	# This has to be removed, so that the desired label 
	# will be found on the Index page instead. 
	#
    if ($pageref) {
		if ($pageref =~ /tex2html_anchor_mark/ ) {
			$pageref =~ s/><tex2html_anchor_mark><\/A><A//g;
			local($tmpA,$tmpB) = split("NAME=\"", $pageref);
			($tmpA,$tmpB) = split("\"", $tmpB);
			$ref_files{$tmpA}='';
			$index_labels{$tmpA} = 1;
		}
		#
		# resolve and clean-up any hyperlinks in the page-ref, 
		# so they can be saved in an  index.pl  file
		#
		if ($pageref =~ /$cross_ref_mark/ ) {
			local($label,$id,$ref_label);
			#	    $pageref =~ s/$cross_ref_mark#(\w+)#(\w+)>$cross_ref_mark/
			$pageref =~ s/$cross_ref_mark#([^#]+)#([^>]+)>$cross_ref_mark/
				do { ($label,$id) = ($1,$2);
				$ref_files{$label} = ''; # ???? RRM
				if ($index_labels{$label}) { $ref_label = ''; } 
					else { $ref_label = $external_labels{$label} 
						unless ($ref_label = $ref_files{$label});
				}
			'"' . "$ref_label#$label" . '">' . &get_ref_mark($label,$id)}/geo;
		}
		$pageref =~ s/<\#[^\#>]*\#>//go;

		if ($pageref eq ' ') { $index{$index_key}='@'; }
			else { $index{$index_key} .= $pageref . "\n | "; }
    } else {
		local($thisref) = &make_named_href('',"$CURRENT_FILE#$br_id",$words);
		$thisref =~ s/\n//g;
		$index{$index_key} .= $thisref."\n | ";
    }
	#print "\nREF: $sort_key : $index_key :$index{$index_key}";
 
	#join('',"<A NAME=$br_id>$anchor_invisible_mark<\/A>",$_);

    "<A NAME=\"$br_id\">$anchor_invisible_mark<\/A>";
}


# KEC. -- Copied from makeidx.perl, then modified to do multiple indices.
#  Feeds the index entries to the output. This is called for each index to be built.
#
#  Generates a list of lookup keys for index entries, from both %printable_keys
#    and %index keys.
#  Sorts the keys according to index-sorting rules.
#  Removes keys with a 0x01 token. (duplicates?)
#  Builds a string to go to the index file.
#    Adds the index entries to the string if they belong in this index.
#    Keeps track of which index is being worked on, so only the proper entries
#      are included.
#  Places the index just built in to the output at the proper place.
{ my $index_number = 0;
sub add_real_idx {
	print "\nDoing the index ... Index Number $index_number\n";
	local($key, @keys, $next, $index, $old_key, $old_html);
	my ($idx_ref,$keyref);
	# RRM, 15.6.96:  index constructed from %printable_key, not %index
	@keys = keys %printable_key;

	while (/$idx_mark/) {
		# Get the index reference from what follows the $idx_mark and
		#  remove it from the string.
		s/$idxmark\002(.*?)\002/$idxmark/;
		$idx_ref = $1;
		$index = '';
		# include non- makeidx  index-entries
		foreach $key (keys %index) {
			next if $printable_key{$key};
			$old_key = $key;
			if ($key =~ s/###(.*)$//) {
				next if $printable_key{$key};
				push (@keys, $key);
				$printable_key{$key} = $key;
				if ($index{$old_key} =~ /HREF="([^"]*)"/i) {
					$old_html = $1;
					$old_html =~ /$dd?([^#\Q$dd\E]*)#/;
					$old_html = $1;
				} else { $old_html = '' }
				$index{$key} = $index{$old_key} . $old_html."</A>\n | ";
			};
		}
		@keys = sort makeidx_keysort @keys;
		@keys = grep(!/\001/, @keys);
		my $cnt = 0;
		foreach $key (@keys) {
			my ($keyref) = $key =~ /.*\002(.*)/;
			next unless ($idx_ref eq $keyref);		# KEC.
			$index .= &add_idx_key($key);
			$cnt++;
		}
		print "$cnt Index Entries Added\n";
		$index = '<DD>'.$index unless ($index =~ /^\s*<D(D|T)>/);
		$index_number++;	# KEC.
		if ($SHORT_INDEX) {
			print "(compact version with Legend)";
			local($num) = ( $index =~ s/\<D/<D/g );
			if ($num > 50 ) {
				s/$idx_mark/$preindex<HR><DL>\n$index\n<\/DL>$preindex/o;
			} else {
				s/$idx_mark/$preindex<HR><DL>\n$index\n<\/DL>/o;
			}
		} else { 
		s/$idx_mark/<DL COMPACT>\n$index\n<\/DL>/o; }
	}
}
}

# KEC.  Copied from latex2html.pl and modified to support multiple indices.
# The bibliography and the index should be treated as separate sections
# in their own HTML files. The \bibliography{} command acts as a sectioning command
# that has the desired effect. But when the bibliography is constructed
# manually using the thebibliography environment, or when using the
# theindex environment it is not possible to use the normal sectioning
# mechanism. This subroutine inserts a \bibliography{} or a dummy
# \textohtmlindex command just before the appropriate environments
# to force sectioning.
sub add_bbl_and_idx_dummy_commands {
	local($id) = $global{'max_id'};

	s/([\\]begin\s*$O\d+$C\s*thebibliography)/$bbl_cnt++; $1/eg;
	## if ($bbl_cnt == 1) {
		s/([\\]begin\s*$O\d+$C\s*thebibliography)/$id++; "\\bibliography$O$id$C$O$id$C $1"/geo;
	#}
	$global{'max_id'} = $id;
	# KEC. Modified to global substitution to place multiple index tokens.
	s/[\\]begin\s*($O\d+$C)\s*theindex/\\textohtmlindex$1/go;
	# KEC. Modified to pick up the optional argument to \printindex
	s/[\\]printindex\s*(\[.*?\])?/
		do { (defined $1) ? "\\textohtmlindex $1" : "\\textohtmlindex []"; } /ego;
	&lib_add_bbl_and_idx_dummy_commands() if defined(&lib_add_bbl_and_idx_dummy_commands);
}

# KEC.  Copied from latex2html.pl and modified to support multiple indices.
#  For each textohtmlindex mark found, determine the index titles and headers.
#  We place the index ref in the header so the proper index can be generated later.
#  For the default index, the index ref is blank.
# 
#  One problem is that this routine is called twice..  Once for processing the 
#    command as originally seen, and once for processing the command when 
#    doing the name for the index file. We can detect that by looking at the 
#    id numbers (or ref) surrounding the \theindex command, and not incrementing
#    index_number unless a new id (or ref) is seen.  This has the side effect of 
#    having to unconventionally start the index_number at -1. But it works.
#
#  Gets the title from the list of indices.
#  If this is the first index, save the title in $first_idx_file. This is what's referenced
#    in the navigation buttons.
#  Increment the index_number for next time.  
#  If the indexname command is defined or a newcommand defined for indexname, do it.  
#  Save the index TITLE in the toc 
#  Save the first_idx_file into the idxfile. This goes into the nav buttons.  
#  Build index_labels if needed.
#  Create the index headings and put them in the output stream.

{ my $index_number = 0;  # Will be incremented before use.
	my $first_idx_file;  # Static
	my $no_increment = 0;

sub do_cmd_textohtmlindex {
	local($_) = @_;
	my ($idxref,$idxnum,$index_name);

	# We get called from make_name with the first argument = "\001noincrement". This is a sign
	#  to not increment $index_number the next time we are called. We get called twice, once
	#  my make_name and once by process_command.  Unfortunately, make_name calls us just to set the name
	#  but doesn't use the result so we get called a second time by process_command.  This works fine
	#  except for cases where there are multiple indices except if they aren't named, which is the case
	#  when the index is inserted by an include command in latex. In these cases we are only able to use
	#  the index number to decide which index to draw from, and we don't know how to increment that index
	#  number if we get called a variable number of times for the same index, as is the case between
	#  making html (one output file) and web (multiple output files) output formats.
	if (/\001noincrement/) {
		$no_increment = 1;
		return;
	}

	# Remove (but save) the index reference 
	s/^\s*\[(.*?)\]/{$idxref = $1; "";}/e;

	# If we have an $idxref, the index name was specified.  In this case, we have all the
	#  information we need to carry on.  Otherwise, we need to get the idxref
	#  from the $index_number and set the name to "Index".
	if ($idxref) {
		$index_name = $indices{'title'}{$idxref};
	} else {
		if (defined ($idxref = $indices{'newcmdorder'}->[$index_number])) {
			$index_name = $indices{'title'}{$idxref};
		} else { 
			$idxref = '';
			$index_name = "Index";
		}
	}

	$idx_title = "Index"; # The name displayed in the nav bar text.

	# Only set $idxfile if we are at the first index.  This will point the 
	#  navigation panel to the first index file rather than the last.
	$first_idx_file = $CURRENT_FILE if ($index_number == 0);
	$idxfile = $first_idx_file; # Pointer for the Index button in the nav bar.
	$toc_sec_title = $index_name; # Index link text in the toc.
	$TITLE = $toc_sec_title; # Title for this index, from which its filename is built.
	if (%index_labels) { &make_index_labels(); }
	if (($SHORT_INDEX) && (%index_segment)) { &make_preindex(); }
		else { $preindex = ''; }
	local $idx_head = $section_headings{'textohtmlindex'};
	local($heading) = join(''
		, &make_section_heading($TITLE, $idx_head)
		, $idx_mark, "\002", $idxref, "\002" );
	local($pre,$post) = &minimize_open_tags($heading);
	$index_number++ unless ($no_increment);
	$no_increment = 0;
	join('',"<BR>\n" , $pre, $_);
}
}

# Returns an index key, given the key passed as the first argument.
#  Not modified for multiple indices.
sub add_idx_key {
    local($key) = @_;
	local($index, $next);
	if (($index{$key} eq '@' )&&(!($index_printed{$key}))) {
		if ($SHORT_INDEX) { $index .= "<DD><BR>\n<DT>".&print_key."\n<DD>"; }
			else { $index .= "<DT><DD><BR>\n<DT>".&print_key."\n<DD>"; }
	} elsif (($index{$key})&&(!($index_printed{$key}))) {
		if ($SHORT_INDEX) {
			$next = "<DD>".&print_key."\n : ". &print_idx_links;
		} else {
			$next = "<DT>".&print_key."\n<DD>". &print_idx_links;
		}
		$index .= $next."\n";
		$index_printed{$key} = 1;
	}

	if ($sub_index{$key}) {
		local($subkey, @subkeys, $subnext, $subindex);
		@subkeys = sort(split("\004", $sub_index{$key}));
		if ($SHORT_INDEX) {
			$index .= "<DD>".&print_key  unless $index_printed{$key};
			$index .= "<DL>\n"; 
		} else {
			$index .= "<DT>".&print_key."\n<DD>"  unless $index_printed{$key};
			$index .= "<DL COMPACT>\n"; 
		}
		foreach $subkey (@subkeys) {
			$index .= &add_sub_idx_key($subkey) unless ($index_printed{$subkey});
		}
		$index .= "</DL>\n";
	}
	return $index;
}

1;  # Must be present as the last line.
