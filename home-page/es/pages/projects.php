<? require_once("inc/header.php"); ?>
<table>
</tr>
<tr>
        <td class="content">

<pre>
                
                
                
Projects:
                     Bacula Projects Roadmap 
                    Status updated 7 July 2007
                   After re-ordering in vote priority

Items Completed:
Item:   2   Implement a Bacula GUI/management tool.
Item:  18   Quick release of FD-SD connection after backup.
Item:  23   Implement from-client and to-client on restore command line.
Item:  25   Implement huge exclude list support using dlist   
Item:  41   Enable to relocate files and directories when restoring
Item:  42   Batch attribute inserts (ten times faster)
Item:  43   More concurrency in SD using micro-locking
Item:  44   Performance enhancements (POSIX/Win32 OS file access hints).
Item:  40   Include JobID in spool file name

Summary:
Item:   1   Accurate restoration of renamed/deleted files
Item:   2*  Implement a Bacula GUI/management tool.
Item:   3   Allow FD to initiate a backup
Item:   4   Merge multiple backups (Synthetic Backup or Consolidation).
Item:   5   Deletion of Disk-Based Bacula Volumes
Item:   6   Implement Base jobs.
Item:   7   Implement creation and maintenance of copy pools
Item:   8   Directive/mode to backup only file changes, not entire file
Item:   9   Implement a server-side compression feature
Item:  10   Improve Bacula's tape and drive usage and cleaning management.
Item:  11   Allow skipping execution of Jobs
Item:  12   Add a scheduling syntax that permits weekly rotations
Item:  13   Archival (removal) of User Files to Tape
Item:  14   Cause daemons to use a specific IP address to source communications
Item:  15   Multiple threads in file daemon for the same job
Item:  16   Add Plug-ins to the FileSet Include statements.
Item:  17   Restore only file attributes (permissions, ACL, owner, group...)
Item:  18*  Quick release of FD-SD connection after backup.
Item:  19   Implement a Python interface to the Bacula catalog.
Item:  20   Archive data
Item:  21   Split documentation
Item:  22   Implement support for stacking arbitrary stream filters, sinks.
Item:  23*  Implement from-client and to-client on restore command line.
Item:  24   Add an override in Schedule for Pools based on backup types.
Item:  25*  Implement huge exclude list support using hashing.
Item:  26   Implement more Python events in Bacula.
Item:  27   Incorporation of XACML2/SAML2 parsing
Item:  28   Filesystem watch triggered backup.
Item:  29   Allow inclusion/exclusion of files in a fileset by creation/mod times
Item:  30   Tray monitor window cleanups
Item:  31   Implement multiple numeric backup levels as supported by dump
Item:  32   Automatic promotion of backup levels
Item:  33   Clustered file-daemons
Item:  34   Commercial database support
Item:  35   Automatic disabling of devices
Item:  36   An option to operate on all pools with update vol parameters
Item:  37   Add an item to the restore option where you can select a pool
Item:  38   Include timestamp of job launch in "stat clients" output
Item:  39   Message mailing based on backup types
Item:  40*  Include JobID in spool file name
Item:  41*  Enable to relocate files and directories when restoring
Item:  42*  Batch attribute inserts (ten times faster)
Item:  43*  More concurrency in SD using micro-locking
Item:  44*  Performance enhancements (POSIX/Win32 OS file access hints).

