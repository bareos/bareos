<? require_once("inc/header.php"); ?>
<table>
<tr>
   <td class="contentTopic"> Feature Requests </td>
</tr>
<tr>
   <td class="content">

<h2>Funded Development</h2>
Now that <a href="http://www.baculasystems.com">Bacula Systems SA</a> exists,
it is possible to sponsor funded development projects. This is a contractual
relationship where the Bacula Systems developers agree to implement
a specific project within a specific time frame. All code that is developed
by Bacula Systems developers, goes into the Bacula community source
repository, so is available for everyone to use.

<h2>Community Development</h2>
If you are not interested in sponsoring a development project, you can
nevertheless submit a feature request to have a favorite feature
implemented (and even submit your own patch for it).  

<p>In the past, users informally submitted feature requests by email, and 
we collected them, then once a version was released, we would publish the
list for users to vote on.
<p>
Now that Bacula has become a bigger project, this process has been 
formalized a bit more. The main change is for users
to carefully think about their feature, and submit it on a feature
request form. A mostly empty form is shown below along with an 
example of an actual filled in form. A text copy of the form can
be found in the <b>projects</b> file in the main source directory
of the Bacula release. That file also contains a list of all the
currently approved projects and their status.
<p>
The best time to submit a Feature Request is just after a release when
we officially request feature requests for the next version. The worst
time to submit a feature request is just prior to a new release (we are
very busy at that time).  To actually submit the Feature request,
fill out the form, and submit it to both the bacula-users and
the bacula-devel email lists. It will then be openly discussed.
<p>
Once the Feature Request has beeen adequately discussed, Bacula Project
Manager (Kern) will either reject it, approve it, or possibly request some
modifications.  If you plan to implement the feature or donate
funds to have it implemented, this is important to note,
otherwise, the feature, even if approved, may wait a long time
for someone to implement it.
<p>
Once the Feature request is approved, we will add it to the projects
file, which contains a list of all open Feature Requests.  The projects
file is updated from time to time
<p>
The current (though possibly somewhat old) list of projects can also
be found on the Web site by clicking on the Projects menu item to the
left of this window.
                     

<h3>Feature Request Form</h3>
<pre>
Item n:   One line summary ...
  Origin: Name and email of originator.
  Date:   Date submitted (e.g. 28 October 2005)
  Status:

  What:   More detailed explanation ...

  Why:    Why it is important ...

  Notes:  Additional notes or features ...

</pre>

<h3>An Example Feature Request</h3>
<pre>
Item 1:   Implement a Migration job type that will move the job
          data from one device to another.
  Date:   28 October 2005
  Origin: Sponsored by Riege Sofware International GmbH. Contact:
          Daniel Holtkamp <holtkamp at riege dot com>
  Status: Partially coded in 1.37 -- much more to do. Assigned to
          Kern.

  What:   The ability to copy, move, or archive data that is on a
          device to another device is very important.

  Why:    An ISP might want to backup to disk, but after 30 days
          migrate the data to tape backup and delete it from
          disk.  Bacula should be able to handle this
          automatically.  It needs to know what was put where,
          and when, and what to migrate -- it is a bit like
          retention periods.  Doing so would allow space to be
          freed up for current backups while maintaining older
          data on tape drives.

  Notes:  Migration could be triggered by:
           Number of Jobs
           Number of Volumes
           Age of Jobs
           Highwater size (keep total size)
           Lowwater mark

</pre>

</td>
</tr>

</table>
<? require_once("inc/footer.php"); ?>
