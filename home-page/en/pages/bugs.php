<? require_once("inc/header.php"); ?>
<table>
<tr>
        <td class="contentTopic">
        Bug Reporting
        </td>
</tr>
<tr>
    <td class="content">
Before reporting a bug, please be sure it is a bug and
not a request for support or a feature request (see the menu
bar at the left of this page).

<p>Bugs are resolved by volunteer Bacula developers (and the
community at large). If you have a high priority for your bug or
need professional help, please see the
 <a href="http://www.bacula.org/en/?page=professional">
Professional Services</a> page of this website.

<p>Bacula now has a Mantis bug reporting system
implemented by Dan Langille and hosted on his computer. It is web based,
easy to use, and we recommend you give it a try.  However, before
visiting our bugs database, please carefully read the following:


To view the bug reports, you can login as user <b>anonymous</b> and
password <b>anonymous</b>. The advantage of actually being subscribed
is that you will be notified by email of any serious bugs and their
resolution.

To submit bug reports, you must create an account.  You must also use a
browser running a US ASCII code page or UTF-8.  Some users running Win32
IE with Windows Eastern European code pages have experienced problems
interfacing with the system.

<p>
Most Bacula problems are questions of support, so if you are not
sure if a problem you are having is a bug, see the support page
on this site for links to the email lists.  However, once you have
determined that a problem is a bug, you must either submit a bug
report to the bugs database or send an email to the bacula-devel
list, otherwise it is possible that the developers will never know
about your bug and thus it will not get fixed.

You should expect two things to be slightly different in our Bugs
handling than many other Open Source projects.  First, we unfortunately
cannot give support or handle feature requests via the bugs database,
and second, we close bugs very quickly to avoid being overwhelmed.
Please don't take this personally.  If you want to add a note to the bug
report after it is closed, you can do so by reopening the bug, adding a
bug note, then closing the bug report again, or for really simple
matters, you can send an email to the bacula-devel email list.  If a
developer closes a bug report and after everything considered, you are
convinced there really *is* a bug and you have new information, you can
always reopen the bug report.

<h3>Information Needed in a Bug Report</h3>
For us to respond to a bug report, we normally need the following
as the minimum information, which you should enter into the appropriate
fields of the bug reporting system:
<ul>
<li>Your operating system</li>
<li>The version of Bacula you are using</li>
<li>A <a href="http://www.chiark.greenend.org.uk/~sgtatham/bugs.html">clear and concise</a> description of the problem</li>
<li>If you say &quot;it crashes&quot;, &quot;it doesn't work&quot; or something
  similar, you should include some output from Bacula that shows this.</li>
</ul>
If you are having tape problems, please include:
<ul>
 <li>The kind of tape drive you have </li>
 <li>Have you run the <b>btape</b> &quot;test&quot; command?</li>

</ul>
The first two of these items can be fulfilled by sending us a copy of
your <b>config.out</b> file, which is in the main <b>Bacula</b> source
directory after you have done your <b>./configure</b>.

<p>In addition, we will sometimes need a copy of your Bacula
configuration files (especially bacula-dir.conf).  If you think it is a
configuration problem, please don't hesitate to send them if
necessary.</td>
</tr>
<tr>
   <td style="font-size: 14px; padding: 0px 20px 0px 20px">
   Please read that little Bug-Report-<a href="http://www.chiark.greenend.org.uk/~sgtatham/bugs.html">HowTo</a> as well.
   </td>
</tr>
<tr><td class="content">
You can submit a bug or review the list of open or closed bugs by going to:

<p style="text-align: center; font-size: 24px">
        <a href="http://bugs.bacula.org">http://bugs.bacula.org</a>
</p>
</td></tr>

</table>
<? require_once("inc/footer.php"); ?>