Item  1:  Accurate restoration of renamed/deleted files
  Date:   28 November 2005
  Origin: Martin Simmons (martin at lispworks dot com)
  Status: Robert Nelson will implement this

  What:   When restoring a fileset for a specified date (including "most
          recent"), Bacula should give you exactly the files and directories
          that existed at the time of the last backup prior to that date.

          Currently this only works if the last backup was a Full backup.
          When the last backup was Incremental/Differential, files and
          directories that have been renamed or deleted since the last Full
          backup are not currently restored correctly.  Ditto for files with
          extra/fewer hard links than at the time of the last Full backup.

  Why:    Incremental/Differential would be much more useful if this worked.

  Notes:  Merging of multiple backups into a single one seems to
          rely on this working, otherwise the merged backups will not be
          truly equivalent to a Full backup.  

          Kern: notes shortened. This can be done without the need for 
          inodes. It is essentially the same as the current Verify job,
          but one additional database record must be written, which does 
          not need any database change.

          Kern: see if we can correct restoration of directories if
          replace=ifnewer is set.  Currently, if the directory does not
          exist, a "dummy" directory is created, then when all the files
          are updated, the dummy directory is newer so the real values
          are not updated.

Item  2:  Implement a Bacula GUI/management tool.
  Origin: Kern
  Date:   28 October 2005
  Status: In progress

  What:   Implement a Bacula console, and management tools
          probably using Qt3 and C++.

  Why:    Don't we already have a wxWidgets GUI?  Yes, but
          it is written in C++ and changes to the user interface
          must be hand tailored using C++ code. By developing
          the user interface using Qt designer, the interface
          can be very easily updated and most of the new Python       
          code will be automatically created.  The user interface
          changes become very simple, and only the new features
          must be implement.  In addition, the code will be in
          Python, which will give many more users easy (or easier)
          access to making additions or modifications.

 Notes:   There is a partial Python-GTK implementation
          Lucas Di Pentima <lucas at lunix dot com dot ar> but
          it is no longer being developed.

Item  3:  Allow FD to initiate a backup
  Origin: Frank Volf (frank at deze dot org)
  Date:   17 November 2005
  Status:

   What:  Provide some means, possibly by a restricted console that
          allows a FD to initiate a backup, and that uses the connection
          established by the FD to the Director for the backup so that
          a Director that is firewalled can do the backup.

   Why:   Makes backup of laptops much easier.


Item  4:  Merge multiple backups (Synthetic Backup or Consolidation).
  Origin: Marc Cousin and Eric Bollengier 
  Date:   15 November 2005
  Status: Waiting implementation. Depends on first implementing 
          project Item 2 (Migration) which is now done.

  What:   A merged backup is a backup made without connecting to the Client.
          It would be a Merge of existing backups into a single backup.
          In effect, it is like a restore but to the backup medium.

          For instance, say that last Sunday we made a full backup.  Then
          all week long, we created incremental backups, in order to do
          them fast.  Now comes Sunday again, and we need another full.
          The merged backup makes it possible to do instead an incremental
          backup (during the night for instance), and then create a merged
          backup during the day, by using the full and incrementals from
          the week.  The merged backup will be exactly like a full made
          Sunday night on the tape, but the production interruption on the
          Client will be minimal, as the Client will only have to send
          incrementals.

          In fact, if it's done correctly, you could merge all the
          Incrementals into single Incremental, or all the Incrementals
          and the last Differential into a new Differential, or the Full,
          last differential and all the Incrementals into a new Full
          backup.  And there is no need to involve the Client.

  Why:    The benefit is that :
          - the Client just does an incremental ;
          - the merged backup on tape is just as a single full backup,
            and can be restored very fast.

          This is also a way of reducing the backup data since the old
          data can then be pruned (or not) from the catalog, possibly
          allowing older volumes to be recycled

Item  5:  Deletion of Disk-Based Bacula Volumes
  Date:   Nov 25, 2005
  Origin: Ross Boylan <RossBoylan at stanfordalumni dot org> (edited
          by Kern)
  Status:         

   What:  Provide a way for Bacula to automatically remove Volumes
          from the filesystem, or optionally to truncate them.
          Obviously, the Volume must be pruned prior removal.

  Why:    This would allow users more control over their Volumes and
          prevent disk based volumes from consuming too much space.

  Notes:  The following two directives might do the trick:

          Volume Data Retention = <time period>
          Remove Volume After = <time period>

          The migration project should also remove a Volume that is
          migrated. This might also work for tape Volumes.

Item  6:  Implement Base jobs.
  Date:   28 October 2005
  Origin: Kern
  Status: 
  
  What:   A base job is sort of like a Full save except that you 
          will want the FileSet to contain only files that are
          unlikely to change in the future (i.e.  a snapshot of
          most of your system after installing it).  After the
          base job has been run, when you are doing a Full save,
          you specify one or more Base jobs to be used.  All
          files that have been backed up in the Base job/jobs but
          not modified will then be excluded from the backup.
          During a restore, the Base jobs will be automatically
          pulled in where necessary.

  Why:    This is something none of the competition does, as far as
          we know (except perhaps BackupPC, which is a Perl program that
          saves to disk only).  It is big win for the user, it
          makes Bacula stand out as offering a unique
          optimization that immediately saves time and money.
          Basically, imagine that you have 100 nearly identical
          Windows or Linux machine containing the OS and user
          files.  Now for the OS part, a Base job will be backed
          up once, and rather than making 100 copies of the OS,
          there will be only one.  If one or more of the systems
          have some files updated, no problem, they will be
          automatically restored.

  Notes:  Huge savings in tape usage even for a single machine.
          Will require more resources because the DIR must send
          FD a list of files/attribs, and the FD must search the
          list and compare it for each file to be saved.

Item  7:  Implement creation and maintenance of copy pools
  Date:   27 November 2005
  Origin: David Boyes (dboyes at sinenomine dot net)
  Status:

  What:   I would like Bacula to have the capability to write copies
          of backed-up data on multiple physical volumes selected
          from different pools without transferring the data
          multiple times, and to accept any of the copy volumes
          as valid for restore.

  Why:    In many cases, businesses are required to keep offsite
          copies of backup volumes, or just wish for simple
          protection against a human operator dropping a storage
          volume and damaging it. The ability to generate multiple
          volumes in the course of a single backup job allows
          customers to simple check out one copy and send it
          offsite, marking it as out of changer or otherwise
          unavailable. Currently, the library and magazine
          management capability in Bacula does not make this process
          simple.

          Restores would use the copy of the data on the first
          available volume, in order of copy pool chain definition.

          This is also a major scalability issue -- as the number of
          clients increases beyond several thousand, and the volume
          of data increases, transferring the data multiple times to
          produce additional copies of the backups will become
          physically impossible due to transfer speed
          issues. Generating multiple copies at server side will
          become the only practical option. 

  How:    I suspect that this will require adding a multiplexing
          SD that appears to be a SD to a specific FD, but 1-n FDs
          to the specific back end SDs managing the primary and copy
          pools.  Storage pools will also need to acquire parameters
          to define the pools to be used for copies. 

  Notes:  I would commit some of my developers' time if we can agree
          on the design and behavior. 

Item  8:  Directive/mode to backup only file changes, not entire file
  Date:   11 November 2005
  Origin: Joshua Kugler <joshua dot kugler at uaf dot edu>
          Marek Bajon <mbajon at bimsplus dot com dot pl>
  Status: 

  What:   Currently when a file changes, the entire file will be backed up in
          the next incremental or full backup.  To save space on the tapes
          it would be nice to have a mode whereby only the changes to the
          file would be backed up when it is changed.

  Why:    This would save lots of space when backing up large files such as 
          logs, mbox files, Outlook PST files and the like.

  Notes:  This would require the usage of disk-based volumes as comparing 
          files would not be feasible using a tape drive.

Item  9:  Implement a server-side compression feature
  Date:   18 December 2006
  Origin: Vadim A. Umanski , e-mail umanski@ext.ru
  Status:
  What:   The ability to compress backup data on server receiving data
          instead of doing that on client sending data.
  Why:    The need is practical. I've got some machines that can send
          data to the network 4 or 5 times faster than compressing
          them (I've measured that). They're using fast enough SCSI/FC
          disk subsystems but rather slow CPUs (ex. UltraSPARC II).
          And the backup server has got a quite fast CPUs (ex. Dual P4
          Xeons) and quite a low load. When you have 20, 50 or 100 GB
          of raw data - running a job 4 to 5 times faster - that
          really matters. On the other hand, the data can be
          compressed 50% or better - so losing twice more space for
          disk backup is not good at all. And the network is all mine
          (I have a dedicated management/provisioning network) and I
          can get as high bandwidth as I need - 100Mbps, 1000Mbps...
          That's why the server-side compression feature is needed!
  Notes:

Item 10:  Improve Bacula's tape and drive usage and cleaning management.
  Date:   8 November 2005, November 11, 2005
  Origin: Adam Thornton <athornton at sinenomine dot net>,
          Arno Lehmann <al at its-lehmann dot de>
  Status:

  What:   Make Bacula manage tape life cycle information, tape reuse
          times and drive cleaning cycles.

  Why:    All three parts of this project are important when operating
          backups.
          We need to know which tapes need replacement, and we need to
          make sure the drives are cleaned when necessary.  While many
          tape libraries and even autoloaders can handle all this
          automatically, support by Bacula can be helpful for smaller
          (older) libraries and single drives.  Limiting the number of
          times a tape is used might prevent tape errors when using
          tapes until the drives can't read it any more.  Also, checking
          drive status during operation can prevent some failures (as I
          [Arno] had to learn the hard way...)

  Notes:  First, Bacula could (and even does, to some limited extent)
          record tape and drive usage.  For tapes, the number of mounts,
          the amount of data, and the time the tape has actually been
          running could be recorded.  Data fields for Read and Write
          time and Number of mounts already exist in the catalog (I'm
          not sure if VolBytes is the sum of all bytes ever written to
          that volume by Bacula).  This information can be important
          when determining which media to replace.  The ability to mark
          Volumes as "used up" after a given number of write cycles
          should also be implemented so that a tape is never actually
          worn out.  For the tape drives known to Bacula, similar
          information is interesting to determine the device status and
          expected life time: Time it's been Reading and Writing, number
          of tape Loads / Unloads / Errors.  This information is not yet
          recorded as far as I [Arno] know.  A new volume status would
          be necessary for the new state, like "Used up" or "Worn out".
          Volumes with this state could be used for restores, but not
          for writing. These volumes should be migrated first (assuming
          migration is implemented) and, once they are no longer needed,
          could be moved to a Trash pool.

          The next step would be to implement a drive cleaning setup.
          Bacula already has knowledge about cleaning tapes.  Once it
          has some information about cleaning cycles (measured in drive
          run time, number of tapes used, or calender days, for example)
          it can automatically execute tape cleaning (with an
          autochanger, obviously) or ask for operator assistance loading
          a cleaning tape.

          The final step would be to implement TAPEALERT checks not only
          when changing tapes and only sending the information to the
          administrator, but rather checking after each tape error,
          checking on a regular basis (for example after each tape
          file), and also before unloading and after loading a new tape.
          Then, depending on the drives TAPEALERT state and the known
          drive cleaning state Bacula could automatically schedule later
          cleaning, clean immediately, or inform the operator.

          Implementing this would perhaps require another catalog change
          and perhaps major changes in SD code and the DIR-SD protocol,
          so I'd only consider this worth implementing if it would
          actually be used or even needed by many people.

          Implementation of these projects could happen in three distinct
          sub-projects: Measuring Tape and Drive usage, retiring
          volumes, and handling drive cleaning and TAPEALERTs.

Item 11:  Allow skipping execution of Jobs
  Date:   29 November 2005
  Origin: Florian Schnabel <florian.schnabel at docufy dot de>
  Status:

    What: An easy option to skip a certain job  on a certain date.
     Why: You could then easily skip tape backups on holidays.  Especially
          if you got no autochanger and can only fit one backup on a tape
          that would be really handy, other jobs could proceed normally
          and you won't get errors that way.

Item 12:  Add a scheduling syntax that permits weekly rotations
   Date:  15 December 2006
  Origin: Gregory Brauer (greg at wildbrain dot com)
  Status:

   What:  Currently, Bacula only understands how to deal with weeks of the
          month or weeks of the year in schedules.  This makes it impossible
          to do a true weekly rotation of tapes.  There will always be a
          discontinuity that will require disruptive manual intervention at
          least monthly or yearly because week boundaries never align with
          month or year boundaries.

          A solution would be to add a new syntax that defines (at least)
          a start timestamp, and repetition period.

   Why:   Rotated backups done at weekly intervals are useful, and Bacula
          cannot currently do them without extensive hacking.

   Notes: Here is an example syntax showing a 3-week rotation where full
          Backups would be performed every week on Saturday, and an
          incremental would be performed every week on Tuesday.  Each
          set of tapes could be removed from the loader for the following
          two cycles before coming back and being reused on the third
          week.  Since the execution times are determined by intervals
          from a given point in time, there will never be any issues with
          having to adjust to any sort of arbitrary time boundary.  In
          the example provided, I even define the starting schedule
          as crossing both a year and a month boundary, but the run times
          would be based on the "Repeat" value and would therefore happen
          weekly as desired.


          Schedule {
              Name = "Week 1 Rotation"
              #Saturday.  Would run Dec 30, Jan 20, Feb 10, etc.
              Run {
                  Options {
                      Type   = Full
                      Start  = 2006-12-30 01:00
                      Repeat = 3w
                  }
              }
              #Tuesday.  Would run Jan 2, Jan 23, Feb 13, etc.
              Run {
                  Options {
                      Type   = Incremental
                      Start  = 2007-01-02 01:00
                      Repeat = 3w
                  }
              }
          }

          Schedule {
              Name = "Week 2 Rotation"
              #Saturday.  Would run Jan 6, Jan 27, Feb 17, etc.
              Run {
                  Options {
                      Type   = Full
                      Start  = 2007-01-06 01:00
                      Repeat = 3w
                  }
              }
              #Tuesday.  Would run Jan 9, Jan 30, Feb 20, etc.
              Run {
                  Options {
                      Type   = Incremental
                      Start  = 2007-01-09 01:00
                      Repeat = 3w
                  }
              }
          }

          Schedule {
              Name = "Week 3 Rotation"
              #Saturday.  Would run Jan 13, Feb 3, Feb 24, etc.
              Run {
                  Options {
                      Type   = Full
                      Start  = 2007-01-13 01:00
                      Repeat = 3w
                  }
              }
              #Tuesday.  Would run Jan 16, Feb 6, Feb 27, etc.
              Run {
                  Options {
                      Type   = Incremental
                      Start  = 2007-01-16 01:00
                      Repeat = 3w
                  }
              }
          }

Item 13:  Archival (removal) of User Files to Tape
  Date:   Nov. 24/2005 
  Origin: Ray Pengelly [ray at biomed dot queensu dot ca
  Status: 

  What:   The ability to archive data to storage based on certain parameters
          such as age, size, or location.  Once the data has been written to
          storage and logged it is then pruned from the originating
          filesystem. Note! We are talking about user's files and not
          Bacula Volumes.

  Why:    This would allow fully automatic storage management which becomes
          useful for large datastores.  It would also allow for auto-staging
          from one media type to another.

          Example 1) Medical imaging needs to store large amounts of data.
          They decide to keep data on their servers for 6 months and then put
          it away for long term storage.  The server then finds all files
          older than 6 months writes them to tape.  The files are then removed
          from the server.

          Example 2) All data that hasn't been accessed in 2 months could be
          moved from high-cost, fibre-channel disk storage to a low-cost
          large-capacity SATA disk storage pool which doesn't have as quick of
          access time.  Then after another 6 months (or possibly as one
          storage pool gets full) data is migrated to Tape.

Item 14:  Cause daemons to use a specific IP address to source communications
 Origin:  Bill Moran <wmoran@collaborativefusion.com>
 Date:    18 Dec 2006
 Status:
 What:    Cause Bacula daemons (dir, fd, sd) to always use the ip address
          specified in the [DIR|DF|SD]Addr directive as the source IP
          for initiating communication.
 Why:     On complex networks, as well as extremely secure networks, it's
          not unusual to have multiple possible routes through the network.
          Often, each of these routes is secured by different policies
          (effectively, firewalls allow or deny different traffic depending
          on the source address)
          Unfortunately, it can sometimes be difficult or impossible to
          represent this in a system routing table, as the result is
          excessive subnetting that quickly exhausts available IP space.
          The best available workaround is to provide multiple IPs to
          a single machine that are all on the same subnet.  In order
          for this to work properly, applications must support the ability
          to bind outgoing connections to a specified address, otherwise
          the operating system will always choose the first IP that
          matches the required route.
 Notes:   Many other programs support this.  For example, the following
          can be configured in BIND:
          query-source address 10.0.0.1;
          transfer-source 10.0.0.2;
          Which means queries from this server will always come from
          10.0.0.1 and zone transfers will always originate from
          10.0.0.2.

Item 15:  Multiple threads in file daemon for the same job
  Date:   27 November 2005
  Origin: Ove Risberg (Ove.Risberg at octocode dot com)
  Status:

  What:   I want the file daemon to start multiple threads for a backup
          job so the fastest possible backup can be made.

          The file daemon could parse the FileSet information and start
          one thread for each File entry located on a separate
          filesystem.

          A confiuration option in the job section should be used to
          enable or disable this feature. The confgutration option could
          specify the maximum number of threads in the file daemon.

          If the theads could spool the data to separate spool files
          the restore process will not be much slower.

  Why:    Multiple concurrent backups of a large fileserver with many
          disks and controllers will be much faster.

Item 16:  Add Plug-ins to the FileSet Include statements.
  Date:   28 October 2005
  Origin:
  Status: Partially coded in 1.37 -- much more to do.

  What:   Allow users to specify wild-card and/or regular
          expressions to be matched in both the Include and
          Exclude directives in a FileSet.  At the same time,
          allow users to define plug-ins to be called (based on
          regular expression/wild-card matching).

  Why:    This would give the users the ultimate ability to control
          how files are backed up/restored.  A user could write a
          plug-in knows how to backup his Oracle database without
          stopping/starting it, for example.

Item 17:  Restore only file attributes (permissions, ACL, owner, group...)
  Origin: Eric Bollengier
  Date:   30/12/2006
  Status:

  What:   The goal of this project is to be able to restore only rights
          and attributes of files without crushing them.

  Why:    Who have never had to repair a chmod -R 777, or a wild update
          of recursive right under Windows? At this time, you must have
          enough space to restore data, dump attributes (easy with acl,
          more complex with unix/windows rights) and apply them to your 
          broken tree. With this options, it will be very easy to compare
          right or ACL over the time.

  Notes:  If the file is here, we skip restore and we change rights.
          If the file isn't here, we can create an empty one and apply
          rights or do nothing.

Item 18:  Quick release of FD-SD connection after backup.
  Origin: Frank Volf (frank at deze dot org)
  Date:   17 November 2005
  Status: Done -- implemented by Kern -- in CVS 26Jan07

   What:  In the Bacula implementation a backup is finished after all data
          and attributes are successfully written to storage.  When using a
          tape backup it is very annoying that a backup can take a day,
          simply because the current tape (or whatever) is full and the
          administrator has not put a new one in.  During that time the
          system cannot be taken off-line, because there is still an open
          session between the storage daemon and the file daemon on the
          client.

          Although this is a very good strategy for making "safe backups"
          This can be annoying for e.g.  laptops, that must remain
          connected until the backup is completed.

          Using a new feature called "migration" it will be possible to
          spool first to harddisk (using a special 'spool' migration
          scheme) and then migrate the backup to tape.

          There is still the problem of getting the attributes committed.
          If it takes a very long time to do, with the current code, the
          job has not terminated, and the File daemon is not freed up.  The
          Storage daemon should release the File daemon as soon as all the
          file data and all the attributes have been sent to it (the SD).
          Currently the SD waits until everything is on tape and all the
          attributes are transmitted to the Director before signaling
          completion to the FD. I don't think I would have any problem
          changing this.  The reason is that even if the FD reports back to
          the Dir that all is OK, the job will not terminate until the SD
          has done the same thing -- so in a way keeping the SD-FD link
          open to the very end is not really very productive ...

   Why:   Makes backup of laptops much faster.

Item 19:  Implement a Python interface to the Bacula catalog.
  Date:   28 October 2005
  Origin: Kern
  Status: 

  What:   Implement an interface for Python scripts to access
          the catalog through Bacula.

  Why:    This will permit users to customize Bacula through
          Python scripts.

Item 20:  Archive data
  Date:   15/5/2006
  Origin: calvin streeting calvin at absentdream dot com
  Status:

  What:   The abilty to archive to media (dvd/cd) in a uncompressed format
          for dead filing (archiving not backing up)

    Why:  At my works when jobs are finished and moved off of the main file
          servers (raid based systems) onto a simple linux file server (ide based
          system) so users can find old information without contacting the IT
          dept.

          So this data dosn't realy change it only gets added to,
          But it also needs backing up.  At the moment it takes
          about 8 hours to back up our servers (working data) so
          rather than add more time to existing backups i am trying
          to implement a system where we backup the acrhive data to
          cd/dvd these disks would only need to be appended to
          (burn only new/changed files to new disks for off site
          storage).  basialy understand the differnce between
          achive data and live data.

  Notes:  Scan the data and email me when it needs burning divide
          into predifind chunks keep a recored of what is on what
          disk make me a label (simple php->mysql=>pdf stuff) i
          could do this bit ability to save data uncompresed so
          it can be read in any other system (future proof data)
          save the catalog with the disk as some kind of menu
          system 

Item 21:  Split documentation
  Origin: Maxx <maxxatworkat gmail dot com>
  Date:   27th July 2006
  Status:

  What:   Split documentation in several books

  Why:    Bacula manual has now more than 600 pages, and looking for
          implementation details is getting complicated.  I think
          it would be good to split the single volume in two or
          maybe three parts:

          1) Introduction, requirements and tutorial, typically
             are useful only until first installation time

          2) Basic installation and configuration, with all the
             gory details about the directives supported 3)
             Advanced Bacula: testing, troubleshooting, GUI and
             ancillary programs, security managements, scripting,
             etc.


