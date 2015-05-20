File Services Daemon
====================

Please note, this section is somewhat out of date as the code has
evolved significantly. The basic idea has not changed though.

This chapter is intended to be a technical discussion of the File daemon
services and as such is not targeted at end users but rather at
developers and system administrators that want or need to know more of
the working details of <span>**Bareos**</span>.

The <span>**Bareos File Services**</span> consist of the programs that
run on the system to be backed up and provide the interface between the
Host File system and Bareos – in particular, the Director and the
Storage services.

When time comes for a backup, the Director gets in touch with the File
daemon on the client machine and hands it a set of “marching orders”
which, if written in English, might be something like the following:

OK, <span>**File daemon**</span>, it’s time for your daily incremental
backup. I want you to get in touch with the Storage daemon on host
archive.mysite.com and perform the following save operations with the
designated options. You’ll note that I’ve attached include and exclude
lists and patterns you should apply when backing up the file system. As
this is an incremental backup, you should save only files modified since
the time you started your last backup which, as you may recall, was
2000-11-19-06:43:38. Please let me know when you’re done and how it
went. Thank you.

So, having been handed everything it needs to decide what to dump and
where to store it, the File daemon doesn’t need to have any further
contact with the Director until the backup is complete providing there
are no errors. If there are errors, the error messages will be delivered
immediately to the Director. While the backup is proceeding, the File
daemon will send the file coordinates and data for each file being
backed up to the Storage daemon, which will in turn pass the file
coordinates to the Director to put in the catalog.

During a <span>**Verify**</span> of the catalog, the situation is
different, since the File daemon will have an exchange with the Director
for each file, and will not contact the Storage daemon.

A <span>**Restore**</span> operation will be very similar to the
<span>**Backup**</span> except that during the <span>**Restore**</span>
the Storage daemon will not send storage coordinates to the Director
since the Director presumably already has them. On the other hand, any
error messages from either the Storage daemon or File daemon will
normally be sent directly to the Directory (this, of course, depends on
how the Message resource is defined).

Commands Received from the Director for a Backup
------------------------------------------------

To be written ...

Commands Received from the Director for a Restore
-------------------------------------------------

To be written ...
