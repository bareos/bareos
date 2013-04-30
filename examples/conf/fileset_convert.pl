#!/usr/bin/perl
#
# A nice little script written by Matt Howard that will
#  convert old style Bacula FileSets into the new style.
#
#   fileset_convert.pl bacula-dir.conf >output-file 
#
use warnings;
use strict;

my $in;
$in .= $_ while (<>);

sub options {
    return "Options { ".join('; ',split(/\s+/, shift)) . " } ";
}

sub file_lines {
    return join($/, map {"    File = $_"} split(/\n\s*/, shift));
}
 
$in =~ s/Include\s*=\s*((?:\w+=\w+\s+)*){\s*((?:.*?\n)+?)\s*}/
  "Include { " . 
  ( $1 ? options($1) : '' ) . "\n" .
  file_lines($2) .
  "\n  }" /eg;

$in =~ s/Exclude\s*=\s*{\s*}/
  "Exclude { }"/eg;

$in =~ s/Exclude\s*=\s*{\s*((?:.*?\n)+?)\s*}/
  "Exclude {\n" . file_lines($1) . "\n  }"/eg;

print $in;
