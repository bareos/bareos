.. _RestoreChapter:

The Restore Command
===================

:index:`\ <single: Restore>`\

General
-------

Below, we will discuss restoring files with the Console restore command, which is the recommended way of doing restoring files. It is not possible to restore files by automatically starting a job as you do with Backup, Verify, ... jobs. However, in addition to the console restore command, there is a standalone program named bextract, which also permits restoring files. For more information on this program, please see the :ref:`Bareos Utility Programs <bextract>` chapter of this
manual. We don’t particularly recommend the bextract program because it lacks many of the features of the normal Bareos restore, such as the ability to restore Win32 files to Unix systems, and the ability to restore access control lists (ACL). As a consequence, we recommend, wherever possible to use Bareos itself for restores as described below.

You may also want to look at the bls program in the same chapter, which allows you to list the contents of your Volumes. Finally, if you have an old Volume that is no longer in the catalog, you can restore the catalog entries using the program named bscan, documented in the same :ref:`Bareos Utility Programs <bscan>` chapter.

In general, to restore a file or a set of files, you must run a restore job. That is a job with Type = Restore. As a consequence, you will need a predefined restore job in your bareos-dir.conf (Director’s config) file. The exact parameters (Client, FileSet, ...) that you define are not important as you can either modify them manually before running the job or if you use the restore command, explained below, Bareos will automatically set them for you. In fact, you can no longer simply run a
restore job. You must use the restore command.

Since Bareos is a network backup program, you must be aware that when you restore files, it is up to you to ensure that you or Bareos have selected the correct Client and the correct hard disk location for restoring those files. Bareos will quite willingly backup client A, and restore it by sending the files to a different directory on client B. Normally, you will want to avoid this, but assuming the operating systems are not too different in their file structures, this should work perfectly
well, if so desired. By default, Bareos will restore data to the same Client that was backed up, and those data will be restored not to the original places but to /tmp/bareos-restores. This is configured in the default restore command resource in bareos-dir.conf. You may modify any of these defaults when the restore command prompts you to run the job by selecting the mod option.

.. _RestoreCommand:

Restore Command
---------------

:index:`\ <single: Console; Command; restore>`\

Since Bareos maintains a catalog of your files and on which Volumes (disk or tape), they are stored, it can do most of the bookkeeping work, allowing you simply to specify what kind of restore you want (current, before a particular date), and what files to restore. Bareos will then do the rest.

This is accomplished using the restore command in the Console. First you select the kind of restore you want, then the JobIds are selected, the File records for those Jobs are placed in an internal Bareos directory tree, and the restore enters a file selection mode that allows you to interactively walk up and down the file tree selecting individual files to be restored. This mode is somewhat similar to the standard Unix restore program’s interactive file selection mode.

If a Job’s file records have been pruned from the catalog, the restore command will be unable to find any files to restore. Bareos will ask if you want to restore all of them or if you want to use a regular expression to restore only a selection while reading media. See :ref:`FileRegex option <FileRegex>` and below for more details on this.

Within the Console program, after entering the restore command, you are presented with the following selection prompt:

.. code-block:: bconsole
   :caption: restore

   * <input>restore</input>
   First you select one or more JobIds that contain files
   to be restored. You will be presented several methods
   of specifying the JobIds. Then you will be allowed to
   select which files from those JobIds are to be restored.

   To select the JobIds, you have the following choices:
        1: List last 20 Jobs run
        2: List Jobs where a given File is saved
        3: Enter list of comma separated JobIds to select
        4: Enter SQL list command
        5: Select the most recent backup for a client
        6: Select backup for a client before a specified time
        7: Enter a list of files to restore
        8: Enter a list of files to restore before a specified time
        9: Find the JobIds of the most recent backup for a client
       10: Find the JobIds for a backup for a client before a specified time
       11: Enter a list of directories to restore for found JobIds
       12: Select full restore to a specified Job date
       13: Cancel
   Select item:  (1-13):

There are a lot of options, and as a point of reference, most people will want to select item 5 (the most recent backup for a client). The details of the above options are:

-  Item 1 will list the last 20 jobs run. If you find the Job you want, you can then select item 3 and enter its JobId(s).

-  Item 2 will list all the Jobs where a specified file is saved. If you find the Job you want, you can then select item 3 and enter the JobId.

