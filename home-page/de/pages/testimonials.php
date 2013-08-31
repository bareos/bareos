<? require_once("inc/header.php"); ?>
<table>
<tr>
        <td class="contentTopic">
                Empfehlungsschreiben
        </td>
</tr>
<tr>
        <td class="content">
	Hier finden Sie einige Ausz&uuml;ge aus E-Mails die uns von Benutzer geschrieben wurden.
	Das Ziel ist es, Ihnen zu zeigen, an welchen Stellen Bacula l&auml;uft und wie es dort eingesetzt wird.
	Alle Ausz&uuml;ge werden mit Erlaubnis des Authors verwendet.
        </td>
</tr>

<tr>
        <td class="content">
        <h3 style="padding: 5px; border-bottom: 1px dotted #002244"> Norm Dressler - 2004/06/15 </h3>
        Bacula has been awesome for us. We used to use Ar**** but I
        have always hated the interface. And the cost was outrageous.
        Then I found bacula and wow! Everything in Ar**** and then
        some! The console is easy to use, easy to understand once you
        get the hang of it, and I usually don't have any problems
        restoring files! :)
        <br>
        I have 15+ machines I backup with Bacula
        with an autoloader, and I'm extremely happy with the product.
        Whenever someone asks me about what to use, I point them at
        Bacula.
        <p> Norm Dressler, Senior Network Architect</p>
        </td>
</tr>

<tr>
        <td class="content">
        <h3 style="padding: 5px; border-bottom: 1px dotted #002244"> Michael Scherer - 2005/02/09 </h3>
        Our former backup-system was ARGHserve running on NT4.
        Due to the fact that we replaced most of our servers with
        Linux machines we had to find some other solution.<br> At
        first we tried ARGHserve on Linux, without much comfort.
        Database updates took days to complete, the database
        itself grew enormously large, ...  not really something
        you except from such an expensive piece of software.<br>
        I began the quest for a new backup-solution, testing
        almost anything I could find on Sourceforge and Freshmeat
        and finally decided to go with Bacula.<br> It's perfectly
        maintained by Kern and many others, is an (very) active
        project with good support through maillists and an
        irc-channel, which can be found on Freenode.<br>
        <br>
        Today we run Bacula on a SuSE based x86 machine with a
        2.6 kernel, some RAID5-Systems and IBM Ultrium
        Tapelibrary.  Without any issues and without more work
        than changing two tapes in the morning.<br>
        <br>
        Recoveries aren't complicated as well, you either choose
        a jobid to restore from or let Bacula find the correct
        jobid for a file or directory you need to recover.  You
        mark everything you need, Bacula tells you which tapes it
        needs, done.<br>
        You don't even have to wait for any daemon to finish
        database updates for backuped files, you can start with
        the recovery right after the backup-job is done.
        Perfect.  <br>

        <br>
        <p> Michael Scherer, some admin </p>
        </td>
</tr>
<tr>
        <td class="content">
        <h3 style="padding: 5px; border-bottom: 1px dotted #002244"> Ludovicz Strappazon - 2005/03/05</h3>
                I had previously used Veritas Netbackup, but it was really too expensive
                for our University. Bacula permitted us to buy a library. Now, we use
                Bacula since 10/2002 with an ADIC Scalar 24 library and LTO ultrium,
                without any problems. I think it can do anything Netbackup could do with our
                configuration. We backup seven Linux servers, one NT server, four
                Windows 2003 servers and a few XP workstations. Some of these servers
                are backed up across a firewall using ssh; some others are on a private
                network. We tried succesfully the disaster recovery procedure on Linux
                and had some good results in restoring Windows "from bare metal". What do I
                like in Bacula ?  It is very flexible and reliable. With its light
                interface console, I can manage the backups from everywhere. A few words
                about the support : it is free but efficient. I don't have to cross a
                level 1, level 2 helpdesk to have some help, I never felt alone, and the
                bacula-users mailing list is a mix of courtesy and honest speech. At
                last, you don't need to be a big company to have your features requests
                heard.<br>
                Thanks to Kern Sibbald and the others who give so much work and time
                for this project.<br>
                <br>
                Ludovic Strappazon<br>
                University Marc Bloch de Strasbourg.<br>
        </td>
</tr>

<tr>
  <td class="content">
  <h3 style="padding: 5px; border-bottom: 1px dotted #002244"> Jeff Richards - 2006/08/26</h3>
      I used Bacula at my previous employer to backup:  Linux, OpenBSD, Windows 2000,XP,2003, and AIX 5.1 
      <br>  <br>
      Bacula provided a solution when I had no budget for backup
      software.  The Linux systems (about 30) acted as Tivoli (TMF)
      gateways.  The Linux systems were commodity PCs, so when the IDE
      HDs failed it took hours (usually at least 4) to clean up the
      Tivoli environment and rebuild the failed gateway.  Using Bacula
      and mkCDrec I cut that time down to under an hour, and most of
      that time was not spent doing anything except waiting for the
      restore to finish.  I recovered 2 failed Linux systems with
      Bacula.
      <br>     
      I would like to thank you and the entire Bacula team for an
      excellent piece of software.
         <br> <br>
         Jeff Richards<br>
         Consultant<br>
  </td>
</tr>

</table>

<? require_once("inc/footer.php"); ?>
