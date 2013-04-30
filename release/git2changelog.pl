#!/usr/bin/perl -w
#
=head USAGE
    
    ./git2changelog.pl Release-3.0.1..Release-3.0.2

 For bweb ReleaseNotes, use
    FORBWEB=1 ./git2changelog.pl Release-3.0.1..Release-3.0.2

=cut

use strict;
use POSIX q/strftime/;

my $d='';
my $cur;
my %elt;
my $last_txt='';
my %bugs;
my $refs = shift || '';
my $for_bweb = $ENV{FORBWEB}?1:0;
open(FP, "git log --no-merges --pretty=format:'%at: %s' $refs|") or die "Can't run git log $!";
while (my $l = <FP>) {

    # remove non useful messages
    next if ($l =~ /(tweak|typo|cleanup|regress:|again|.gitignore|fix compilation|technotes)/ixs);
    next if ($l =~ /update (version|technotes|kernstodo|projects|releasenotes|version|home|release|todo|notes|changelog|tpl|configure)/i);

    next if ($l =~ /bacula-web:/);

    if ($for_bweb) {
        next if ($l !~ /bweb/ixs);
        $l =~ s/bweb: *//ig;
    } else {
        next if ($l =~ /bweb:/ixs);
    }

    # keep list of fixed bugs
    if ($l =~ /#(\d+)/) {
        $bugs{$1}=1;
    }

    # remove old commit format
    $l =~ s/^(\d+): (kes|ebl)  /$1: /;

    if ($l =~ /(\d+): (.+)/) {
        # use date as 01Jan70
        my $dnow = strftime('%d%b%y', localtime($1));
        my $cur = strftime('%Y%m%d', localtime($1));
        my $txt = $2;

        # avoid identical multiple commit message
        next if ($last_txt eq $txt);
        $last_txt = $txt;

        # We format the string on 79 caracters
        $txt =~ s/\s\s+/ /g;
        $txt =~ s/.{70,77} /$&\n  /g;

        # if we are the same day, just add entry
        if ($dnow ne $d) {
            $d = $dnow;
            if (!exists $elt{$cur}) {
                push @{$elt{$cur}}, "\n\n$dnow";
            }
        }
        push @{$elt{$cur}},  " - $txt";

    } else {
        print STDERR "invalid format: $l\n";
    }
}

close(FP);

foreach my $d (sort {$b <=> $a} keys %elt) {
    print join("\n", @{$elt{$d}});
}

print "\n\nBugs fixed/closed since last release:\n";
print join(" ", sort keys %bugs), "\n";