-  Item 3 allows you the enter a list of comma separated JobIds whose files will be put into the directory tree. You may then select which files from those JobIds to restore. Normally, you would use this option if you have a particular version of a file that you want to restore and you know its JobId. The most common options (5 and 6) will not select a job that did not terminate normally, so if you know a file is backed up by a Job that failed (possibly because of a system crash), you can access
   it through this option by specifying the JobId.

-  Item 4 allows you to enter any arbitrary SQL command. This is probably the most primitive way of finding the desired JobIds, but at the same time, the most flexible. Once you have found the JobId(s), you can select item 3 and enter them.

-  Item 5 will automatically select the most recent Full backup and all subsequent incremental and differential backups for a specified Client. These are the Jobs and Files which, if reloaded, will restore your system to the most current saved state. It automatically enters the JobIds found into the directory tree in an optimal way such that only the most recent copy of any particular file found in the set of Jobs will be restored. This is probably the most convenient of all the above options to
   use if you wish to restore a selected Client to its most recent state.

   There are two important things to note. First, this automatic selection will never select a job that failed (terminated with an error status). If you have such a job and want to recover one or more files from it, you will need to explicitly enter the JobId in item 3, then choose the files to restore.

   If some of the Jobs that are needed to do the restore have had their File records pruned, the restore will be incomplete. Bareos currently does not correctly detect this condition. You can however, check for this by looking carefully at the list of Jobs that Bareos selects and prints. If you find Jobs with the JobFiles column set to zero, when files should have been backed up, then you should expect problems.

   If all the File records have been pruned, Bareos will realize that there are no file records in any of the JobIds chosen and will inform you. It will then propose doing a full restore (non-selective) of those JobIds. This is possible because Bareos still knows where the beginning of the Job data is on the Volumes, even if it does not know where particular files are located or what their names are.

-  Item 6 allows you to specify a date and time, after which Bareos will automatically select the most recent Full backup and all subsequent incremental and differential backups that started before the specified date and time.

-  Item 7 allows you to specify one or more filenames (complete path required) to be restored. Each filename is entered one at a time or if you prefix a filename with the less-than symbol (<) Bareos will read that file and assume it is a list of filenames to be restored. If you prefix the filename with a question mark (?), then the filename will be interpreted as an SQL table name, and Bareos will include the rows of that table in the list to be restored. The table must contain the JobId in the
   first column and the FileIndex in the second column. This table feature is intended for external programs that want to build their own list of files to be restored. The filename entry mode is terminated by entering a blank line.

-  Item 8 allows you to specify a date and time before entering the filenames. See Item 7 above for more details.

-  Item 9 allows you find the JobIds of the most recent backup for a client. This is much like option 5 (it uses the same code), but those JobIds are retained internally as if you had entered them manually. You may then select item 11 (see below) to restore one or more directories.

-  Item 10 is the same as item 9, except that it allows you to enter a before date (as with item 6). These JobIds will then be retained internally.

   :index:`\ <single: Restore Directories>`\

-  Item 11 allows you to enter a list of JobIds from which you can select directories to be restored. The list of JobIds can have been previously created by using either item 9 or 10 on the menu. You may then enter a full path to a directory name or a filename preceded by a less than sign (<). The filename should contain a list of directories to be restored. All files in those directories will be restored, but if the directory contains subdirectories, nothing will be restored in the subdirectory
   unless you explicitly enter its name.

-  Item 12 is a full restore to a specified job date.

-  Item 13 allows you to cancel the restore command.

For items 6, 7 and 9, note that you must specify the date/time either exactly as shown YYYY-MM-DD HH-MM-SS or you can enter a shorter version of the same format such as YYYY-MM, and the command will take care of formatting. For example, if you want the system to restore saves done before October 2020, you can write 2020-10, and the console will understand it as 2020-10-01 00:00:00 and restore all saves before it. Empty fields in the format will get assigned the lowest possible value.

As an example, suppose that we select item 5 (restore to most recent state). If you have not specified a client=xxx on the command line, it it will then ask for the desired Client, which on my system, will print all the Clients found in the database as follows:

