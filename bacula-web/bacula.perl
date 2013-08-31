# This file contains subroutines for use by the latex2html system.
#  This file is executed due to a \usepackage{bacula} statement
#  in the LaTeX source. The subroutines here impliment functionality
#  specific to the generation of html manuals for the Bacula project.
# Some of the added functionality is designed to extend the capabiltites
#  of latex2html and some is to change its behavior.


# Returns the minimum of any number of numeric arguments.
sub min {
	my $tmp = shift;
	while ($test = shift) {
		$tmp = $test if ($test < $tmp);
	}
	return $tmp;
}

# These two are copied from 
#   /usr/lib/latex2html/style/hthtml.perl,
#   from the subroutine do_cmd_htmladdnormallink.
#   They have been renamed, then removed the 
#   name argument and reversed the other two arguments.
sub do_cmd_elink{
	local($_) = @_;
	local($text, $url, $href);
	local($opt, $dummy) = &get_next_optional_argument;
	$text = &missing_braces unless
		((s/$next_pair_pr_rx/$text = $2; ''/eo)
		||(s/$next_pair_rx/$text = $2; ''/eo));
	$url = &missing_braces unless
		((s/$next_pair_pr_rx/$url = $2; ''/eo)
		||(s/$next_pair_rx/$url = $2; ''/eo));
	$*=1; s/^\s+/\n/; $*=0;
	$href = &make_href($url,$text);
	print "\nHREF:$href" if ($VERBOSITY > 3);
	join ('',$href,$_);
}

sub do_cmd_ilink {
	local($_) = @_;
	local($text);
	local($opt, $dummy) = &get_next_optional_argument;
	$text = &missing_braces unless
			((s/$next_pair_pr_rx/$text = $2; ''/eo)
			||(s/$next_pair_rx/$text = $2; ''/eo));
	&process_ref($cross_ref_mark,$cross_ref_mark,$text);
}

sub do_cmd_lt { join('',"\&lt;",$_[0]); }
sub do_cmd_gt { join('',"\&gt;",$_[0]); }

# KEC  Copied from latex2html.pl and modified to prevent 
#  filename collisions. This is done with a static hash of 
#  already-used filenames. An integer is appended to the 
#  filename if a collision would result without it.
#  The addition of the integer is done by removing 
#  character(s) before .html if adding the integer would result
#  in a filename longer than 32 characters. Usually just removing
#  the character before .html would resolve the collision, but we
#  add the integer anyway. The first integer that resolves the 
#  collision is used.
# If a filename is desired that is 'index.html' or any case
#  variation of that, it is changed to index_page.html,
#  index_page1.html, etc.