Item 22:  Implement support for stacking arbitrary stream filters, sinks.
Date:     23 November 2006
Origin:   Landon Fuller <landonf@threerings.net>
Status:   Planning. Assigned to landonf.

  What:   Implement support for the following:
          - Stacking arbitrary stream filters (eg, encryption, compression,  
            sparse data handling))
          - Attaching file sinks to terminate stream filters (ie, write out  
            the resultant data to a file)
          - Refactor the restoration state machine accordingly

   Why:   The existing stream implementation suffers from the following:
           - All state (compression, encryption, stream restoration), is  
             global across the entire restore process, for all streams. There are  
             multiple entry and exit points in the restoration state machine, and  
             thus multiple places where state must be allocated, deallocated,  
             initialized, or reinitialized. This results in exceptional complexity  
             for the author of a stream filter.
           - The developer must enumerate all possible combinations of filters  
             and stream types (ie, win32 data with encryption, without encryption,  
             with encryption AND compression, etc).

  Notes:  This feature request only covers implementing the stream filters/ 
          sinks, and refactoring the file daemon's restoration implementation  
          accordingly. If I have extra time, I will also rewrite the backup  
          implementation. My intent in implementing the restoration first is to  
          solve pressing bugs in the restoration handling, and to ensure that  
          the new restore implementation handles existing backups correctly.

          I do not plan on changing the network or tape data structures to  
          support defining arbitrary stream filters, but supporting that  
          functionality is the ultimate goal.

          Assistance with either code or testing would be fantastic.

