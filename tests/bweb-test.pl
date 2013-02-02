#!/usr/bin/perl -w
use strict;

=head1 LICENSE

   Bweb - A Bareos web interface
   Bareos® - The Network Backup Solution

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.

   The main author of Bweb is Eric Bollengier.
   The main author of Bareos is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation plus additions
   that are listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bareos® is a registered trademark of Kern Sibbald.
   The licensor of Bareos is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zurich,
   Switzerland, email:ftf@fsfeurope.org.

=head1 USAGE

    - You need to be in a X session
    - Install Selenium IDE addon from http://seleniumhq.org/
    - Install through CPAN WWW::Selenium
      $ perl -e 'install WWW::Selenium' -MCPAN
    - Download Selenium RC (remote control) from  http://seleniumhq.org/
    - unzip the archive, and start the Selenium server (require java >= 1.5)
      $ java -jar selenium-server.jar
    - Load bweb sql file
    - run backup-bareos-test
    - Start the test
      $ ./tests/bweb-test.pl

=cut

use warnings;
use Time::HiRes qw(sleep);
use Test::WWW::Selenium;
use Test::More "no_plan";
use Test::Exception;
use Getopt::Long;
use scripts::functions;
use File::Copy qw/copy move/;

my ($login, $pass, $verbose, %part, @part, $noclean, $client, $multi);
my $url = "http://localhost:9180";
my @available = qw/client group location run missingjob media overview config/;

GetOptions ("login=s"   => \$login,
	    "passwd=s"  => \$pass,
	    "url|u=s"   => \$url,
            "module=s@" => \@part,
	    "verbose"   => \$verbose,
            "nocleanup" => \$noclean,
            "client=s"  => \$client,
            "dirs"      => \$multi,
	    );

die "Usage: $0 --url http://.../cgi-bin/bweb/bweb.pl [-m module] [-n]"
    unless ($url);

if (scalar(@part)) {
    %part = map { $_ => 1 } @part;
} else {
    %part = map { $_ => 1 } @available;
}

if ($multi) {
    $part{multidir} = 1;
}

if (!$noclean) {
    cleanup();
    `scripts/copy-confs`;
    `sed -i 's/# Sched/ Sched/' $conf/bareos-dir.conf`;
    start_bareos();
}

my $sel = Test::WWW::Selenium->new( host => "localhost",
                                    port => 4444,
                                    browser => "*firefox",
                                    browser_url => $url );

$sel->open_ok("/cgi-bin/bweb/bweb.pl?");
$sel->set_speed(100);

if ($part{multidir}) {
    # if the current bconsole.conf doesn't contain two sections, we
    # create it
    if (!get_resource("$conf/bconsole.conf", "Director", "bweb-dir")) {
        copy("$conf/bconsole.conf", "$tmp/bconsole.conf.$$");
        my $r = get_resource("$conf/bconsole.conf", "Director", ".+?");
        open(FP, ">>$conf/bconsole.conf");
        $r =~ s/Name = .+$/Name = bweb1-dir/m;
        print FP $r;
        $r =~ s/Name = .+$/Name = bweb2-dir/m;
        print FP $r;
        close(FP);
    }
    $sel->open_ok("/cgi-bin/bweb/bweb.pl");
    $sel->click_ok("link=Configuration");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("Main Configuration");

    # create subconf
    $sel->click_ok("//button[\@name='action' and \@value='add_conf']");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("Unnamed");

    $sel->type_ok("name", "MyBweb");
    $sel->type_ok("new_dir", "bweb1-dir");

    $sel->type_ok("stat_job_table", "JobHisto");
    $sel->click_ok("//button[\@name='action' and \@value='apply_conf']");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("MyBweb");
    $sel->is_text_present_ok("bweb1-dir");

    $sel->click_ok("link=Main");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("Directors");
    $sel->click_ok("link=MyBweb");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("Informations on MyBweb");
}

