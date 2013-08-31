<? require_once("inc/header.php"); ?>
<table>
<tr>
   <td class="contentTopic"> Support for Bacula </td>
</tr>
<tr>
   <td class="content">


   <h3>Professional Support</h3>
   If you need professional support, please see the 
   <a href="http://www.bacula.org/en/?page=professional">
   professional support</a> page of this website.<br>

   <h3>Community Support</h3>
   Please keep in mind that no one is getting paid for this.
   Nevertheless, our desire is to see as many people using
   <b>Bacula</b> as possible. A number of very knowledgeable volunteers are 
   willing to provide a reasonable level of email support.

   <p>Before asking for help, please read the <b>Information Needed</b>
   listed below, and it could be useful to check against the
   email archive as often solution to your problem has been discussed
   or a patch has been released. Please see:
   <a href="http://news.gmane.org/search.php?match=bacula">
   http://news.gmane.org/search.php?match=bacula</a>.

   <p>Also, if you are using Bacula in production, we highly recommend
   subscribing to the bugs database at: <a href="http://bugs.bacula.org">
   http://bugs.bacula.org</a> to keep informed of problems and 
   patches. You might also want to look for professional support at: 
   <a href="http://www.bacula.org/en/?page=professional">
      http://www.bacula.org/en/?page=professional</a>.<br>


   <p>Please do not submit support requests to the bugs database.
   For more information on bugs, please see the <a 
   href=http://www.bacula.org/en/?page=bugs>Bugs page</a> on this
   web site.

   <p>For community support, send an email to
   <b>bacula-users at lists.sourceforge.net</b>, and if you are
   specific enough, some kind Bacula user will help you. Please
   note that if you don't at least specify what version of Bacula
   and what platform you are using, it will not be easy to get a
   valid answer.  The email address noted above
   was modified to prevent easy use by spammers.
   To use it, you must replace the <b>at</b> with an @ symbol. Due
   to the increasing volumes of spam on the list, you must 
   be subscribed to it to be able to send and email to it.  The
   link to your left entitled <b>Email Lists</b> provides links
   to where you can subscribe to each of the Bacula email lists.
    
   The users constantly monitor this list and will generally provide
   support. Please see <b>Information Needed</b> below for what to
   include in your support request. If you don't supply the necessary
   information, it will take longer to respond to your request, and
   users may be afraid to try to respond,
   if your request is too complicated or not well formulated.


   <p>I (Kern) get a number of &quot;off-list&quot; emails sent
   directly to me. Unfortunately, I am no longer able to provide
   direct user support. However, I do read all the email sent and
   occasionally provide a tip or two. If you do send email to me,
   please always copy the appropriate list, if you
   do not copy the list, I may not answer you, or I will answer by copying
   the list. If you <em>really</em> have something confidential,
   please clearly indicate it.</p>

   <p>Please do not send general support requests to the bacula-devel list.
   You may send a preliminary bug question, a development question,  
   or minor enhancement request to the bacula-devel list.  If 
   you do not provide the information
   requested below, particularly the Bacula version, it is <b>very</b>
   frustrating for us, because it is quite often the case that your problem is
   version dependent, and possibly already fixed.  In such case, we will note
   the problem, but you will be unlikely to get a response, especially if we
   are busy, because it forces us to first ask you what version you are using
   (or other information), then deal with your response, thus doubling the
   time for us.  If we do ask you for information, please include <b>all</b>
   the previous correspondence in each email, to avoid us having to search
   the archives to find what you previously wrote.  In short, if you want a
   response, please see &quot;Information Needed&quot; below.

   <p>If you are looking for live-support you might check out our irc-channel
   in the <a href="http://www.freenode.net">Freenode</a> net, called #bacula.

<h3>Information Needed</h3>
For us to respond to a bug report, we normally need the following
as the minimum information, which you can enter into the appropriate
fields of the bug reporting system:
<ul>
<li>Your operating system</li>
<li>The version of Bacula you are using</li>
<li>A <a href="http://www.chiark.greenend.org.uk/~sgtatham/bugs.html">clear and concise</a> description of the problem</li>
<li>If you say &quot;it crashes&quot;, &quot;it doesn't work&quot; or something
  similar, you should include some output from Bacula that shows this.</li>
<li>If we respond to your email, and you answer, possibly supplying more
  information, please be sure to include the <b>full</b> text of previous
  emails so that we have all the information in one place.</li>
</ul>
If you are having tape problems, please include:
<ul>
 <li>The kind of tape drive you have </li>
 <li>Have you run the <b>btape</b> &quot;test&quot; command?</li>
</ul>

If you are having database problems, please include:
<ul>
 <li>The database you are using: MySQL, PostgreSQL, SQLite, SQLite3 </li>
 <li>The version of the database you are using</li>
</ul>

The first two of these items can be fulfilled by sending
us a copy of your <b>config.out</b> file, which is in the
main <b>Bacula</b> source directory after you have done
your <b>./configure</b>.
<p>In addition, we will sometimes need a copy of your Bacula
configuration files (especially bacula-dir.conf). If you
think it is a configuration problem, please don't hesitate
to send them if necessary.</td>
</tr>
<tr>
   <td style="font-size: 14px; padding: 0px 20px 0px 20px">
   Please read that little Bug-Report-<a href="http://www.chiark.greenend.org.uk/~sgtatham/bugs.html">HowTo</a> as well.
   </td>
</tr>

</table>
<? require_once("inc/footer.php"); ?>