Item 23:  Implement from-client and to-client on restore command line.
   Date:  11 December 2006
  Origin: Discussion on Bacula-users entitled 'Scripted restores to
          different clients', December 2006 
  Status: New feature request
 
  What:   While using bconsole interactively, you can specify the client
          that a backup job is to be restored for, and then you can
          specify later a different client to send the restored files
          back to. However, using the 'restore' command with all options
          on the command line, this cannot be done, due to the ambiguous
          'client' parameter. Additionally, this parameter means different
          things depending on if it's specified on the command line or
          afterwards, in the Modify Job screens.
 
     Why: This feature would enable restore jobs to be more completely
          automated, for example by a web or GUI front-end.
 
   Notes: client can also be implied by specifying the jobid on the command
          line

Item 24:  Add an override in Schedule for Pools based on backup types.
Date:     19 Jan 2005
Origin:   Chad Slater <chad.slater@clickfox.com>
Status: 
                                                
  What:   Adding a FullStorage=BigTapeLibrary in the Schedule resource
          would help those of us who use different storage devices for different
          backup levels cope with the "auto-upgrade" of a backup.

  Why:    Assume I add several new device to be backed up, i.e. several
          hosts with 1TB RAID.  To avoid tape switching hassles, incrementals are
          stored in a disk set on a 2TB RAID.  If you add these devices in the
          middle of the month, the incrementals are upgraded to "full" backups,
          but they try to use the same storage device as requested in the
          incremental job, filling up the RAID holding the differentials.  If we
          could override the Storage parameter for full and/or differential
          backups, then the Full job would use the proper Storage device, which
          has more capacity (i.e. a 8TB tape library.

Item 25:  Implement huge exclude list support using hashing (dlists).
  Date:   28 October 2005
  Origin: Kern
  Status: Done in 2.1.2 but was done with dlists (doubly linked lists
          since hashing will not help. The huge list also supports
          large include lists).

  What:   Allow users to specify very large exclude list (currently
          more than about 1000 files is too many).

  Why:    This would give the users the ability to exclude all
          files that are loaded with the OS (e.g. using rpms
          or debs). If the user can restore the base OS from
          CDs, there is no need to backup all those files. A
          complete restore would be to restore the base OS, then
          do a Bacula restore. By excluding the base OS files, the
          backup set will be *much* smaller.

Item 26:  Implement more Python events in Bacula.
  Date:   28 October 2005
  Origin: Kern
  Status: 

  What:   Allow Python scripts to be called at more places 
          within Bacula and provide additional access to Bacula
          internal variables.

  Why:    This will permit users to customize Bacula through
          Python scripts.

  Notes:  Recycle event
          Scratch pool event
          NeedVolume event
          MediaFull event
           
          Also add a way to get a listing of currently running
          jobs (possibly also scheduled jobs).


Item 27:  Incorporation of XACML2/SAML2 parsing
   Date:   19 January 2006
   Origin: Adam Thornton <athornton@sinenomine.net>
   Status: Blue sky

   What:   XACML is "eXtensible Access Control Markup Language" and  
          "SAML is the "Security Assertion Markup Language"--an XML standard  
          for making statements about identity and authorization.  Having these  
          would give us a framework to approach ACLs in a generic manner, and  
          in a way flexible enough to support the four major sorts of ACLs I  
          see as a concern to Bacula at this point, as well as (probably) to  
          deal with new sorts of ACLs that may appear in the future.

   Why:    Bacula is beginning to need to back up systems with ACLs  
          that do not map cleanly onto traditional Unix permissions.  I see  
          four sets of ACLs--in general, mutually incompatible with one  
          another--that we're going to need to deal with.  These are: NTFS  
          ACLs, POSIX ACLs, NFSv4 ACLS, and AFS ACLS.  (Some may question the  
          relevance of AFS; AFS is one of Sine Nomine's core consulting  
          businesses, and having a reputable file-level backup and restore  
          technology for it (as Tivoli is probably going to drop AFS support  
          soon since IBM no longer supports AFS) would be of huge benefit to  
          our customers; we'd most likely create the AFS support at Sine Nomine  
          for inclusion into the Bacula (and perhaps some changes to the  
          OpenAFS volserver) core code.)

          Now, obviously, Bacula already handles NTFS just fine.  However, I  
          think there's a lot of value in implementing a generic ACL model, so  
          that it's easy to support whatever particular instances of ACLs come  
          down the pike: POSIX ACLS (think SELinux) and NFSv4 are the obvious  
          things arriving in the Linux world in a big way in the near future.   
          XACML, although overcomplicated for our needs, provides this  
          framework, and we should be able to leverage other people's  
          implementations to minimize the amount of work *we* have to do to get  
          a generic ACL framework.  Basically, the costs of implementation are  
          high, but they're largely both external to Bacula and already sunk.

Item 28:  Filesystem watch triggered backup.
  Date:   31 August 2006
  Origin: Jesper Krogh <jesper@krogh.cc>
  Status: Unimplemented, depends probably on "client initiated backups"

  What:   With inotify and similar filesystem triggeret notification
          systems is it possible to have the file-daemon to monitor
          filesystem changes and initiate backup.

  Why:    There are 2 situations where this is nice to have.
          1) It is possible to get a much finer-grained backup than
             the fixed schedules used now.. A file created and deleted
             a few hours later, can automatically be caught.

          2) The introduced load on the system will probably be
             distributed more even on the system.

  Notes:  This can be combined with configration that specifies
          something like: "at most every 15 minutes or when changes
          consumed XX MB".