.. code-block:: bconsole
   :caption: restore: select client

   Select item:  (1-13): <input>5</input>
   Defined clients:
        1: Rufus
        2: Matou
        3: Polymatou
        4: Minimatou
        5: Minou
        6: MatouVerify
        7: PmatouVerify
        8: RufusVerify
        9: Watchdog
   Select Client (File daemon) resource (1-9): <input>1</input>

The listed clients are only examples, yours will look differently. If you have only one Client, it will be automatically selected. In this example, I enter 1 for Rufus to select the Client. Then Bareos needs to know what FileSet is to be restored, so it prompts with:



::

   The defined FileSet resources are:
        1: Full Set
        2: Other Files
   Select FileSet resource (1-2):



If you have only one FileSet defined for the Client, it will be selected automatically. I choose item 1, which is my full backup. Normally, you will only have a single FileSet for each Job, and if your machines are similar (all Linux) you may only have one FileSet for all your Clients.

At this point, Bareos has all the information it needs to find the most recent set of backups. It will then query the database, which may take a bit of time, and it will come up with something like the following. Note, some of the columns are truncated here for presentation:



::

   +-------+------+----------+-------------+-------------+------+-------+------------+
   | JobId | Levl | JobFiles | StartTime   | VolumeName  | File | SesId |VolSesTime  |
   +-------+------+----------+-------------+-------------+------+-------+------------+
   | 1,792 | F    |  128,374 | 08-03 01:58 | DLT-19Jul02 |   67 |    18 | 1028042998 |
   | 1,792 | F    |  128,374 | 08-03 01:58 | DLT-04Aug02 |    0 |    18 | 1028042998 |
   | 1,797 | I    |      254 | 08-04 13:53 | DLT-04Aug02 |    5 |    23 | 1028042998 |
   | 1,798 | I    |       15 | 08-05 01:05 | DLT-04Aug02 |    6 |    24 | 1028042998 |
   +-------+------+----------+-------------+-------------+------+-------+------------+
   You have selected the following JobId: 1792,1792,1797
   Building directory tree for JobId 1792 ...
   Building directory tree for JobId 1797 ...
   Building directory tree for JobId 1798 ...
   cwd is: /
   $



Depending on the number of JobFiles for each JobId, the "Building directory tree ..." can take a bit of time. If you notice ath all the JobFiles are zero, your Files have probably been pruned and you will not be able to select any individual files – it will be restore everything or nothing.

In our example, Bareos found four Jobs that comprise the most recent backup of the specified Client and FileSet. Two of the Jobs have the same JobId because that Job wrote on two different Volumes. The third Job was an incremental backup to the previous Full backup, and it only saved 254 Files compared to 128,374 for the Full backup. The fourth Job was also an incremental backup that saved 15 files.

Next Bareos entered those Jobs into the directory tree, with no files marked to be restored as a default, tells you how many files are in the tree, and tells you that the current working directory (cwd) is /. Finally, Bareos prompts with the dollar sign ($) to indicate that you may enter commands to move around the directory tree and to select files.

If you want all the files to automatically be marked when the directory tree is built, you could have entered the command restore all, or at the $ prompt, you can simply enter mark \*.

Instead of choosing item 5 on the first menu (Select the most recent backup for a client), if we had chosen item 3 (Enter list of JobIds to select) and we had entered the JobIds 1792,1797,1798 we would have arrived at the same point.

One point to note, if you are manually entering JobIds, is that you must enter them in the order they were run (generally in increasing JobId order). If you enter them out of order and the same file was saved in two or more of the Jobs, you may end up with an old version of that file (i.e. not the most recent).

Directly entering the JobIds can also permit you to recover data from a Job that wrote files to tape but that terminated with an error status.

While in file selection mode, you can enter help or a question mark (?) to produce a summary of the available commands:



::

    Command    Description
     =======    ===========
     cd         change current directory
     count      count marked files in and below the cd
     dir        long list current directory, wildcards allowed
     done       leave file selection mode
     estimate   estimate restore size
     exit       same as done command
     find       find files, wildcards allowed
     help       print help
     ls         list current directory, wildcards allowed
     lsmark     list the marked files in and below the cd
     mark       mark dir/file to be restored recursively in dirs
     markdir    mark directory name to be restored (no files)
     pwd        print current working directory
     unmark     unmark dir/file to be restored recursively in dir
     unmarkdir  unmark directory name only no recursion
     quit       quit and do not do restore
     ?          print help



