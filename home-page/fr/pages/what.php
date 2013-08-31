<? require_once("inc/header.php"); ?>
<table>
<tr>
        <td class="contentTopic">
                What is Bacula?
        </td>
</tr>
<tr>
        <td class="content">

<b>Bacula</b> is a set of computer programs that permit you (or the
system administrator) to manage backup, recovery, and verification of
computer data across a network of computers of different kinds. In
technical terms, it is a network Client/Server based backup program.
Bacula is relatively easy to use and efficient, while offering many
advanced storage management features that make it easy to find and
recover lost or damaged files. Due to its modular design, Bacula is
scalable from small single computer systems to systems consisting of
hundreds of computers located over a large network.
<h3>Who Needs Bacula?</h3>
If you are currently using a program such as <b>tar</b>, <b>dump</b>, or
<b>bru</b> to backup your computer data, and you would like a network
solution, more flexibility, or catalog services, Bacula will most
likely provide the additional features you want. However, if you are
new to Unix systems or do not have offsetting experience with a sophisticated
backup package, we do not recommend using Bacula as it is
much more difficult to setup and use than <b>tar</b> or <b>dump</b>.
<p>If you are running <b>Amanda</b> and would like a backup program that
can write to multiple volumes (i.e. is not limited by your tape drive
capacity), Bacula can most likely fill your needs. In addition,
quite a number of our users report that Bacula is simpler to
setup and use than other equivalent programs.
<p>If you are
currently using a sophisticated commercial package such as Legato
Networker. ARCserveIT, Arkeia, or PerfectBackup+, you may be interested
in Bacula, which provides many of the same features, and is free
software available under the GNU Version 2 software license.

<h3>Bacula Components or Services</h3>
Bacula is made up of the following five major components or services:
<p style="text-align: center; font-size: small">
        <img src="/images/manual/bacula-applications.png" alt="" width="576" height="734"><br>
        thanks to Aristedes Maniatis for this graphic and the one below
</p>
<p>
<ul>
<li><a name="DirDef"></a>
    <b>Bacula Director</b> service consists of the program that
     supervises all the backup, restore, verify and archive operations.
     The system administrator uses the Bacula Director to schedule
     backups and to recover files. For more details see the <a
     href="rel-manual/director.html">Director Services Daemon Design Document</a>.
     The Director runs as a daemon or a service (i.e. in the background).
</li>
<li><a name="UADef"></a>
    <b>Bacula Console</b> services is the program that allows the
    administrator or user to communicate with the <b>Bacula Director</b>
    (see above). Currently, the Bacula Console is available in three
    versions. The first and simplest is to run the Console program in a
    shell window (i.e. TTY interface). Most system administrators will
    find this completely adequate. The second version is a GNOME GUI
    interface that for the moment (23 November 2003) is far from complete,
    but quite functional as it has most the capabilities of the shell
    Console. The third version is a wxWidgets GUI with an interactive file
    restore. It also has most the capabilities of the shell console,
    allows command completion with tabulation, and gives you instant
    help about the command you are typing. For more details see the
    <a href="rel-manual/console.html">Bacula Console Design Document</a>.