Kern Notes: I would rather see this implemented by an external program
          that monitors the Filesystem changes, then uses the console
          to start the appropriate job.

Item 29:  Allow inclusion/exclusion of files in a fileset by creation/mod times
  Origin: Evan Kaufman <evan.kaufman@gmail.com>
  Date:   January 11, 2006
  Status:

  What:   In the vein of the Wild and Regex directives in a Fileset's
          Options, it would be helpful to allow a user to include or exclude
          files and directories by creation or modification times.

          You could factor the Exclude=yes|no option in much the same way it
          affects the Wild and Regex directives.  For example, you could exclude
          all files modified before a certain date:

   Options {
     Exclude = yes
     Modified Before = ####
   }

           Or you could exclude all files created/modified since a certain date:

   Options {
      Exclude = yes
     Created Modified Since = ####
   }

           The format of the time/date could be done several ways, say the number
           of seconds since the epoch:
           1137008553 = Jan 11 2006, 1:42:33PM   # result of `date +%s`

           Or a human readable date in a cryptic form:
           20060111134233 = Jan 11 2006, 1:42:33PM   # YYYYMMDDhhmmss

  Why:    I imagine a feature like this could have many uses. It would
          allow a user to do a full backup while excluding the base operating
          system files, so if I installed a Linux snapshot from a CD yesterday,
          I'll *exclude* all files modified *before* today.  If I need to
          recover the system, I use the CD I already have, plus the tape backup.
          Or if, say, a Windows client is hit by a particularly corrosive
          virus, and I need to *exclude* any files created/modified *since* the
          time of infection.

  Notes:  Of course, this feature would work in concert with other
          in/exclude rules, and wouldnt override them (or each other).

  Notes:  The directives I'd imagine would be along the lines of
          "[Created] [Modified] [Before|Since] = <date>".
          So one could compare against 'ctime' and/or 'mtime', but ONLY 'before'
           or 'since'.