#RRM  Extended to allow customised filenames, set $CUSTOM_TITLES
#     or long title from the section-name, set $LONG_TITLES
#
{ my %used_names; # Static hash.
sub make_name {
	local($sec_name, $packed_curr_sec_id) = @_;
	local($title,$making_name,$saved) = ('',1,'');
	my $final_name;
	if ($LONG_TITLES) {
		$saved = $_;
		# This alerts the subroutine textohtmlindex not to increment its index counter on the next call.
		&do_cmd_textohtmlindex("\001noincrement"); 
		&process_command($sections_rx, $_) if /^$sections_rx/;
		$title = &make_bacula_title($TITLE)
			unless ((! $TITLE) || ($TITLE eq $default_title));
		$_ = $saved;
	} elsif ($CUSTOM_TITLES) {
		$saved = $_;
		# This alerts the subroutine textohtmlindex not to increment its index counter on the next call.
		&do_cmd_textohtmlindex("\001noincrement"); 
		&process_command($sections_rx, $_) if /^$sections_rx/;
		$title = &custom_title_hook($TITLE)
			unless ((! $TITLE) || ($TITLE eq $default_title));
		$_ = $saved;
	}
	if ($title) {
		#ensure no more than 32 characters, including .html extension
		$title =~ s/^(.{1,27}).*$/$1/;
		++$OUT_NODE;
		$final_name = join("", ${PREFIX}, $title, $EXTN);
	} else {
		# Remove 0's from the end of $packed_curr_sec_id
		$packed_curr_sec_id =~ s/(_0)*$//;
		$packed_curr_sec_id =~ s/^\d+$//o; # Top level file
		$final_name = join("",($packed_curr_sec_id ?
			"${PREFIX}$NODE_NAME". ++$OUT_NODE : $sec_name), $EXTN);
	}

	# Change the name from index to index_page to avoid conflicts with
	#  index.html.
	$final_name =~ s/^(index)\.html$/$1_Page.html/i;

	# If the $final_name is already used, put an integer before the
	#     #  .html to make it unique.
	my $integer = 0;
	my $saved_name = $final_name;
	while (exists($used_names{$final_name})) {
		$final_name = $saved_name;
		my ($filename,$ext) = $final_name =~ /(.*)(\..*)$/;
		my $numlen = length(++$integer);

		# If the filename (after adding the integer) would be longer than
		#  32 characters, insert the integer within it.
		if (((my $namelen = length($final_name)) + $numlen) >= 32) {
			substr($filename,-$numlen) = $integer;
		} else {
			$filename .= $integer;
		}
		$final_name = $filename . $ext;
	}

	# Save the $final_name in the hash to mark it as being used.
	$used_names{$final_name} = undef;

	# Save the first name evaluated here.  This is the name of the top-level html file, and
	#  can be used to produce the index.html hard link at the end.
	$OVERALL_TITLE = $final_name if (!defined $OVERALL_TITLE);

	return $final_name;
}
}