</li>
<li><a name="FDDef"></a>
    <b>Bacula File</b> services (or Client program) is the software
    program that is installed on the machine to be backed up. It is
    specific to the operating system on which it runs and is responsible
    for providing the file attributes and data when requested by the
    Director. The File services are also responsible for the file
    system dependent part of restoring the file attributes and data
    during a recovery operation. For more details see the <a
    href="rel-manual/file.html">File Services Daemon Design Document</a>. This
    program runs as a daemon on the machine to be backed up, and in some
    of the documentation, the File daemon is referred to as the Client
    (for example in Bacula's configuration file). In addition to
    Unix/Linux File daemons, there is a Windows File daemon (normally
    distributed in binary format). The Windows File daemon runs on
    all currently known Windows versions (95, 98, Me, NT, 2000, XP).
</li>
<li><a name="SDDef"></a>
    <b>Bacula Storage</b> services consist of the software programs that
    perform the storage and recovery of the file attributes and data to
    the physical backup media or volumes. In other words, the Storage daemon
    is responsible for reading and writing your tapes (or other
    storage media, e.g. files). For more details see the <a
    href="rel-manual/storage.html">Storage Services Daemon Design Document</a>.
    The Storage services runs as a daemon on the machine that has the
    backup device (usually a tape drive).
</li>
<li><a name="DBDefinition"></a>
    <b>Catalog</b> services are comprised of the software programs
    responsible for maintaining the file indexes and volume databases for
    all files backed up. The Catalog services permit the System
    Administrator or user to quickly locate and restore any desired
    file. The Catalog services sets Bacula apart from simple backup
    programs like tar and bru, because the catalog maintains a record
    of all Volumes used, all Jobs run, and all Files saved, permitting
    efficicient restoration and Volume management.
   Bacula currently supports three different databases, MySQL,
    PostgreSQL, and SQLite, one of which must be chosen when building
    <b>Bacula</b>. There also exists an Internal database, but it is no
    longer supported.
    <p>
    The three SQL databases currently supported (MySQL, PostgreSQL or SQLite)
    provide quite a number of features,
    including rapid indexing, arbitrary queries, and security. Although
    we plan to support other major SQL databases, the current
    Bacula implementation interfaces only to MySQL, PostgreSQL and SQLite.
    For more details see the <a href="rel-manual/catalog.html">Catalog Services
    Design Document</a>.
    <p>The RPMs for MySQL and PostgreSQL ship as part of the Linux RedHat release,
    or building it from the source is quite easy, see the
    <a href="rel-manual/mysql.html"> Installing and Configuring MySQL</a> chapter
    of this document for the details. For more information on MySQL,
    please see: <a href="http://www.mysql.com">www.mysql.com</a>.
    Or see the <a href="rel-manual/postgresql.html"> Installing and Configuring
    PostgreSQL</a> chapter of this document for the details. For more
    information on PostgreSQL, please see: <a
    href="http://www.postgresql.org">www.postgresql.org</a>.
    <p>Configuring and building SQLite is even easier. For the details
    of configuring SQLite, please see the <a href="rel-manual/sqlite.html">
    Installing and Configuring SQLite</a> chapter of this document.
</li>
<li><a name="MonDef"></a>
    <b>Bacula Monitor</b> services is the program that allows the
    administrator or user to watch current status of <b>Bacula Directors</b>,
    <b>Bacula File Daemons</b> and <b>Bacula Storage Daemons</b>
    (see above). Currently, only a GTK+ version is available, which
    works with Gnome and KDE (or any window manager that supports the
    FreeDesktop.org system tray standard).
</li>
</ul>
To perform a successful save or restore, the following four daemons
must be configured and running: the Director daemon, the File daemon,
the Storage daemon, and MySQL, PostgreSQL or SQLite.

<h3>Bacula Configuration</h3>
In order for Bacula to understand your system, what clients you
want backed up, and how, you must create a number of configuration
files containing resources (or objects). The following presents an
overall picture of this:
<p style="text-align: center">
        <img src="/images/manual/bacula-objects.png" alt="" width="576" height="734">
</p>

<h3>Conventions Used in this Document</h3>
<b>Bacula</b> is in a state of evolution, and as a consequence,
this manual will not always agree with the code. If an
item in this manual is preceded by an asterisk (*), it indicates
that the particular feature is not implemented. If it is preceded
by a plus sign (+), it indicates that the feature may be partially
implemented.
<p>If you are reading this manual as supplied in a released version
of the software, the above paragraph holds true. If you are reading
the online version of the manual, <a href="/dev-manual">
http://www.bacula.org/rel-manual</a>, please bear in mind that this version
describes the current version in development (in the CVS) that may
contain features not in the released version. Just the same,
it generally lags behind the code a bit.
<h3>Quick Start</h3>
To get Bacula up and running quickly, we recommend that you first
scan the Terminology section below, then quickly review the next chapter
entitled <a href="rel-manual/state.html">The Current State of Bacula</a>, then the
<a href="rel-manual/quickstart.html">Quick Start Guide to Bacula</a>, which will
give you a quick overview of getting Bacula running. After
which, you should proceed to the
chapter on <a href="rel-manual/install.html"> Installing Bacula</a>, then <a
href="rel-manual/configure.html">How to Configure Bacula</a>,
and finally the chapter on <a href="rel-manual/running.html">
Running Bacula</a>.

<h3>Terminology</h3>
To facilitate communication about this project, we provide here
the definitions of the terminology that we use.
<dl>
        <dt>Administrator</dt>
        <dd>The person or persons responsible for administrating the Bacula system.</dd>

        <dt>Backup</dt>
        <dd>We use the term <b>Backup</b> to refer to a Bacula Job that saves files. </dd>

        <dt>Bootstrap File</dt>
        <dd>The bootstrap file is an ASCII file
    containing a compact form of commands that allow Bacula or
    the stand-alone file extraction utility (<b>bextract</b>) to
    restore the contents of one or more Volumes, for example, the
    current state of a system just backed up. With a bootstrap file,
    Bacula can restore your system without a Catalog. You can
    create a bootstrap file from a Catalog to extract any file or
    files you wish.</dd>

        <dt>Catalog</dt>
        <dd>The Catalog is used to store summary information
    about the Jobs, Clients, and Files that were backed up and on
    what Volume or Volumes. The information saved in the Catalog
    permits the administrator or user to determine what jobs were
    run, their status as well as the important characteristics
    of each file that was backed up. The Catalog is an online resource,
    but does not contain the data for the files backed up. Most of
    the information stored in the catalog is also stored on the
    backup volumes (i.e. tapes). Of course, the tapes will also have
    a copy of the file in addition to the File Attributes (see below).
    <p>The catalog feature is one part of Bacula that distinguishes
    it from simple backup and archive programs such as <b>dump</b>
    and <b>tar</b>.
    </dd>
        
        <dt>Client</dt>
        <dd>In Bacula's terminology, the word Client
    refers to the machine being backed up, and it is synonymous
    with the File services or File daemon, and quite often, we
    refer to it as the FD. A Client is defined in a configuration
    file resource. </dd>
        
        <dt>Console</dt>
        <dd>The program that interfaces to the Director allowing
    the user or system administrator to control Bacula.</dd>
        
        <dt>Daemon</dt>
        <dd>Unix terminology for a program that is always present in
    the background to carry out a designated task. On Windows systems, as
    well as some Linux systems, daemons are called <b>Services</b>.</dd>
        
        <dt>Directive</dt>
        <dd>The term directive is used to refer to a statement
    or a record within a Resource in a configuration file that
    defines one specific thing. For example, the <b>Name</b> directive
    defines the name of the Resource.</dd>
        
        <dt>Director</dt>
        <dd>The main Bacula server daemon that schedules and directs all
    Bacula operations. Occassionally, we refer to the Director as DIR.</dd>
        
        <dt>Differential</dt>
        <dd>A backup that includes all files changed since the last
    Full save started. Note, other backup programs may define this differently.</dd>

        <dt>File Attributes</dt>
        <dd>The File Attributes are all the information
    necessary about a file to identify it and all its properties such as
    size, creation date, modification date, permissions, etc. Normally, the
    attributes are handled entirely by Bacula so that the user never
    needs to be concerned about them. The attributes do not include the
    file's data.

        <dt>File Daemon</dt>
        <dd>The daemon running on the client
    computer to be backed up. This is also referred to as the File
    services, and sometimes as the Client services or the FD.
        
        <dt><a name="FileSetDef"></a> FileSet</dt>
        <dd>A FileSet is a Resource contained in a configuration
    file that defines the files to be backed up. It consists
    of a list of included files or directories, a list of excluded files, and
    how the file is to be stored (compression, encryption, signatures).
    For more details, see the
        <a href="rel-manual/director.html#FileSetResource">FileSet Resource definition</a>
    in the Director chapter of this document.</dd>

        <dt>Incremental</dt>
        <dd>A backup that includes all files changed since the
    last Full, Differential, or Incremental backup started. It is normally
    specified on the <b>Level</b> directive within the Job resource
    definition, or in a Schedule resourc. </dd>
        
        <dt><a name="JobDef"></a>Job</dt>
        <dd>A Bacula Job is a configuration resource that defines
    the work that Bacula must perform to backup or restore a particular
    Client. It consists of the <b>Type</b> (backup, restore, verify,
    etc), the <b>Level</b> (full, incremental,...), the <b>FileSet</b>,
    and <b>Storage</b> the files are to be backed up (Storage device,
    Media Pool). For more details, see the 
        <a href="rel-manual/director.html#JobResource">Job Resource definition</a>
        in the Director chapter of this document. </dd>

        <dt>Monitor</dt>
        <dd>The program that interfaces to the all the daemons
    allowing the user or system administrator to monitor Bacula status.</dd>
        
        <dt>Resource</dt>
        <dd>A resource is a part of a configuration file that
    defines a specific unit of information that is available to Bacula.
    For example, the <b>Job</b> resource defines all the properties of
    a specific Job: name, schedule, Volume pool, backup type, backup
    level, ...</dd>
        
        <dt>Restore</dt>
        <dd>A restore is a configuration resource that
    describes the operation of recovering a file (lost or damaged) from
    backup media. It is the inverse of a save, except that in most
    cases, a restore will normally have a small set of files to restore,
    while normally a Save backs up all the files on the system. Of
    course, after a disk crash, Bacula can be called upon to do
    a full Restore of all files that were on the system. </dd>
        
        <dt>Schedule</dt>
        <dd>A Schedule is a configuration resource that
     defines when the Bacula Job will be scheduled for
     execution. To use the Schedule, the Job resource will refer to
     the name of the Schedule. For more details, see the <a
     href="rel-manual/director.html#ScheduleResource">Schedule Resource
     definition</a> in the Director chapter of this document. </dd>
         
        <dt>Service</dt>
        <dd>This is Windows terminology for a <b>daemon</b> -- see
    above. It is now frequently used in Unix environments as well.</dd>
        
        <dt>Storage Coordinates</dt>
        <dd>The information returned from the
    Storage Services that uniquely locates a file on a backup medium. It
    consists of two parts: one part pertains to each file saved, and the
    other part pertains to the whole Job. Normally, this information is
    saved in the Catalog so that the user doesn't need specific knowledge
    of the Storage Coordinates. The Storage Coordinates include the
    File Attributes (see above) plus the unique location of the information on
    the backup Volume. </dd>
        
        <dt>Storage Daemon</dt>
        <dd>The Storage daemon, sometimes referred to as
    the SD, is the code that writes the attributes and data to a storage
    Volume (usually a tape or disk).</dd>
        
        <dt>Session</dt>
        <dd>Normally refers to the internal conversation between
    the File daemon and the Storage daemon. The File daemon opens a
    <b>session</b> with the Storage daemon to save a FileSet, or to restore
    it. A session has a one to one correspondence to a Bacula Job (see
    above). </dd>
        
        <dt>Verify</dt>
        <dd>A verify is a job that compares the current file
    attributes to the attributes that have previously been stored in the
    Bacula Catalog. This feature can be used for detecting changes to
    critical system files similar to what <b>Tripwire</b> does. One
    of the major advantages of using Bacula to do this is that
    on the machine you want protected such as a server, you can run
    just the File daemon, and the Director, Storage daemon, and Catalog
    reside on a different machine. As a consequence, if your server is
    ever compromised, it is unlikely that your verification database
    will be tampered with.
    <p>Verify can also be used to check that the most recent Job
    data written to a Volume agrees with what is stored in the Catalog
    (i.e. it compares the file attributes), *or it can check the
    Volume contents against the original files on disk. </dd>
        
        <dt>*Archive</dt>
        <dd>An Archive operation is done after a Save, and it
    consists of removing the Volumes on which data is saved from active
    use. These Volumes are marked as Archived, and many no longer be
    used to save files. All the files contained on an Archived Volume
    are removed from the Catalog. NOT YET IMPLEMENTED. </dd>
        
        <dt>*Update</dt>
        <dd>An Update operation causes the files on the remote
    system to be updated to be the same as the host system. This is
    equivalent to an <b>rdist</b> capability. NOT YET IMPLEMENTED.
    </dd>

        <dt>Retention Period</dt>
        <dd>There are various kinds of retention
    periods that Bacula recognizes. The most important are the
    <b>File</b> Retention Period, <b>Job</b> Retention Period, and the
    <b>Volume</b> Retention Period. Each of these retention periods
    applies to the time that specific records will be kept in the
    Catalog database. This should not be confused with the time that
    the data saved to a Volume is valid. <p>The File Retention Period
    determines the time that File records are kept in the catalog
    database. This period is important because the volume of the
    database File records by far use the most storage space in the
    database. As a consequence, you must ensure that regular
    &quot;pruning&quot; of the database file records is done. (See
    the Console <b>retention</b> command for more details on this
    subject). <p>The Job Retention Period is the length of time that
    Job records will be kept in the database. Note, all the File
    records are tied to the Job that saved those files. The File
    records can be purged leaving the Job records. In this case,
    information will be available about the jobs that ran, but not the
    details of the files that were backed up. Normally, when a Job
    record is purged, all its File records will also be purged. <p>The
    Volume Retention Period is the minimum of time that a Volume will be
    kept before it is reused. Bacula will normally never
    overwrite a Volume that contains the only backup copy of a file.
    Under ideal conditions, the Catalog would retain entries for all
    files backed up for all current Volumes. Once a Volume is
    overwritten, the files that were backed up on that Volume are
    automatically removed from the Catalog. However, if there is a very
    large pool of Volumes or a Volume is never overwritten, the Catalog
    database may become enormous. To keep the Catalog to a manageable
    size, the backup information should removed from the Catalog after
    the defined File Retention Period. Bacula provides the
    mechanisms for the catalog to be automatically pruned according to
    the retention periods defined. </dd>

        <dt>Scan</dt>
        <dd>A Scan operation causes the contents of a Volume or a
    series of Volumes to be scanned. These Volumes with the information
    on which files they contain are restored to the Bacula Catalog.
    Once the information is restored to the Catalog, the files contained
    on those Volumes may be easily restored. This function is
    particularly useful if certain Volumes or Jobs have exceeded
    their retention period and have been pruned or purged from the
    Catalog. Scanning data from Volumes into the Catalog is done
    by using the <b>bscan</b> program. See the <a href="rel-manual/progs.html#bscan">
    bscan section</a> of the Bacula Utilities Chapter of this manual
    for more details.</dd>

        <dt>Volume</dt>
        <dd>A Volume is an archive unit, normally a tape or
    a named disk file where Bacula stores the data from one or more
    backup jobs. All Bacula Volumes have a software label written to
    the Volume by Bacula so that it identify what Volume it is really
    reading. (Normally there should be no confusion with disk files,
    but with tapes, it is easy to mount the wrong one).</dd>
</dl>

<h3>What Bacula is Not</h3>
<b>Bacula</b> is a backup, restore and verification program and is not a
complete disaster recovery system in itself, but it can be a key part
of one if you plan carefully and follow the instructions included in the <a
href="rel-manual/rescue.html"> Disaster Recovery</a> Chapter of this manual.
<p>
With proper planning, as mentioned in the Disaster Recovery chapter
<b>Bacula</b> can be a central component of your disaster recovery
system. For example, if you have created an emergency boot disk, a
Bacula Rescue disk to save the current partitioning information of your
hard disk, and maintain a complete Bacula backup, it is possible to
completely recover your system from &quot;bare metal&quot;.
<p>
If you have used the <b>WriteBootstrap</b> record in your job or some
other means to save a valid bootstrap file, you will be able to use it
to extract the necessary files (without using the catalog or manually
searching for the files to restore).

<h3>Interactions Between the Bacula Services</h3>
The following block diagram shows the typical interactions
between the Bacula Services for a backup job. Each block
represents in general a separate process (normally a daemon).
In general, the Director oversees the flow of information. It also
maintains the Catalog.
<p style="text-align: center">
        <img src="/images/manual/flow.jpeg" border="0" alt="Interactions between Bacula Services" width="480" height="370">
</p>
        </td>
</tr>
</table>

<? require_once("inc/header.php"); ?>