Item 30:  Tray monitor window cleanups
  Origin: Alan Brown ajb2 at mssl dot ucl dot ac dot uk
  Date:   24 July 2006
  Status:
  What:   Resizeable and scrollable windows in the tray monitor.

  Why:    With multiple clients, or with many jobs running, the displayed
          window often ends up larger than the available screen, making
          the trailing items difficult to read.


Item 31:  Implement multiple numeric backup levels as supported by dump
Date:     3 April 2006
Origin:   Daniel Rich <drich@employees.org>
Status:
What:     Dump allows specification of backup levels numerically instead of just
          "full", "incr", and "diff".  In this system, at any given level, all
          files are backed up that were were modified since the last backup of a
          higher level (with 0 being the highest and 9 being the lowest).  A
          level 0 is therefore equivalent to a full, level 9 an incremental, and
          the levels 1 through 8 are varying levels of differentials.  For
          bacula's sake, these could be represented as "full", "incr", and
          "diff1", "diff2", etc.

Why:      Support of multiple backup levels would provide for more advanced backup
          rotation schemes such as "Towers of Hanoi".  This would allow better
          flexibility in performing backups, and can lead to shorter recover
          times.

Notes:    Legato Networker supports a similar system with full, incr, and 1-9 as
          levels.
        
Item 32:  Automatic promotion of backup levels
   Date:  19 January 2006
  Origin: Adam Thornton <athornton@sinenomine.net>
  Status: 

    What: Amanda has a feature whereby it estimates the space that a  
          differential, incremental, and full backup would take.  If the  
          difference in space required between the scheduled level and the next  
          level up is beneath some user-defined critical threshold, the backup  
          level is bumped to the next type.  Doing this minimizes the number of  
          volumes necessary during a restore, with a fairly minimal cost in  
          backup media space.

    Why:  I know at least one (quite sophisticated and smart) user  
          for whom the absence of this feature is a deal-breaker in terms of  
          using Bacula; if we had it it would eliminate the one cool thing  
          Amanda can do and we can't (at least, the one cool thing I know of).