sub make_bacula_title {
    local($_)= @_;
	local($num_words) = $LONG_TITLES;
	#RRM:  scan twice for short words, due to the $4 overlap
	#      Cannot use \b , else words break at accented letters
	$_ =~ s/(^|\s)\s*($GENERIC_WORDS)(\'|(\s))/$4/ig;
	$_ =~ s/(^|\s)\s*($GENERIC_WORDS)(\'|(\s))/$4/ig;
	#remove leading numbering, unless that's all there is.
	local($sec_num);
	if (!(/^\d+(\.\d*)*\s*$/)&&(s/^\s*(\d+(\.\d*)*)\s*/$sec_num=$1;''/e))
		{ $num_words-- };
	&remove_markers; s/<[^>]*>//g; #remove tags
	#revert entities, etc. to TeX-form...
	s/([\200-\377])/"\&#".ord($1).";"/eg;
	$_ = &revert_to_raw_tex($_);

	# get $LONG_TITLES number of words from what remains
	$_ = &get_bacula_words($_, $num_words) if ($num_words);
	# ...and cleanup accents, spaces and punctuation
	$_ = join('', ($SHOW_SECTION_NUMBERS ? $sec_num : ''), $_);
	s/\\\W\{?|\}//g;
	s/\s/_/g;
	s/\'s/s/ig; # Replace 's with just the s.
	s/\W/_/g;
	s/__+/_/g;
	s/_+$//;
	$_;
}

	#JCL(jcl-tcl)
	# changed completely
	# KEC 2-21-05  Changed completely again. 
	#
	# We take the first real words specified by $min from the string.
	#  REmove all markers and markups.
	# Split the line into words.
	# Determine how many words we should process.
	# Return if no words to process.
	# Determine lengths of the words.
	# Reduce the length of the longest words in the list until the
	#  total length of all the words is acceptable.
	# Put the words back together and return the result.
	#
sub get_bacula_words {
	local($_, $min) = @_;
	local($words,$i);
	local($id,%markup);
	# KEC
	my ($oalength,@lengths,$last,$thislen);
	my $maxlen = 28;

	#no limit if $min is negative
	$min = 1000 if ($min < 0);

	&remove_anchors;
	#strip unwanted HTML constructs
	s/<\/?(P|BR|H)[^>]*>/ /g;
	#remove leading white space and \001 characters
	s/^\s+|\001//g;
	#lift html markup
	s/(<[^>]*>(#[^#]*#)?)//ge;

	# Split $_ into a list of words.
	my @wrds = split /\s+|\-{3,}/;
	$last = &min($min - 1,$#wrds);
	return '' if ($last < 0);

	# Get a list of word lengths up to the last word we're to process.
	#  Add one to each for the separator.
	@lengths = map (length($_)+1,@wrds[0..$last]);

	$thislen = $maxlen + 1; # One more than the desired max length.
	do {
		$thislen--;
		@lengths = map (&min($_,$thislen),@lengths);
		$oalength = 0;
		foreach (@lengths) {$oalength += $_;}
	} until ($oalength <= $maxlen);
	$words = join(" ",map (substr($wrds[$_],0,$lengths[$_]-1),0..$last));
	return $words;
}

sub do_cmd_htmlfilename {
	my $input = shift;

	my ($id,$filename) = $input =~ /^<#(\d+)#>(.*?)<#\d+#>/;
}

# KEC 2-26-05
# do_cmd_addcontentsline adds support for the addcontentsline latex command. It evaluates
#  the arguments to the addcontentsline command and determines where to put the information.  Three
#  global lists are kept: for table of contents, list of tables, and list of figures entries.
#  Entries are saved in the lists in the order they are encountered so they can be retrieved
#  in the same order.
my (%toc_data);
sub do_cmd_addcontentsline { 
	&do_cmd_real_addcontentsline(@_); 
}
sub do_cmd_real_addcontentsline {
    my $data = shift;
	my ($extension,$pat,$unit,$entry);

    # The data is sent to us as fields delimited by their ID #'s.  Extract the
    #  fields.  The first is the extension of the file to which the cross-reference
	#  would be written by LaTeX, such as {toc}, {lot} or {lof}.  The second is either
	#  {section}, {subsection}, etc. for a toc entry, or , {table}, or {figure} 
	#  for a lot, or lof extension (must match the first argument), and 
	#  the third is the name of the entry.  The position in the document represents
	#  and anchor that must be built to provide the linkage from the entry.
    $extension = &missing_braces unless (
    ($data =~ s/$next_pair_pr_rx/$extension=$2;''/eo)
    ||($data =~ s/$next_pair_rx/$extension=$2;''/eo));
    $unit = &missing_braces unless (
    ($data =~ s/$next_pair_pr_rx/$unit=$2;''/eo)
    ||($data =~ s/$next_pair_rx/$unit=$2;''/eo));
    $entry = &missing_braces unless (
    ($data =~ s/$next_pair_pr_rx/$pat=$1;$entry=$2;''/eo)
    ||($data =~ s/$next_pair_rx/$pat=$1;$entry=$2;''/eo));

	$contents_entry = &make_contents_entry($extension,$pat,$entry,$unit);
	return ($contents_entry . $data);
}

# Creates and saves a contents entry (toc, lot, lof) to strings for later use, 
#  and returns the entry to be inserted into the stream.
# 
sub make_contents_entry {
    local($extension,$br_id, $str, $unit) = @_;
	my $words = '';
	my ($thisref);

    # If TITLE is not yet available use $before.
    $TITLE = $saved_title if (($saved_title)&&(!($TITLE)||($TITLE eq $default_title)));
    $TITLE = $before unless $TITLE;
    # Save the reference
    if ($SHOW_SECTION_NUMBERS) { 
		$words = &get_first_words($TITLE, 1);
	} else { 
		$words = &get_first_words($TITLE, 4);
	}
	$words = 'no title' unless $words;

	#
	# any \label in the $str will have already
	# created a label where the \addcontentsline occurred.
	# This has to be removed, so that the desired label 
	# will be found on the toc page.
	#
	if ($str =~ /tex2html_anchor_mark/ ) {
		$str =~ s/><tex2html_anchor_mark><\/A><A//g;
	}
	#
	# resolve and clean-up the hyperlink entries 
	# so they can be saved
	#
	if ($str =~ /$cross_ref_mark/ ) {
		my ($label,$id,$ref_label);
		$str =~ s/$cross_ref_mark#([^#]+)#([^>]+)>$cross_ref_mark/
			do { ($label,$id) = ($1,$2);
			$ref_label = $external_labels{$label} unless
			($ref_label = $ref_files{$label});
			'"' . "$ref_label#$label" . '">' .
			&get_ref_mark($label,$id)}
		/geo;
	}
	$str =~ s/<\#[^\#>]*\#>//go;
	#RRM
	# recognise \char combinations, for a \backslash
	#
	$str =~ s/\&\#;\'134/\\/g;		# restore \\s
	$str =~ s/\&\#;\`<BR> /\\/g;	#  ditto
	$str =~ s/\&\#;*SPMquot;92/\\/g;	#  ditto

	$thisref = &make_named_href('',"$CURRENT_FILE#$br_id",$str);
	$thisref =~ s/\n//g;

	# Now we build the actual entry that will go in the lot and lof.
	# If this is the first entry, we have to put a leading newline.
	if ($unit eq 'table' ) {
		if (!$table_captions) { $table_captions = "\n";}
		$table_captions .= "<LI>$thisref\n";
	} elsif ($unit eq 'figure') {
		if (!$figure_captions) { $figure_captions = "\n"; }
		$figure_captions .= "<LI>$thisref\n";
	}
    "<A NAME=\"$br_id\">$anchor_invisible_mark<\/A>";
}

# This is needed to keep latex2html from trying to make an image for the registered
#  trademark symbol (R).  This wraps the command in a deferred wrapper so it can be
#  processed as a normal command later on.  If this subroutine is not put in latex2html
#  invokes latex to create an image for the symbol, which looks bad.
sub wrap_cmd_textregistered {
    local($cmd, $_) = @_;
    (&make_deferred_wrapper(1).$cmd.&make_deferred_wrapper(0),$_)
}

# KEC
# Copied from latex2html.pl and modified to create a file of image translations.
#  The problem is that latex2html creates new image filenames like imgXXX.png, where
#  XXX is a number sequentially assigned.  This is fine but makes for very unfriendly 
#  image filenames. I looked into changing this behavior and it seems very much embedded
#  into the latex2html code, not easy to change without risking breaking something.
# So I'm taking the approach here to write out a file of image filename translations,
#  to reference the original filenames from the new filenames.  THis was post-processing
#  can be done outside of latex2html to rename the files and substitute the meaningful
#  image names in the html code generated by latex2html.  This post-processing is done
#  by a program external to latex2html.
#
# What we do is this: This subroutine is called to output images.tex, a tex file passed to 
#  latex to convert the original images to .ps.  The string $latex_body contains info for 
#  each image file, in the form of a unique id and the orininal filename.  We extract both, use
#  the id is used to look up the new filename in the %id_map hash.  The new and old filenames
#  are output into the file 'filename_translations' separated by \001.
#  
sub make_image_file {
    do {
		print "\nWriting image file ...\n";
		open(ENV,">.$dd${PREFIX}images.tex")
				|| die "\nCannot write '${PREFIX}images.tex': $!\n";
		print ENV &make_latex($latex_body);
		print ENV "\n";
		close ENV;
		&copy_file($FILE, "bbl");
		&copy_file($FILE, "aux");
    } if ((%latex_body) && ($latex_body =~ /newpage/));
}


# KEC
# Copied from latex2html.pl and modified to create a file of image translations.

#  The problem is that latex2html creates new image filenames like imgXXX.png, where
#  XXX is a number sequentially assigned.  This is fine but makes for very unfriendly 
#  image filenames. I looked into changing this behavior and it seems very much embedded
#  into the latex2html code, not easy to change without risking breaking something.
# So I'm taking the approach here to write out a file of image filename translations,
#  to reference the original filenames from the new filenames.  THis post-processing
#  can be done outside of latex2html to rename the files and substitute the meaningful
#  image names in the html code generated by latex2html.  This post-processing is done
#  by a program external to latex2html.
#
# What we do is this: This subroutine is called to output process images.  Code has been inserted
#  about 100 lines below this to create the list of filenames to translate.  See comments there for 
#  details.
#  

# Generate images for unknown environments, equations etc, and replace
# the markers in the main text with them.
# - $cached_env_img maps encoded contents to image URL's
# - $id_map maps $env$id to page numbers in the generated latex file and after
# the images are generated, maps page numbers to image URL's
# - $page_map maps page_numbers to image URL's (temporary map);
# Uses global variables $id_map and $cached_env_img,
# $new_page_num and $latex_body


sub make_images {
    local($name, $contents, $raw_contents, $uucontents, $page_num,
	  $uucontents, %page_map, $img);
    # It is necessary to run LaTeX this early because we need the log file
    # which contains information used to determine equation alignment
    if ( $latex_body =~ /newpage/) {
	print "\n";
	if ($LATEX_DUMP) {
	    # dump a pre-compiled format
	    if (!(-f "${PREFIX}images.fmt")) {
	        print "$INILATEX ./${PREFIX}images.tex\n" 
		    if (($DEBUG)||($VERBOSITY > 1));
	        print "dumping ${PREFIX}images.fmt\n"
		    unless ( L2hos->syswait("$INILATEX ./${PREFIX}images.tex"));
	    }
	    local ($img_fmt) = (-f "${PREFIX}images.fmt");
	    if ($img_fmt) {
                # use the pre-compiled format
	        print "$TEX \"&./${PREFIX}images\" ./${PREFIX}images.tex\n"
		    if (($DEBUG)||($VERBOSITY > 1));
	        L2hos->syswait("$TEX \"&./${PREFIX}images\" ./${PREFIX}images.tex");
	    } elsif (-f "${PREFIX}images.dvi") {
	        print "${PREFIX}images.fmt failed, proceeding anyway\n";
	    } else {
	        print "${PREFIX}images.fmt failed, trying without it\n";
		print "$LATEX ./${PREFIX}images.tex\n"
		    if (($DEBUG)||($VERBOSITY > 1));
		L2hos->syswait("$LATEX ./${PREFIX}images.tex");
	    }
	} else { &make_latex_images() }
#	    local($latex_call) = "$LATEX .$dd${PREFIX}images.tex";
#	    print "$latex_call\n" if (($DEBUG)||($VERBOSITY > 1));
#	    L2hos->syswait("$latex_call");
##	    print "$LATEX ./${PREFIX}images.tex\n" if (($DEBUG)||($VERBOSITY > 1));
##	    L2hos->syswait("$LATEX ./${PREFIX}images.tex");
##        }
	$LaTeXERROR = 0;
	&process_log_file("./${PREFIX}images.log"); # Get image size info
    }
    if ($NO_IMAGES) {
        my $img = "image.$IMAGE_TYPE";
	my $img_path = "$LATEX2HTMLDIR${dd}icons$dd$img";
	L2hos->Copy($img_path, ".$dd$img")
            if(-e $img_path && !-e $img);
    }
    elsif ((!$NOLATEX) && ($latex_body =~ /newpage/) && !($LaTeXERROR)) {
   	print "\nGenerating postscript images using dvips ...\n";
        &make_tmp_dir;  # sets  $TMPDIR  and  $DESTDIR
	$IMAGE_PREFIX =~ s/^_//o if ($TMPDIR);

	local($dvips_call) = 
		"$DVIPS -S1 -i $DVIPSOPT -o$TMPDIR$dd$IMAGE_PREFIX .${dd}${PREFIX}images.dvi\n";
	print $dvips_call if (($DEBUG)||($VERBOSITY > 1));
	
	if ((($PREFIX=~/\./)||($TMPDIR=~/\./)) && not($DVIPS_SAFE)) {
	    print " *** There is a '.' in $TMPDIR or $PREFIX filename;\n"
	    	. "  dvips  will fail, so image-generation is aborted ***\n";
	} else {
	    &close_dbm_database if $DJGPP;
	    L2hos->syswait($dvips_call) && print "Error: $!\n";
	    &open_dbm_database if $DJGPP;
	}

	# append .ps suffix to the filenames
	if(opendir(DIR, $TMPDIR || '.')) {
            # use list-context instead; thanks De-Wei Yin <yin@asc.on.ca>
	    my @ALL_IMAGE_FILES = grep /^$IMAGE_PREFIX\d+$/o, readdir(DIR);
	    foreach (@ALL_IMAGE_FILES) {
	        L2hos->Rename("$TMPDIR$dd$_", "$TMPDIR$dd$_.ps");
	    }
	    closedir(DIR);
        } else {
            print "\nError: Cannot read dir '$TMPDIR': $!\n";
        }
    }
    do {print "\n\n*** LaTeXERROR"; return()} if ($LaTeXERROR);
    return() if ($LaTeXERROR); # empty .dvi file
    L2hos->Unlink(".$dd${PREFIX}images.dvi") unless $DEBUG;

    print "\n *** updating image cache\n" if ($VERBOSITY > 1);
    while ( ($uucontents, $_) = each %cached_env_img) {
	delete $cached_env_img{$uucontents}
	    if ((/$PREFIX$img_rx\.$IMAGE_TYPE/o)&&!($DESTDIR&&$NO_SUBDIR));
	$cached_env_img{$uucontents} = $_
	    if (s/$PREFIX$img_rx\.new/$PREFIX$1.$IMAGE_TYPE/go);
    }

	# Modified from the original latex2html to translate image filenames to meaningful ones.
	#  KEC  5-22-05.
	print "\nWriting imagename_translations file\n";
	open KC,">imagename_translations" or die "Cannot open filename translation file for writing";
	my ($oldname_kc,$newname_kc,$temp_kc,%done_kc);
	while ((undef,$temp_kc) = each %id_map) {
		# Here we generate the file containing the list if old and new filenames.
		# The old and new names are extracted from variables in scope at the time
		#  this is run.  The values of the %id_map has contain either the number of the
		#  image file to be created (if an old image file doesn't exist) or the tag to be placed
		#  inside the html file (if an old image file does exist).  We extract the info in either
		#  case.
		if ($temp_kc =~ /^\d+\#\d+$/) {
			my $kcname;
			$kcname = $orig_name_map{$temp_kc};
			$kcname =~ s/\*/star/;
			($oldname_kc) = $img_params{$kcname} =~ /ALT=\"\\includegraphics\{(.*?)\}/s;
			($newname_kc) = split (/#/,$temp_kc);
			$newname_kc = "img" . $newname_kc . ".png";
		} else {
			($newname_kc,$oldname_kc) = $temp_kc =~ /SRC=\"(.*?)\".*ALT=\"\\includegraphics\{(.*?)\}/s;
		}
		# If this is a math-type image, $oldname_kc will be blank.  Don't do anything in that case since
		#  there is no meaningful image filename.
		if (!exists($done_kc{$newname_kc}) and $oldname_kc) {
			print KC "$newname_kc\001$oldname_kc\n";
		}
		$done_kc{$newname_kc} = '';
	}
	close KC;

    print "\n *** removing unnecessary images ***\n" if ($VERBOSITY > 1);
    while ( ($name, $page_num) = each %id_map) {
	$contents = $latex_body{$name};

	if ($page_num =~ /^\d+\#\d+$/) { # If it is a page number
	    do {		# Extract the page, convert and save it
		$img = &extract_image($page_num,$orig_name_map{$page_num});
		if ($contents =~ /$htmlimage_rx/) {
		    $uucontents = &special_encoding($env,$2,$contents);
		} elsif ($contents =~ /$htmlimage_pr_rx/) {
		    $uucontents = &special_encoding($env,$2,$contents);
		} else {
		    $uucontents = &encode(&addto_encoding($contents,$contents));
		}
		if (($HTML_VERSION >=3.2)||!($contents=~/$order_sensitive_rx/)){
		    $cached_env_img{$uucontents} = $img;
		} else {
                    # Blow it away so it is not saved for next time
		    delete $cached_env_img{$uucontents};
		    print "\nimage $name not recycled, contents may change (e.g. numbering)";
		}
		$page_map{$page_num} = $img;
	    } unless ($img = $page_map{$page_num}); # unless we've just done it
	    $id_map{$name} = $img;
	} else {
	    $img = $page_num;	# it is already available from previous runs
	}
	print STDOUT " *** image done ***\n" if ($VERBOSITY > 2);
    }
    &write_warnings(
		    "\nOne of the images is more than one page long.\n".
		    "This may cause the rest of the images to get out of sync.\n\n")
	if (-f sprintf("%s%.3d%s", $IMAGE_PREFIX, ++$new_page_num, ".ps"));
    print "\n *** no more images ***\n"  if ($VERBOSITY > 1);
    # MRO: The following cleanup seems to be incorrect: The DBM is
    # still open at this stage, this causes a lot of unlink errors
    #
    #do { &cleanup; print "\n *** clean ***\n"  if ($VERBOSITY > 1);}
    #	unless $DJGPP;
}

## KEC: Copied &text_cleanup here to modify it. It was filtering out double
# dashes such as {-}{-}sysconfig.  This would be used as an illustration
# of a command-line arguement.  It was being changed to a single dash.

# This routine must be called once on the text only,
# else it will "eat up" sensitive constructs.
sub text_cleanup {
    # MRO: replaced $* with /m
    s/(\s*\n){3,}/\n\n/gom;   # Replace consecutive blank lines with one
    s/<(\/?)P>\s*(\w)/<$1P>\n$2/gom;      # clean up paragraph starts and ends
    s/$O\d+$C//go;      # Get rid of bracket id's
    s/$OP\d+$CP//go;    # Get rid of processed bracket id's
	 # KEC: This is the line causing trouble...
    #s/(<!)?--?(>)?/(length($1) || length($2)) ? "$1--$2" : "-"/ge;
    s/(<!)?--?(>)?/(length($1) || length($2)) ? "$1--$2" : $&/ge;
    # Spacing commands
    s/\\( |$)/ /go;
    #JKR: There should be no more comments in the source now.
    #s/([^\\]?)%/$1/go;        # Remove the comment character
    # Cannot treat \, as a command because , is a delimiter ...
    s/\\,/ /go;
    # Replace tilde's with non-breaking spaces
    s/ *~/&nbsp;/g;

    ### DANGEROUS ?? ###
    # remove redundant (not <P></P>) empty tags, incl. with attributes
    s/\n?<([^PD >][^>]*)>\s*<\/\1>//g;
    s/\n?<([^PD >][^>]*)>\s*<\/\1>//g;
    # remove redundant empty tags (not </P><P> or <TD> or <TH>)
    s/<\/(TT|[^PTH][A-Z]+)><\1>//g;
    s/<([^PD ]+)(\s[^>]*)?>\n*<\/\1>//g;


#JCL(jcl-hex)
# Replace ^^ special chars (according to p.47 of the TeX book)
# Useful when coming from the .aux file (german umlauts, etc.)
    s/\^\^([^0-9a-f])/chr((64+ord($1))&127)/ge;
    s/\^\^([0-9a-f][0-9a-f])/chr(hex($1))/ge;
}




1;  # Must be present as the last line.