if ($part{client}) {
# test client
    $sel->click_ok("link=Main");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("Running Jobs");

    $sel->click_ok("link=Clients");
    $sel->wait_for_page_to_load_ok("30000");
    $client = $sel->get_text("//tr[\@id='even_row']/td[1]");

    $sel->click_ok("//input[\@name='client']"); # click the first client
    $sel->click_ok("//button[\@name='action' and \@value='client_status']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Command output");
    $sel->click_ok("link=Clients");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Running Jobs"); # This message is in client status
}

if ($part{media}) {
# add media
    $sel->click_ok("link=Main");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("Running Jobs");

    $sel->click_ok("link=Add Media");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->select_ok("pool", "label=Scratch");
    $sel->select_ok("storage", "label=File");
    $sel->type_ok("nb", "10");
    $sel->click_ok("//button[\@name='action']"); # create 10 volumes
    $sel->wait_for_page_to_load_ok("30000");

  WAIT: {
      for (1..60) {
          if (eval { $sel->is_text_present("Select") }) { pass; last WAIT }
        sleep(1);
      }
      fail("timeout");
}
    $sel->click_ok("link=All Media");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Vol0001");
    $sel->is_text_present_ok("Vol0010");
    $sel->select_ok("mediatype", "label=File");
    $sel->select_ok("volstatus", "label=Append");
    $sel->select_ok("pool", "label=Scratch");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Vol0001");
    $sel->click_ok("media");
    $sel->click_ok("//button[\@name='action' and \@value='update_media']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->select_ok("volstatus", "label=Archive");
    $sel->select_ok("enabled", "label=no");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("arrow_0");
    $sel->is_text_present_ok("New Volume status is: Archive");
    $sel->click_ok("link=All Media");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Archive");
    $sel->select_ok("volstatus", "label=Append");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("media");
    $sel->click_ok("//button[\@name='action' and \@value='media_zoom']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Volume Infos");

    $sel->click_ok("//button[\@name='action' and \@value='prune']");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("The current Volume retention period");

    $sel->go_back_ok();
    $sel->is_text_present_ok("Volume Infos");
    $sel->click_ok("//button[\@name='action' and \@value='purge']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Marking it purged");
}

if ($part{missingjob}) {
# view missing jobs
    $sel->click_ok("link=Main");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("Running Jobs");

    $sel->click_ok("link=Missing Jobs");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("BackupClient1");
    $sel->is_text_present_ok("BackupCatalog");
    $sel->click_ok("job");
    $sel->click_ok("//input[\@name='job' and \@value='BackupCatalog']");
    $sel->click_ok("//button[\@name='action' and \@value='job']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->body_text_isnt("BackupCatalog");
}

if ($part{run}) {
# run a new job
    $sel->click_ok("link=Main");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("Running Jobs");

    $sel->click_ok("link=Defined Jobs");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->select_ok("job", "label=BackupClient1");
    $sel->is_text_present_ok("BackupClient1");
    $sel->click_ok("//button[\@name='action' and \@value='run_job_mod']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Default");
    $sel->is_text_present_ok("Full Set");
    $sel->is_text_present_ok("Incremental");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

  WAIT: {
      for (1..60) {
          if (eval { $sel->is_text_present("Start Backup JobId") }) { pass; last WAIT }
          sleep(1);
      }
    fail("timeout");
    }
    $sel->is_text_present_ok("Log: BackupClient1 on");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

  WAIT: {
      for (1..60) {
          if (eval { $sel->is_text_present("Termination: Backup OK") }) { pass; last WAIT }
          sleep(1);
      }
      fail("timeout");
    }
    my $volume = $sel->get_text("//tr[\@id='even_row']/td[12]");
    $sel->click_ok("//button[\@name='action' and \@value='media']");
    $sel->wait_for_page_to_load_ok("30000");

    my $volume_found = $sel->get_text("//tr[\@id='even_row']/td[1]");
    $sel->click_ok("media");
    $sel->text_is("//tr[\@id='even_row']/td[5]", "Append");
    $sel->click_ok("//button[\@name='action' and \@value='media_zoom']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Volume Infos");
    $sel->click_ok("//img[\@title='terminated normally']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//button[\@name='action' and \@value='fileset_view']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("FileSet Full Set");
    $sel->is_text_present_ok("What is included:");
    $sel->is_text_present_ok("/regress/build");
    $sel->go_back_ok();
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//button[\@name='action' and \@value='run_job_mod']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_element_present_ok("level");
    $sel->selected_value_is("name=level", "Full");
    $sel->selected_label_is("name=fileset", "Full Set");
    $sel->selected_label_is("name=job", "BackupClient1");
    $sel->click_ok("//button[\@name='action' and \@value='fileset_view']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("FileSet Full Set");
    $sel->is_text_present_ok("/regress/build");
}

if ($part{group}) {
# test group
    $sel->click_ok("link=Main");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("Running Jobs");

    $sel->click_ok("link=Groups");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->text_is("//h1", "Groups");
    $sel->click_ok("//button[\@name='action' and \@value='groups_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->type_ok("newgroup", "All");
    $sel->select_ok("name=client", "index=0");
    $sel->click_ok("//button[\@name='action' and \@value='groups_save']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("All");
    $sel->click_ok("//input [\@name='client_group' and \@value='All']");
    $sel->click_ok("//button[3]");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Group: 'All'");
    $sel->selected_index_is("name=client", "0");
    $sel->click_ok("//button[\@name='action' and \@value='groups_save']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//input[\@name='client_group' and \@value='All']");
    $sel->click_ok("//button[4]");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//input[\@name='client']");
    $sel->click_ok("//button[\@name='action' and \@value='client_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("All");
    $sel->click_ok("link=Groups");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//button[\@name='action' and \@value='groups_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->type_ok("newgroup", "Empty");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("client_group");
    $sel->is_text_present_ok("Empty");
    $sel->click_ok("//button[\@name='action' and \@value='client']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//input[\@name='client']");
    $sel->click_ok("//button[\@name='action' and \@value='client_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("link=Groups");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//input[\@name='client_group' and \@value='Empty']");
    $sel->click_ok("//button[\@name='action' and \@value='groups_del']");
    ok($sel->get_confirmation() =~ /^Do you want to delete this group[\s\S]$/);
    $sel->click_ok("link=Groups");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->type_ok("newgroup", "Empty");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Empty");
    $sel->click_ok("//input[\@name='client_group' and \@value='All']");
    $sel->click_ok("//button[\@name='action' and \@value='client']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//input[\@name='client']");
    $sel->click_ok("//button[\@name='action' and \@value='client_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    ok(not $sel->is_checked("//input[\@name='client_group' and \@value='Empty']"));

    # click on Statistics -> Groups
    $sel->click_ok("//ul[\@id='menu']/li[6]/ul/li[3]/a");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("All");
    $sel->is_text_present_ok("Empty");
    $sel->click_ok("link=Groups");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//input[\@name='client_group' and \@value='Empty']");
    $sel->click_ok("document.forms[1].elements[4]");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->type_ok("newgroup", "EmptyGroup");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("EmptyGroup");
}

if ($part{location}) {
# test location
    $sel->click_ok("link=Main");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("Running Jobs");

    $sel->click_ok("link=Locations");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//button[\@name='action' and \@value='location_add']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->type_ok("location", "Bank");
    $sel->click_ok("//button[\@name='action']"); # save
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Bank");
    $sel->click_ok("location");
    $sel->click_ok("//button[\@name='action' and \@value='location_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->type_ok("cost", "100");
    $sel->is_text_present_ok("Location: Bank");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("100");
    $sel->is_element_present_ok("//img[\@src='/bweb/inflag1.png']");
    $sel->click_ok("location");
    $sel->click_ok("//button[\@name='action' and \@value='location_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->select_ok("enabled", "label=no");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_element_present_ok("//img[\@src='/bweb/inflag0.png']");
    $sel->click_ok("location");
    $sel->click_ok("//button[\@name='action' and \@value='location_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->select_ok("enabled", "label=archived");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_element_present_ok("//img[\@src='/bweb/inflag2.png']");
    $sel->click_ok("location");
    $sel->click_ok("//button[\@name='action' and \@value='location_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->type_ok("newlocation", "Office");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Office");
    $sel->click_ok("link=All Media");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("media");
    $sel->click_ok("//button[\@name='action' and \@value='update_media']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->select_ok("location", "label=Office");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->selected_value_is("location", "Office");
    $sel->click_ok("//button[\@name='action' and \@value='media']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->select_ok("location", "label=Office");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->body_text_isnt("Vol0010");
    $sel->click_ok("link=Locations");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("location");
    $sel->click_ok("//button[\@name='action' and \@value='location_del']");
    ok($sel->get_confirmation() =~ /^Do you want to remove this location[\s\S]$/);
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Sorry, the location must be empty");
    $sel->click_ok("link=Locations");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->type_ok("location", "OtherPlace");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("location");
    $sel->click_ok("//button[\@name='action' and \@value='media']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("media");
    $sel->click_ok("//button[\@name='action' and \@value='update_media']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->select_ok("location", "label=OtherPlace");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("link=Locations");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("location");
    $sel->click_ok("//button[\@name='action' and \@value='location_del']");
    ok($sel->get_confirmation() =~ /^Do you want to remove this location[\s\S]$/);
    $sel->wait_for_page_to_load_ok("30000");

    $sel->body_text_isnt("Office");
}

if ($part{overview}) {
    unless ($client) {
        $sel->click_ok("link=Clients");
        $sel->wait_for_page_to_load_ok("30000");
        $client = $sel->get_text("//tr[\@id='even_row']/td[1]");
    }
    $sel->click_ok("link=Main");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("Running Jobs");

    $sel->click_ok("link=Jobs overview");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("All");
    $sel->click_ok("link=All");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("link=$client");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("BackupClient1");
    $sel->is_text_present_ok("Full Set");
}

if ($part{config}) {
    my ($dbi, $user, $pass, $histo);
    $sel->open_ok("/cgi-bin/bweb/bweb.pl");
    $sel->click_ok("link=Configuration");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("SQL Connection");

    $sel->click_ok("//button[\@name='action' and \@value='edit_conf']");
    $sel->wait_for_page_to_load_ok("30000");
    $dbi = $sel->get_value("dbi");
    $user = $sel->get_value("user");
    $pass = $sel->get_value("password");
    $histo = $sel->get_value("stat_job_table");

    $sel->type_ok("dbi", "dbi:Pg:database=dbi1");
    $sel->type_ok("user", "user1");
    $sel->type_ok("password", "password1");

    $sel->type_ok("stat_job_table", ($histo eq 'Job')?"JobHisto":"Job");

    $sel->click_ok("//button[\@name='action' and \@value='apply_conf']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//button[\@name='action' and \@value='edit_conf']");
    $sel->wait_for_page_to_load_ok("30000");

    is($sel->get_value("dbi"), "dbi:Pg:database=dbi1", "verify dbi");
    is($sel->get_value("user"), "user1", "verify user");
    is($sel->get_value("password"), "password1", "verify passwd");

    $sel->type_ok("dbi", $dbi);
    $sel->type_ok("user", $user);
    $sel->type_ok("password", $pass);
    $sel->click_ok("//button[\@name='action' and \@value='apply_conf']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("SQL Connection");

    if(0) {                     # not used for now
    # test create conf
        $sel->click_ok("//button[\@name='action' and \@value='add_conf']");
        $sel->wait_for_page_to_load_ok("30000");
        $sel->is_text_present_ok("Unnamed");

        $sel->type_ok("name", "MyBweb2");
        $sel->type_ok("new_dir", "bweb2-dir");

        $sel->type_ok("dbi", $dbi);
        $sel->type_ok("user", $user);
        $sel->type_ok("password", $pass);

        $sel->type_ok("stat_job_table", "JobHisto");
        $sel->click_ok("//button[\@name='action' and \@value='apply_conf']");
        $sel->wait_for_page_to_load_ok("30000");
        $sel->is_text_present_ok("MyBweb2");
        $sel->is_text_present_ok("bweb2-dir");

        # test create other conf
        $sel->click_ok("//button[\@name='action' and \@value='view_main_conf']");
        $sel->wait_for_page_to_load_ok("30000");
        $sel->is_text_present_ok("Main Configuration");

        $sel->click_ok("//button[\@name='action' and \@value='add_conf']");
        $sel->wait_for_page_to_load_ok("30000");
        $sel->is_text_present_ok("Unnamed");

        $sel->type_ok("name", "MyBweb3");
        $sel->type_ok("new_dir", "bweb3-dir");
        $sel->type_ok("user", "test");

        $sel->click_ok("//button[\@name='action' and \@value='apply_conf']");
        $sel->wait_for_page_to_load_ok("30000");
        $sel->is_text_present_ok("MyBweb3");

        $sel->click_ok("//button[\@name='action' and \@value='view_main_conf']");
        $sel->wait_for_page_to_load_ok("30000");
        $sel->is_text_present_ok("Main Configuration");
        $sel->is_text_present_ok("MyBweb3");
        $sel->is_text_present_ok("MyBweb2");

        # test rename
        $sel->click_ok("//input[\@name='dir' and \@value='bweb2-dir']");
        $sel->click_ok("//button[\@name='action' and \@value='view_conf']");
        $sel->wait_for_page_to_load_ok("30000");
        $sel->is_text_present_ok("MyBweb2");
        $sel->click_ok("//button[\@name='action' and \@value='edit_conf']");
        $sel->wait_for_page_to_load_ok("30000");
        $sel->is_text_present_ok("MyBweb2");
        $sel->type_ok("new_dir", "bweb-dir");
        $sel->click_ok("//button[\@name='action' and \@value='apply_conf']");
        $sel->wait_for_page_to_load_ok("30000");
        $sel->is_text_present_ok("MyBweb2");
        $sel->is_text_present_ok("bweb-dir");

        # test delete
        $sel->click_ok("//button[\@name='action' and \@value='view_main_conf']");
        $sel->wait_for_page_to_load_ok("30000");
        $sel->is_text_present_ok("MyBweb3");

        $sel->click_ok("//input[\@name='dir' and \@value='bweb3-dir']");
    $sel->click_ok("//button[\@name='action' and \@value='del_conf']");
        ok($sel->get_confirmation() =~ /^Do you really want to remove this Director[\s\S]$/);
    $sel->wait_for_page_to_load_ok("30000");
        $sel->body_text_isnt("MyBweb3");
        $sel->is_text_present_ok("MyBweb2");

        $sel->click_ok("//input[\@name='dir' and \@value='bweb-dir']");
        $sel->click_ok("//button[\@name='action' and \@value='del_conf']");
        ok($sel->get_confirmation() =~ /^Do you really want to remove this Director[\s\S]$/);
        $sel->wait_for_page_to_load_ok("30000");
        $sel->body_text_isnt("MyBweb2");
    }
}

if ($part{multidir}) {
    # cleanup
    $sel->click_ok("link=Configuration");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->click_ok("//input[\@name='dir' and \@value='bweb1-dir']");
    $sel->click_ok("//button[\@name='action' and \@value='del_conf']");
    ok($sel->get_confirmation() =~ /^Do you really want to remove this Director[\s\S]$/);
    $sel->wait_for_page_to_load_ok("30000");
    $sel->body_text_isnt("MyBweb");

    if (-f "$tmp/bconsole.conf.$$") {
        move("$tmp/bconsole.conf.$$", "$bin/bconsole.conf");
    }
}