Item 33:  Clustered file-daemons
  Origin: Alan Brown ajb2 at mssl dot ucl dot ac dot uk
  Date:   24 July 2006
  Status:
  What:   A "virtual" filedaemon, which is actually a cluster of real ones.

  Why:    In the case of clustered filesystems (SAN setups, GFS, or OCFS2, etc)
          multiple machines may have access to the same set of filesystems

          For performance reasons, one may wish to initate backups from
          several of these machines simultaneously, instead of just using
          one backup source for the common clustered filesystem.

          For obvious reasons, normally backups of $A-FD/$PATH and
          B-FD/$PATH are treated as different backup sets. In this case
          they are the same communal set.

          Likewise when restoring, it would be easier to just specify
          one of the cluster machines and let bacula decide which to use.

          This can be faked to some extent using DNS round robin entries
          and a virtual IP address, however it means "status client" will
          always give bogus answers. Additionally there is no way of
          spreading the load evenly among the servers.

          What is required is something similar to the storage daemon
          autochanger directives, so that Bacula can keep track of
          operating backups/restores and direct new jobs to a "free"
          client.

   Notes:

Item 34:  Commercial database support
  Origin: Russell Howe <russell_howe dot wreckage dot org>
  Date:   26 July 2006
  Status:

  What:   It would be nice for the database backend to support more 
          databases. I'm thinking of SQL Server at the moment, but I guess Oracle, 
          DB2, MaxDB, etc are all candidates. SQL Server would presumably be 
          implemented using FreeTDS or maybe an ODBC library?

  Why:    We only really have one database server, which is MS SQL Server 
          2000. Maintaining a second one for the backup software (we grew out of 
          SQLite, which I liked, but which didn't work so well with our database 
          size). We don't really have a machine with the resources to run 
          postgres, and would rather only maintain a single DBMS. We're stuck with 
          SQL Server because pretty much all the company's custom applications 
          (written by consultants) are locked into SQL Server 2000. I can imagine 
          this scenario is fairly common, and it would be nice to use the existing 
          properly specced database server for storing Bacula's catalog, rather 
          than having to run a second DBMS.

Item 35:  Automatic disabling of devices
   Date:  2005-11-11
  Origin: Peter Eriksson <peter at ifm.liu dot se>
  Status:

   What:  After a configurable amount of fatal errors with a tape drive
          Bacula should automatically disable further use of a certain
          tape drive. There should also be "disable"/"enable" commands in
          the "bconsole" tool.

   Why:   On a multi-drive jukebox there is a possibility of tape drives
          going bad during large backups (needing a cleaning tape run,
          tapes getting stuck). It would be advantageous if Bacula would
          automatically disable further use of a problematic tape drive
          after a configurable amount of errors has occurred.

          An example: I have a multi-drive jukebox (6 drives, 380+ slots)
          where tapes occasionally get stuck inside the drive. Bacula will
          notice that the "mtx-changer" command will fail and then fail
          any backup jobs trying to use that drive. However, it will still
          keep on trying to run new jobs using that drive and fail -
          forever, and thus failing lots and lots of jobs... Since we have
          many drives Bacula could have just automatically disabled
          further use of that drive and used one of the other ones
          instead.

Item 36:  An option to operate on all pools with update vol parameters
  Origin: Dmitriy Pinchukov <absh@bossdev.kiev.ua>
   Date:  16 August 2006
  Status:

   What:  When I do update -> Volume parameters -> All Volumes
          from Pool, then I have to select pools one by one.  I'd like
          console to have an option like "0: All Pools" in the list of
          defined pools.

   Why:   I have many pools and therefore unhappy with manually
          updating each of them using update -> Volume parameters -> All
          Volumes from Pool -> pool #.

Item 37:  Add an item to the restore option where you can select a pool
  Origin: kshatriyak at gmail dot com
    Date: 1/1/2006
  Status:

    What: In the restore option (Select the most recent backup for a
          client) it would be useful to add an option where you can limit
          the selection to a certain pool.

     Why: When using cloned jobs, most of the time you have 2 pools - a
          disk pool and a tape pool.  People who have 2 pools would like to
          select the most recent backup from disk, not from tape (tape
          would be only needed in emergency).  However, the most recent
          backup (which may just differ a second from the disk backup) may
          be on tape and would be selected.  The problem becomes bigger if
          you have a full and differential - the most "recent" full backup
          may be on disk, while the most recent differential may be on tape
          (though the differential on disk may differ even only a second or
          so).  Bacula will complain that the backups reside on different
          media then.  For now the only solution now when restoring things
          when you have 2 pools is to manually search for the right
          job-id's and enter them by hand, which is a bit fault tolerant.

Item 38:  Include timestamp of job launch in "stat clients" output
  Origin: Mark Bergman <mark.bergman@uphs.upenn.edu>
  Date:   Tue Aug 22 17:13:39 EDT 2006
  Status:

  What:   The "stat clients" command doesn't include any detail on when
          the active backup jobs were launched.

  Why:    Including the timestamp would make it much easier to decide whether
          a job is running properly. 

  Notes:  It may be helpful to have the output from "stat clients" formatted 
          more like that from "stat dir" (and other commands), in a column
          format. The per-client information that's currently shown (level,
          client name, JobId, Volume, pool, device, Files, etc.) is good, but
          somewhat hard to parse (both programmatically and visually), 
          particularly when there are many active clients.