As a default no files have been selected for restore (unless you added all to the command line. If you want to restore everything, at this point, you should enter mark \*, and then done and Bareos will write the bootstrap records to a file and request your approval to start a restore job.

If you do not enter the above mentioned mark \* command, you will start with an empty state. Now you can simply start looking at the tree and mark particular files or directories you want restored. It is easy to make a mistake in specifying a file to mark or unmark, and Bareos’s error handling is not perfect, so please check your work by using the ls or dir commands to see what files are actually selected. Any selected file has its name preceded by an asterisk.

To check what is marked or not marked, enter the count command, which displays:



::

   128401 total files. 128401 marked to be restored.



Each of the above commands will be described in more detail in the next section. We continue with the above example, having accepted to restore all files as Bareos set by default. On entering the done command, Bareos prints:



::

   Run Restore job
   JobName:         RestoreFiles
   Bootstrap:       /var/lib/bareos/client1.restore.3.bsr
   Where:           /tmp/bareos-restores
   Replace:         Always
   FileSet:         Full Set
   Backup Client:   client1
   Restore Client:  client1
   Format:          Native
   Storage:         File
   When:            2013-06-28 13:30:08
   Catalog:         MyCatalog
   Priority:        10
   Plugin Options:  *None*
   OK to run? (yes/mod/no):



Please examine each of the items very carefully to make sure that they are correct. In particular, look at Where, which tells you where in the directory structure the files will be restored, and Client, which tells you which client will receive the files. Note that by default the Client which will receive the files is the Client that was backed up. These items will not always be completed with the correct values depending on which of the restore options you chose. You can change any of these
default items by entering mod and responding to the prompts.

The above assumes that you have defined a Restore Job resource in your Director’s configuration file. Normally, you will only need one Restore Job resource definition because by its nature, restoring is a manual operation, and using the Console interface, you will be able to modify the Restore Job to do what you want.

An example Restore Job resource definition is given below.

Returning to the above example, you should verify that the Client name is correct before running the Job. However, you may want to modify some of the parameters of the restore job. For example, in addition to checking the Client it is wise to check that the Storage device chosen by Bareos is indeed correct. Although the FileSet is shown, it will be ignored in restore. The restore will choose the files to be restored either by reading the Bootstrap file, or if not specified, it will restore all
files associated with the specified backup JobId (i.e. the JobId of the Job that originally backed up the files).

Finally before running the job, please note that the default location for restoring files is not their original locations, but rather the directory /tmp/bareos-restores. You can change this default by modifying your bareos-dir.conf file, or you can modify it using the mod option. If you want to restore the files to their original location, you must have Where set to nothing or to the root, i.e. /.

If you now enter yes, Bareos will run the restore Job.

Selecting Files by Filename
---------------------------

:index:`\ <single: Restore; by filename>`\

If you have a small number of files to restore, and you know the filenames, you can either put the list of filenames in a file to be read by Bareos, or you can enter the names one at a time. The filenames must include the full path and filename. No wild cards are used.

To enter the files, after the restore, you select item number 7 from the prompt list:

.. code-block:: bconsole
   :caption: restore list of files

   * <input>restore</input>
   First you select one or more JobIds that contain files
   to be restored. You will be presented several methods
   of specifying the JobIds. Then you will be allowed to
   select which files from those JobIds are to be restored.

   To select the JobIds, you have the following choices:
        1: List last 20 Jobs run
        2: List Jobs where a given File is saved
        3: Enter list of comma separated JobIds to select
        4: Enter SQL list command
        5: Select the most recent backup for a client
        6: Select backup for a client before a specified time
        7: Enter a list of files to restore
        8: Enter a list of files to restore before a specified time
        9: Find the JobIds of the most recent backup for a client
       10: Find the JobIds for a backup for a client before a specified time
       11: Enter a list of directories to restore for found JobIds
       12: Select full restore to a specified Job date
       13: Cancel
   Select item:  (1-13): <input>7</input>

which then prompts you for the client name:



::

   Defined Clients:
        1: client1
        2: Tibs
        3: Rufus
   Select the Client (1-3): 3



Of course, your client list will be different, and if you have only one client, it will be automatically selected. And finally, Bareos requests you to enter a filename:



::

   Enter filename:



At this point, you can enter the full path and filename



::

   Enter filename: /etc/resolv.conf
   Enter filename:



as you can see, it took the filename. If Bareos cannot find a copy of the file, it prints the following:



::

   Enter filename: junk filename
   No database record found for: junk filename
   Enter filename:



If you want Bareos to read the filenames from a file, you simply precede the filename with a less-than symbol (<).

It is possible to automate the selection by file by putting your list of files in say /tmp/file-list, then using the following command:



::

   restore client=client1 file=</tmp/file-list



If in modifying the parameters for the Run Restore job, you find that Bareos asks you to enter a Job number, this is because you have not yet specified either a Job number or a Bootstrap file. Simply entering zero will allow you to continue and to select another option to be modified.



.. _Replace:



Replace Options
---------------

When restoring, you have the option to specify a Replace option. This directive determines the action to be taken when restoring a file or directory that already exists. This directive can be set by selecting the mod option. You will be given a list of parameters to choose from. Full details on this option can be found in the Job Resource section of the Director documentation.

.. _CommandArguments:

Command Line Arguments
----------------------

If all the above sounds complicated, you will probably agree that it really isn’t after trying it a few times. It is possible to do everything that was shown above, with the exception of selecting the FileSet, by using command line arguments with a single command by entering:



::

   restore client=Rufus select current all done yes



The client=Rufus specification will automatically select Rufus as the client, the current tells Bareos that you want to restore the system to the most current state possible, and the yes suppresses the final yes/mod/no prompt and simply runs the restore.

The full list of possible command line arguments are:

-  all – select all Files to be restored.

-  select – use the tree selection method.

-  done – do not prompt the user in tree mode.

-  copies – instead of using the actual backup jobs for restoring use the copies which were made of these backup Jobs. This could mean that on restore the client will contact a remote storage daemon if the data is copied to a remote storage daemon as part of your copy Job scheme.

-  current – automatically select the most current set of backups for the specified client.

-  client=xxxx – initially specifies the client from which the backup was made and the client to which the restore will be make. See also "restoreclient" keyword.

-  restoreclient=xxxx – if the keyword is specified, then the restore is written to that client.

-  jobid=nnn – specify a JobId or comma separated list of JobIds to be restored.

-  before=YYYY-MM-DD HH:MM:SS – specify a date and time to which the system should be restored. Only Jobs started before the specified date/time will be selected, and as is the case for current Bareos will automatically find the most recent prior Full save and all Differential and Incremental saves run before the date you specify. Note, the same formatting rules as items 6, 7 and 9 in :ref:`Restore Command <RestoreCommand>` apply to it.

-  file=filename – specify a filename to be restored. You must specify the full path and filename. Prefixing the entry with a less-than sign (<) will cause Bareos to assume that the filename is on your system and contains a list of files to be restored. Bareos will thus read the list from that file. Multiple file=xxx specifications may be specified on the command line.

-  jobid=nnn – specify a JobId to be restored.

-  pool=pool-name – specify a Pool name to be used for selection of Volumes when specifying options 5 and 6 (restore current system, and restore current system before given date). This permits you to have several Pools, possibly one offsite, and to select the Pool to be used for restoring.

-  where=/tmp/bareos-restore – restore files in where directory.

-  yes – automatically run the restore without prompting for modifications (most useful in batch scripts).

-  strip_prefix=/prod – remove a part of the filename when restoring.

-  add_prefix=/test – add a prefix to all files when restoring (like where) (can’t be used with where=).

-  add_suffix=.old – add a suffix to all your files.

-  regexwhere=!a.pdf!a.bkp.pdf! – do complex filename manipulation like with sed unix command. Will overwrite other filename manipulation. For details, see the :ref:`regexwhere <regexwhere>` section.

-  restorejob=jobname – Pre-chooses a restore job. Bareos can be configured with multiple restore jobs ("Type = Restore" in the job definition). This allows the specification of different restore properties, including a set of RunScripts. When more than one job of this type is configured, during restore, Bareos will ask for a user selection interactively, or use the given restorejob.

Using File Relocation
---------------------

:index:`\ <single: File Relocation; using>`\

.. _filerelocation:



.. _restorefilerelocation:



Introduction
~~~~~~~~~~~~

The **where=** option is simple, but not very powerful. With file relocation, Bareos can restore a file to the same directory, but with a different name, or in an other directory without recreating the full path.

You can also do filename and path manipulations, such as adding a suffix to all your files, renaming files or directories, etc. Theses options will overwrite where= option.

For example, many users use OS snapshot features so that file ``/home/eric/mbox`` will be backed up from the directory ``/.snap/home/eric/mbox``, which can complicate restores. If you use **where=/tmp**, the file will be restored to ``/tmp/.snap/home/eric/mbox`` and you will have to move the file to ``/home/eric/mbox.bkp`` by hand.

However, case, you could use the **strip_prefix=/.snap** and **add_suffix=.bkp** options and Bareos will restore the file to its original location – that is ``/home/eric/mbox``.

To use this feature, there are command line options as described in the :ref:`restore section <restorefilerelocation>` of this manual; you can modify your restore job before running it; or you can add options to your restore job in as described in :config:option:`dir/job/StripPrefix`\  and :config:option:`dir/job/AddPrefix`\ .

::

   Parameters to modify:
        1: Level
        2: Storage
       ...
       10: File Relocation
       ...
   Select parameter to modify (1-12):


   This will replace your current Where value
        1: Strip prefix
        2: Add prefix
        3: Add file suffix
        4: Enter a regexp
        5: Test filename manipulation
        6: Use this ?
   Select parameter to modify (1-6):

.. _regexwhere:

RegexWhere Format
~~~~~~~~~~~~~~~~~

The format is very close to that used by sed or Perl (``s/replace this/by that/``) operator. A valid regexwhere expression has three fields :

-  a search expression (with optional submatch)

-  a replacement expression (with optionnal back references $1 to $9)

-  a set of search options (only case-insensitive “i” at this time)

Each field is delimited by a separator specified by the user as the first character of the expression. The separator can be one of the following:

::

   <separator-keyword> = / ! ; % : , ~ # = &

You can use several expressions separated by a commas.

Examples
^^^^^^^^

==================== ===================== ================================= ==============================
Orignal filename     New filename          RegexWhere                        Comments
==================== ===================== ================================= ==============================
``c:/system.ini``    ``c:/system.old.ini`` ``/.ini$/.old.ini/``              $ matches end of name
``/prod/u01/pdata/`` ``/rect/u01/rdata``   ``/prod/rect/,/pdata/rdata/``     uses two regexp
``/prod/u01/pdata/`` ``/rect/u01/rdata``   ``!/prod/!/rect/!,/pdata/rdata/`` use ``!`` as separator
``C:/WINNT``         ``d:/WINNT``          ``/c:/d:/i``                      case insensitive pattern match
==================== ===================== ================================= ==============================

Restoring Directory Attributes
------------------------------

:index:`\ <single: Attributes; Restoring Directory>`\  :index:`\ <single: Restoring Directory Attributes>`\

Depending how you do the restore, you may or may not get the directory entries back to their original state. Here are a few of the problems you can encounter, and for same machine restores, how to avoid them.

-  You backed up on one machine and are restoring to another that is either a different OS or doesn’t have the same users/groups defined. Bareos does the best it can in these situations. Note, Bareos has saved the user/groups in numeric form, which means on a different machine, they may map to different user/group names.

-  You are restoring into a directory that is already created and has file creation restrictions. Bareos tries to reset everything but without walking up the full chain of directories and modifying them all during the restore, which Bareos does and will not do, getting permissions back correctly in this situation depends to a large extent on your OS.

-  You are doing a recursive restore of a directory tree. In this case Bareos will restore a file before restoring the file’s parent directory entry. In the process of restoring the file Bareos will create the parent directory with open permissions and ownership of the file being restored. Then when Bareos tries to restore the parent directory Bareos sees that it already exists (Similar to the previous situation). If you had set the Restore job’s "Replace" property to "never" then Bareos will
   not change the directory’s permissions and ownerships to match what it backed up, you should also notice that the actual number of files restored is less then the expected number. If you had set the Restore job’s "Replace" property to "always" then Bareos will change the Directory’s ownership and permissions to match what it backed up, also the actual number of files restored should be equal to the expected number.

-  You selected one or more files in a directory, but did not select the directory entry to be restored. In that case, if the directory is not on disk Bareos simply creates the directory with some default attributes which may not be the same as the original. If you do not select a directory and all its contents to be restored, you can still select items within the directory to be restored by individually marking those files, but in that case, you should individually use the "markdir" command to
   select all higher level directory entries (one at a time) to be restored if you want the directory entries properly restored.

.. _section-RestoreOnWindows:

Restoring on Windows
--------------------

:index:`\ <single: Restoring on Windows>`\  :index:`\ <single: Windows; Restoring on>`\

If you are restoring on Windows systems, Bareos will restore the files with the original ownerships and permissions as would be expected. This is also true if you are restoring those files to an alternate directory (using the Where option in restore). However, if the alternate directory does not already exist, the Bareos File daemon (Client) will try to create it. In some cases, it may not create the directories, and if it does since the File daemon runs under the SYSTEM account, the directory
will be created with SYSTEM ownership and permissions. In this case, you may have problems accessing the newly restored files.

To avoid this problem, you should create any alternate directory before doing the restore. Bareos will not change the ownership and permissions of the directory if it is already created as long as it is not one of the directories being restored (i.e. written to tape).

The default restore location is /tmp/bareos-restores/ and if you are restoring from drive E:, the default will be /tmp/bareos-restores/e/, so you should ensure that this directory exists before doing the restore, or use the mod option to select a different where directory that does exist.

Some users have experienced problems restoring files that participate in the Active Directory. They also report that changing the userid under which Bareos (bareos-fd.exe) runs, from SYSTEM to a Domain Admin userid, resolves the problem.

Restore Errors
--------------

:index:`\ <single: Errors; Restore>`\  :index:`\ <single: Restore Errors>`\

There are a number of reasons why there may be restore errors or warning messages. Some of the more common ones are:

file count mismatch
   This can occur for the following reasons:

   -  You requested Bareos not to overwrite existing or newer files.

   -  A Bareos miscount of files/directories. This is an on-going problem due to the complications of directories, soft/hard link, and such. Simply check that all the files you wanted were actually restored.

file size error
   When Bareos restores files, it checks that the size of the restored file is the same as the file status data it saved when starting the backup of the file. If the sizes do not agree, Bareos will print an error message. This size mismatch most often occurs because the file was being written as Bareos backed up the file. In this case, the size that Bareos restored will be greater than the status size. This often happens with log files.

   If the restored size is smaller, then you should be concerned about a possible tape error and check the Bareos output as well as your system logs.


could not hard link
   ::

         Error: findlib/create_file.cc:371 Could not hard link <destination path> -> <source path>: ERR=No such file or directory

   This error occurs when a file with multiple hard links has been deleted, and a file that should be restored
   references the original file.

   When Bareos encounters a file with multiple hard links in a backup job, it stores the file only once.
   All further occurrences of this file are stored as a reference to the first occurrence.

   When restoring a file that is a hard link reference, Bareos expects the original file to
   be present.

   The workaround for this error is to include the original file in the restore FileSet,
   and delete it again after the restore is completed.

   Note that in the filesystem, there is no distinction between the "original" hard linked file
   and other hard links. They are all directory entries pointing to the same file (inode).
   But in a Bareos job, one of the directory entries becomes the "original" file, and the
   others become "references". To restore a file which has been backed up as a reference,
   the original file must be present in the filesystem, or it must be restored in the same job.

   This special behaviour of storing hard links can be turned off in the FileSet
   resource :config:option:`dir/fileset/include/options/HardLinks = no`
   In this case, all files with hard links are backed up separately, and can be restored separately.
   Please note that this can increase the size of the backup job. After a restore,
   all files that might have been hard links will be separate files, taking up more disk space.



Example Restore Job Resource
----------------------------

:index:`\ <single: Resource; Example Restore Job>`\



::

   Job {
     Name = "RestoreFiles"
     Type = Restore
     Client = Any-client
     FileSet = "Any-FileSet"
     Storage = Any-storage
     Where = /tmp/bareos-restores
     Messages = Standard
     Pool = Default
   }



If Where is not specified, the default location for restoring files will be their original locations.

.. _Selection:



File Selection Commands
-----------------------

:index:`\ <single: Console; File Selection>`\  :index:`\ <single: File Selection Commands>`\

After you have selected the Jobs to be restored and Bareos has created the in-memory directory tree, you will enter file selection mode as indicated by the dollar sign ($) prompt. While in this mode, you may use the commands listed above. The basic idea is to move up and down the in memory directory structure with the cd command much as you normally do on the system. Once you are in a directory, you may select the files that you want restored. As a default no files are marked to be restored. If
you wish to start with all files, simply enter: cd / and mark \*. Otherwise proceed to select the files you wish to restore by marking them with the mark command. The available commands are:

cd
   :index:`\ <single: Console; File Selection; cd>`\  The cd command changes the current directory to the argument specified. It operates much like the Unix cd command. Wildcard specifications are not permitted.

   Note, on Windows systems, the various drives (c:, d:, ...) are treated like a directory within the file tree while in the file selection mode. As a consequence, you must do a cd c: or possibly in some cases a cd C: (note upper case) to get down to the first directory.

dir
   :index:`\ <single: Console; File Selection; dir>`\  The dir command is similar to the ls command, except that it prints it in long format (all details). This command can be a bit slower than the ls command because it must access the catalog database for the detailed information for each file.

estimate
   :index:`\ <single: Console; File Selection; estimate>`\  The estimate command prints a summary of the total files in the tree, how many are marked to be restored, and an estimate of the number of bytes to be restored. This can be useful if you are short on disk space on the machine where the files will be restored.

find
   :index:`\ <single: Console; File Selection; find>`\  The find command accepts one or more arguments and displays all files in the tree that match that argument. The argument may have wildcards. It is somewhat similar to the Unix command find / -name arg.

ls
   :index:`\ <single: Console; File Selection; ls>`\  The ls command produces a listing of all the files contained in the current directory much like the Unix ls command. You may specify an argument containing wildcards, in which case only those files will be listed.

   Any file that is marked to be restored will have its name preceded by an asterisk (). Directory names will be terminated with a forward slash (/) to distinguish them from filenames.

lsmark
   :index:`\ <single: Console; File Selection; lsmark>`\  The lsmark command is the same as the ls except that it will print only those files marked for extraction. The other distinction is that it will recursively descend into any directory selected.

mark
   :index:`\ <single: Console; File Selection; mark>`\  The mark command allows you to mark files to be restored. It takes a single argument which is the filename or directory name in the current directory to be marked for extraction. The argument may be a wildcard specification, in which case all files that match in the current directory are marked to be restored. If the argument matches a directory rather than a file, then the directory and all the files contained in that directory
   (recursively) are marked to be restored. Any marked file will have its name preceded with an asterisk () in the output produced by the ls or dir commands. Note, supplying a full path on the mark command does not work as expected to select a file or directory in the current directory. Also, the mark command works on the current and lower directories but does not touch higher level directories.

   After executing the mark command, it will print a brief summary:



   ::

          No files marked.



   If no files were marked, or:



   ::

          nn files marked.



   if some files are marked.

unmark
   :index:`\ <single: Console; File Selection; unmark>`\  The unmark is identical to the mark command, except that it unmarks the specified file or files so that they will not be restored. Note: the unmark command works from the current directory, so it does not unmark any files at a higher level. First do a cd / before the unmark \* command if you want to unmark everything.

pwd
   :index:`\ <single: Console; File Selection; pwd>`\  The pwd command prints the current working directory. It accepts no arguments.

count
   :index:`\ <single: Console; File Selection; count>`\  The count command prints the total files in the directory tree and the number of files marked to be restored.

done
   :index:`\ <single: Console; File Selection; done>`\  This command terminates file selection mode.

exit
   :index:`\ <single: Console; File Selection; exit>`\  This command terminates file selection mode (the same as done).

quit
   :index:`\ <single: Console; File Selection; quit>`\  This command terminates the file selection and does not run the restore job.

help
   :index:`\ <single: Console; File Selection; help>`\  This command prints a summary of the commands available.

?
   :index:`\ <single: Console; File Selection; ?>`\  This command is the same as the help command.

If your filename contains some weird caracters, you can use ``?``, ``*`` or \\\. For example, if your filename contains a \\, you can use \\\\\.

::

   * mark weird_file\\\\with-backslash