Item 39:  Message mailing based on backup types
 Origin:  Evan Kaufman <evan.kaufman@gmail.com>
   Date:  January 6, 2006
 Status:

   What:  In the "Messages" resource definitions, allowing messages
          to be mailed based on the type (backup, restore, etc.) and level
          (full, differential, etc) of job that created the originating
          message(s).

 Why:     It would, for example, allow someone's boss to be emailed
          automatically only when a Full Backup job runs, so he can
          retrieve the tapes for offsite storage, even if the IT dept.
          doesn't (or can't) explicitly notify him.  At the same time, his
          mailbox wouldnt be filled by notifications of Verifies, Restores,
          or Incremental/Differential Backups (which would likely be kept
          onsite).

 Notes:   One way this could be done is through additional message types, for example:

   Messages {
     # email the boss only on full system backups
     Mail = boss@mycompany.com = full, !incremental, !differential, !restore, 
            !verify, !admin
     # email us only when something breaks
     MailOnError = itdept@mycompany.com = all
   }


Item 40:  Include JobID in spool file name ****DONE****
  Origin: Mark Bergman <mark.bergman@uphs.upenn.edu>
  Date:   Tue Aug 22 17:13:39 EDT 2006
  Status: Done. (patches/testing/project-include-jobid-in-spool-name.patch)
          No need to vote for this item.

  What:   Change the name of the spool file to include the JobID

  Why:    JobIDs are the common key used to refer to jobs, yet the 
          spoolfile name doesn't include that information. The date/time
          stamp is useful (and should be retained).

============= New Freature Requests after vote of 26 Jan 2007 ========
Item  41: Enable to relocate files and directories when restoring
  Date:   2007-03-01
  Origin: Eric Bollengier <eric@eb.homelinux.org>
  Status: Done.

  What:   The where= option is not powerful enough. It will be
          a great feature if bacula can restore a file in the
          same directory, but with a different name, or in 
          an other directory without recreating the full path.

  Why:    When i want to restore a production environment to a
          development environment, i just want change the first
          directory. ie restore /prod/data/file.dat to /rect/data/file.dat.
          At this time, i have to move by hand files. You must have a big
          dump space to restore and move data after.

          When i use Linux or SAN snapshot, i mount them to /mnt/snap_xxx
          so, when a restore a file, i have to move by hand
          from /mnt/snap_xxx/file to /xxx/file. I can't replace a file
          easily.

          When a user ask me to restore a file in its personal folder,
          (without replace the existing one), i can't restore from
          my_file.txt to my_file.txt.old witch is very practical.

          
  Notes:  I think we can enhance the where= option very easily by
          allowing regexp expression. 

          Since, many users think that regexp are not user friendly, i think
          that bat, bconsole or brestore must provide a simple way to
          configure where= option (i think to something like in
          openoffice "search and replace").

          Ie, if user uses where=/tmp/bacula-restore, we keep the old
          fashion.

          If user uses something like where=s!/prod!/test!, files will
          be restored from /prod/xxx to /test/xxx.

          If user uses something like where=s/$/.old/, files will
          be restored from /prod/xxx.txt to /prod/xxx.txt.old.

          If user uses something like where=s/txt$/old.txt/, files will
          be restored from /prod/xxx.txt to /prod/xxx.old.txt

          if user uses something like where=s/([a-z]+)$/old.$1/, files will
          be restored from /prod/xxx.ext to /prod/xxx.old.ext

Item n:   Implement Catalog directive for Pool resource in Director
configuration 
  Origin: Alan Davis adavis@ruckus.com
  Date:   6 March 2007
  Status: Submitted
 
  What:   The current behavior is for the director to create all pools
          found in the configuration file in all catalogs.  Add a
          Catalog directive to the Pool resource to specify which
          catalog to use for each pool definition.
 
  Why:    This allows different catalogs to have different pool
          attributes and eliminates the side-effect of adding
          pools to catalogs that don't need/use them.
 
  Notes:  


Item n:   Implement NDMP protocol support
  Origin: Alan Davis
  Date:   06 March 2007
  Status: Submitted

  What:   Network Data Management Protocol is implemented by a number of
          NAS filer vendors to enable backups using third-party
          software.

  Why:    This would allow NAS filer backups in Bacula without incurring
          the overhead of NFS or SBM/CIFS.

  Notes:  Further information is available:
          http://www.ndmp.org
          http://www.ndmp.org/wp/wp.shtml
          http://www.traakan.com/ndmjob/index.html

          There are currently no viable open-source NDMP
          implementations.  There is a reference SDK and example
          app available from ndmp.org but it has problems
          compiling on recent Linux and Solaris OS'.  The ndmjob
          reference implementation from Traakan is known to
          compile on Solaris 10.

  Notes (Kern): I am not at all in favor of this until NDMP becomes
          an Open Standard or until there are Open Source libraries
          that interface to it.

Item n: make changing "spooldata=yes|no" possible for
        manual/interactive jobs

Origin: Marc Schiffbauer <marc@schiffbauer.net>

Date:   12 April 2007)

Status: NEW

What:   Make it possible to modify the spooldata option
        for a job when being run from within the console.
        Currently it is possible to modify the backup level
        and the spooldata setting in a Schedule resource.
        It is also possible to modify the backup level when using
        the "run" command in the console. 
        But it is currently not possible to to the same 
        with "spooldata=yes|no" like:

        run job=MyJob level=incremental spooldata=yes

Why:    In some situations it would be handy to be able to switch
        spooldata on or off for interactive/manual jobs based on
        which data the admin expects or how fast the LAN/WAN
        connection currently is.

Notes:  ./.

============= Empty Feature Request form ===========
Item  n:  One line summary ...
  Date:   Date submitted 
  Origin: Name and email of originator.
  Status: 

  What:   More detailed explanation ...

  Why:    Why it is important ...

  Notes:  Additional notes or features (omit if not used)
============== End Feature Request form ==============

</pre>

        </td>
</tr>
</table>

<? require_once("inc/footer.php"); ?>
