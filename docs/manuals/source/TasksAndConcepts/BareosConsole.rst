.. _section-bconsole:

Bareos Console
==============

:index:`\ <single: Bareos Console>`\  :index:`\ <single: bconsole>`\  :index:`\ <single: Command; bconsole>`\

The Bareos Console (:command:`bconsole`) is a program that allows the user or the System Administrator, to interact with the Bareos Director daemon while the daemon is running.

The current Bareos Console comes as a shell interface (TTY style). It permit the administrator or authorized users to interact with Bareos. You can determine the status of a particular job, examine the contents of the Catalog as well as perform certain tape manipulations with the Console program.

Since the Console program interacts with the Director through the network, your Console and Director programs do not necessarily need to run on the same machine.

In fact, a certain minimal knowledge of the Console program is needed in order for Bareos to be able to write on more than one tape, because when Bareos requests a new tape, it waits until the user, via the Console program, indicates that the new tape is mounted.

Console Configuration
---------------------

:index:`\ <single: Configuration; Console>`\  :index:`\ <single: Configuration; bconsole>`\

When the Console starts, it reads a standard Bareos configuration file named bconsole.conf unless you specify the -c command line option (see below). This file allows default configuration of the Console, and at the current time, the only Resource Record defined is the Director resource, which gives the Console the name and address of the Director. For more information on configuration of the Console program, please see the :ref:`Console Configuration <ConsoleConfChapter>` chapter
of this document.

Running the Console Program
---------------------------

The console program can be run with the following options:

:index:`\ <single: Command Line Options>`\

.. code-block:: shell-session
   :caption: bconsole command line options

   root@host:~# bconsole -?
   Usage: bconsole [-s] [-c config_file] [-d debug_level]
          -D <dir>    select a Director
          -l          list Directors defined
          -c <path>   specify configuration file or directory
          -p <file>   specify pam credentials file
          -o          send pam credentials over unencrypted connection
          -d <nn>     set debug level to <nn>
          -dt         print timestamp in debug output
          -s          no signals
          -u <nn>     set command execution timeout to <nn> seconds
          -t          test - read configuration and exit
          -xc         print configuration and exit
          -xs         print configuration file schema in JSON format and exit
          -?          print this message.

After launching the Console program (bconsole), it will prompt you for the next command with an asterisk (*). Generally, for all commands, you can simply enter the command name and the Console program will prompt you for the necessary arguments. Alternatively, in most cases, you may enter the command followed by arguments. The general format is:



::

    <command> <keyword1>[=<argument1>] <keyword2>[=<argument2>] ...



where command is one of the commands listed below; keyword is one of the keywords listed below (usually followed by an argument); and argument is the value. The command may be abbreviated to the shortest unique form. If two commands have the same starting letters, the one that will be selected is the one that appears first in the help listing. If you want the second command, simply spell out the full command. None of the keywords following the command may be abbreviated.

For example:



::

   list files jobid=23



will list all files saved for JobId 23. Or:



::

   show pools



will display all the Pool resource records.

The maximum command line length is limited to 511 characters, so if you are scripting the console, you may need to take some care to limit the line length.

Exit the Console Program
~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Command; bconsole; exit>`\

Normally, you simply enter quit or exit and the Console program will terminate. However, it waits until the Director acknowledges the command. If the Director is already doing a lengthy command (e.g. prune), it may take some time. If you want to immediately terminate the Console program, enter the .quit command.

There is currently no way to interrupt a Console command once issued (i.e. Ctrl-C does not work). However, if you are at a prompt that is asking you to select one of several possibilities and you would like to abort the command, you can enter a period (.), and in most cases, you will either be returned to the main command prompt or if appropriate the previous prompt (in the case of nested prompts). In a few places such as where it is asking for a Volume name, the period will be taken to be the
Volume name. In that case, you will most likely be able to cancel at the next prompt.

Running the Console from a Shell Script
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Console; Running from a Shell>`\

.. _scripting:



You can automate many Console tasks by running the console program from a shell script. For example, if you have created a file containing the following commands:



::

    bconsole -c ./bconsole.conf <<END_OF_DATA
    unmount storage=DDS-4
    quit
    END_OF_DATA



when that file is executed, it will unmount the current DDS-4 storage device. You might want to run this command during a Job by using the RunBeforeJob or RunAfterJob records.

It is also possible to run the Console program from file input where the file contains the commands as follows:



::

   bconsole -c ./bconsole.conf <filename



where the file named filename contains any set of console commands.

As a real example, the following script is part of the Bareos systemtests. It labels a volume (a disk volume), runs a backup, then does a restore of the files saved.



::

   bconsole <<END_OF_DATA
   @output /dev/null
   messages
   @output /tmp/log1.out
   label volume=TestVolume001
   run job=Client1 yes
   wait
   messages
   @#
   @# now do a restore
   @#
   @output /tmp/log2.out
   restore current all
   yes
   wait
   messages
   @output
   quit
   END_OF_DATA



The output from the backup is directed to /tmp/log1.out and the output from the restore is directed to /tmp/log2.out. To ensure that the backup and restore ran correctly, the output files are checked with:



::

   grep "^ *Termination: *Backup OK" /tmp/log1.out
   backupstat=$?
   grep "^ *Termination: *Restore OK" /tmp/log2.out
   restorestat=$?



Console Keywords
----------------

:index:`\ <single: Console; Keywords>`\

Unless otherwise specified, each of the following keywords takes an argument, which is specified after the keyword following an equal sign. For example:

::

   jobid=536

all
   Permitted on the status and show commands to specify all components or resources respectively.

allfrompool
   Permitted on the update command to specify that all Volumes in the pool (specified on the command line) should be updated.

allfrompools
   Permitted on the update command to specify that all Volumes in all pools should be updated.

before
   Used in the restore command.

bootstrap
   Used in the restore command.

catalog
   Allowed in the use command to specify the catalog name to be used.

catalogs
   Used in the show command. Takes no arguments.

client | fd
   Used to specify a client (or filedaemon).

clients
   Used in the show, list, and llist commands. Takes no arguments.

counters
   Used in the show command. Takes no arguments.

current
   Used in the restore command. Takes no argument.

days
   Used to define the number of days the :bcommand:`list nextvol` command should consider when looking for jobs to be run. The days keyword can also be used on the :bcommand:`status dir` command so that it will display jobs scheduled for the number of days you want. It can also be used on the :bcommand:`rerun` command, where it will automatically select all failed jobids in the last number of days for rerunning.

devices
   Used in the show command. Takes no arguments.

director | dir | directors
   Used in the show and status command. Takes no arguments.

directory
   Used in the restore command. Its argument specifies the directory to be restored.

enabled
   This keyword can appear on the :bcommand:`update volume` as well as the :bcommand:`update slots` commands, and can allows one of the following arguments: yes, true, no, false, archived, 0, 1, 2. Where 0 corresponds to no or false, 1 corresponds to yes or true, and 2 corresponds to archived. Archived volumes will not be used, nor will the Media record in the catalog be pruned. Volumes that are not enabled, will not be used for backup or restore.

done
   Used in the restore command. Takes no argument.

file
   Used in the restore command.

files
   Used in the list and llist commands. Takes no arguments.

fileset
   Used in the run and restore command. Specifies the fileset.

filesets
   Used in the show command. Takes no arguments.

help
   Used in the show command. Takes no arguments.

hours
   Used on the :bcommand:`rerun` command to select all failed jobids in the last number of hours for rerunning.

jobs
   Used in the show, list and llist commands. Takes no arguments.

jobmedia
   Used in the list and llist commands. Takes no arguments.

jobtotals
   Used in the list and llist commands. Takes no arguments.

jobid
   The JobId is the numeric jobid that is printed in the Job Report output. It is the index of the database record for the given job. While it is unique for all the existing Job records in the catalog database, the same JobId can be reused once a Job is removed from the catalog. Probably you will refer specific Jobs that ran using their numeric JobId.

   JobId can be used on the :bcommand:`rerun` command to select all jobs failed after and including the given jobid for rerunning.

job | jobname
   The Job or Jobname keyword refers to the name you specified in the Job resource, and hence it refers to any number of Jobs that ran. It is typically useful if you want to list all jobs of a particular name.

level
   Used in the run command. Specifies the backup level.

listing
   Permitted on the estimate command. Takes no argument.

limit
   Specifies the maximum number of items in the result.

messages
   Used in the show command. Takes no arguments.

media
   Used in the list and llist commands. Takes no arguments.

nextvolume | nextvol
   Used in the list and llist commands. Takes no arguments.

on
   Takes no arguments.

off
   Takes no arguments.

pool
   Specify the pool to be used.

pools
   Used in the show, list, and llist commands. Takes no arguments.

select
   Used in the restore command. Takes no argument.

limit
   Used in the setbandwidth command. Takes integer in KB/s unit.

schedules
   Used in the show command. Takes no arguments.

storage | store | sd
   Used to specify the name of a storage daemon.

storages
   Used in the show command. Takes no arguments.

ujobid
   The ujobid is a unique job identification that is printed in the Job Report output. At the current time, it consists of the Job name (from the Name directive for the job) appended with the date and time the job was run. This keyword is useful if you want to completely identify the Job instance run.

volume
   Used to specify a volume.

volumes
   Used in the list and llist commands. Takes no arguments.

where
   Used in the restore command.

yes
   Used in the restore command. Takes no argument.

.. _section-ConsoleCommands:

Console Commands
----------------

The following commands are currently implemented:

add
   :index:`\ <single: Console; Command; add|textbf>`\  This command is used to add Volumes to an existing Pool. That is, it creates the Volume name in the catalog and inserts into the Pool in the catalog, but does not attempt to access the physical Volume. Once added, Bareos expects that Volume to exist and to be labeled. This command is not normally used since Bareos will automatically do the equivalent when Volumes are labeled. However, there may be times when you have removed a Volume
   from the catalog and want to later add it back.

   The full form of this command is:

   .. code-block:: bconsole
      :caption: add

      add [pool=<pool-name>] [storage=<storage>] [jobid=<JobId>]

   Normally, the :bcommand:`label` command is used rather than this command because the :bcommand:`label` command labels the physical media (tape, disk,, ...) and does the equivalent of the :bcommand:`add` command. The :bcommand:`add` command affects only the Catalog and not the physical media (data on Volumes). The physical media must exist and be labeled before use (usually with the :bcommand:`label` command). This command
   can, however, be useful if you wish to add a number of Volumes to the Pool that will be physically labeled at a later time. It can also be useful if you are importing a tape from another site. Please see the :bcommand:`label` command for the list of legal characters in a Volume name.

autodisplay
   :index:`\ <single: Console; Command; autodisplay on/off>`\  This command accepts on or off as an argument, and turns auto-display of messages on or off respectively. The default for the console program is off, which means that you will be notified when there are console messages pending, but they will not automatically be displayed.

   When autodisplay is turned off, you must explicitly retrieve the messages with the messages command. When autodisplay is turned on, the messages will be displayed on the console as they are received.

automount
   :index:`\ <single: Console; Command; automount on/off>`\  This command accepts on or off as the argument, and turns auto-mounting of the Volume after a label command on or off respectively. The default is on. If automount is turned off, you must explicitly mount tape Volumes after a label command to use it.

cancel
   :index:`\ <single: Console; Command; cancel jobid>`\  This command is used to cancel a job and accepts jobid=nnn or job=xxx as an argument where nnn is replaced by the JobId and xxx is replaced by the job name. If you do not specify a keyword, the Console program will prompt you with the names of all the active jobs allowing you to choose one.

   The full form of this command is:

   .. code-block:: bconsole
      :caption: cancel

      cancel [jobid=<number> job=<job-name> ujobid=<unique-jobid>]

   Once a Job is marked to be cancelled, it may take a bit of time (generally within a minute but up to two hours) before the Job actually terminates, depending on what operations it is doing. Don’t be surprised that you receive a Job not found message. That just means that one of the three daemons had already canceled the job. Messages numbered in the 1000’s are from the Director, 2000’s are from the File daemon and 3000’s from the Storage daemon.

   It is possible to cancel multiple jobs at once. Therefore, the following extra options are available for the job-selection:

   -  all jobs

   -  all jobs with a created state

   -  all jobs with a blocked state

   -  all jobs with a waiting state

   -  all jobs with a running state

   Usage:

   .. code-block:: bconsole
      :caption: cancel all

      cancel all
      cancel all state=<created|blocked|waiting|running>

   Sometimes the Director already removed the job from its running queue, but the storage daemon still thinks it is doing a backup (or another job) - so you cannot cancel the job from within a console anymore. Therefore it is possible to cancel a job by JobId on the storage daemon. It might be helpful to execute a :bcommand:`status storage` on the Storage Daemon to make sure what job you want to cancel.

   Usage:

   .. code-block:: bconsole
      :caption: cancel on Storage Daemon

      cancel storage=<Storage Daemon> Jobid=<JobId>

   This way you can also remove a job that blocks any other jobs from running without the need to restart the whole storage daemon.

create
   :index:`\ <single: Console; Command; create pool>`\  This command is not normally used as the Pool records are automatically created by the Director when it starts based on what it finds in the configuration. If needed, this command can be used, to create a Pool record in the database using the Pool resource record defined in the Director’s configuration file. So in a sense, this command simply transfers the information from the Pool resource in the configuration file into the Catalog.
   Normally this command is done automatically for you when the Director starts providing the Pool is referenced within a Job resource. If you use this command on an existing Pool, it will automatically update the Catalog to have the same information as the Pool resource. After creating a Pool, you will most likely use the label command to label one or more volumes and add their names to the Media database.

   The full form of this command is:

   .. code-block:: bconsole
      :caption: create

      create [pool=<pool-name>]

   When starting a Job, if Bareos determines that there is no Pool record in the database, but there is a Pool resource of the appropriate name, it will create it for you. If you want the Pool record to appear in the database immediately, simply use this command to force it to be created.


.. _section-bcommandConfigure:

configure
   Configures director resources during runtime. The first configure subcommands are :bcommand:`configure add` and :bcommand:`configure export`. Other subcommands may follow in later releases.

   .. _section-bcommandConfigureAdd:

   configure add
      :index:`\ <single: Console; Command; configure add>`

      This command allows to add resources during runtime. Usage:

      .. code-block:: bconsole
         :caption: configure add usage

         configure add <resourcetype> name=<resourcename> <directive1>=<value1> <directive2>=<value2> ...

      Values that must be quoted in the resulting configuration must be added as:

      .. code-block:: bconsole
         :caption: configure add usage with values containing spaces

         configure add <resourcetype> name=<resourcename> <directive1>="\"<value containing spaces>\"" ...

      The command generates and loads a new valid resource. As the new resource is also stored at

      :file:`<CONFIGDIR>/bareos-dir.d/<resourcetype>/<resourcename>.conf`

      (see :ref:`section-ConfigurationResourceFileConventions`) it is persistent upon reload and restart.

      This feature requires :ref:`section-ConfigurationSubdirectories`.

      All kinds of resources can be added. When adding a client resource, the :ref:`ClientResourceDirector` for the |fd| is also created and stored at:

      :file:`<CONFIGDIR>/bareos-dir-export/client/<clientname>/bareos-fd.d/director/<clientname>.conf`

      .. code-block:: bconsole
         :caption: Example: adding a client and a job resource during runtime

         *<input>configure add client name=client2-fd address=192.168.0.2 password=secret</input>
         Created resource config file "/etc/bareos/bareos-dir.d/client/client2-fd.conf":
         Client {
           Name = client2-fd
           Address = 192.168.0.2
           Password = secret
         }
         *<input>configure add job name=client2-job client=client2-fd jobdefs=DefaultJob</input>
         Created resource config file "/etc/bareos/bareos-dir.d/job/client2-job.conf":
         Job {
           Name = client2-job
           Client = client2-fd
           JobDefs = DefaultJob
         }

      These two commands create three resource configuration files:

      - :file:`/etc/bareos/bareos-dir.d/client/client2-fd.conf`

      - :file:`/etc/bareos/bareos-dir-export/client/client2-fd/bareos-fd.d/director/bareos-dir.conf` (assuming your director resource is named **bareos-dir**)

      - :file:`/etc/bareos/bareos-dir.d/job/client2-job.conf`

      The files in :file:`bareos-dir-export/client/` directory are not used by the |dir|. However, they can be copied to new clients to configure these clients for the |dir|.

      .. warning::

         Don't be confused by the extensive output of :bcommand:`help configure`. As :bcommand:`configure add` allows configuring arbitrary resources, the output of :bcommand:`help configure` lists all the resources, each with all valid directives. The same data is also used for :command:`bconsole` command line completion.

      Available since Bareos :sinceVersion:`16.2.4: configure add`.


   .. _section-bcommandConfigureExport:

   configure export
      :index:`\ <single: Console; Command; configure export>`

      This command allows to export the :config:option:`Fd/Director`\  resource for clients already configured in the |dir|.

      Usage:

      .. code-block:: bconsole
         :caption: Export the bareos-fd Director resource for the client bareos-fd

         configure export client=bareos-fd
         Exported resource file "/etc/bareos/bareos-dir-export/client/bareos-fd/bareos-fd.d/director/bareos-dir.conf":
         Director {
           Name = bareos-dir
           Password = "[md5]932d1d3ef3c298047809119510f4bee6"
         }

      To use it, copy the :config:option:`Fd/Director`\  resource file to the client machine (on Linux: to :file:`/etc/bareos/bareos-fd.d/director/`) and restart the |fd|.

      Available since Bareos :sinceVersion:`16.2.4: configure export`.

delete
   :index:`\ <single: Console; Command; delete>`\  The delete command is used to delete a Volume, Pool or Job record from the Catalog as well as all associated catalog Volume records that were created. This command operates only on the Catalog database and has no effect on the actual data written to a Volume. This command can be dangerous and we strongly recommend that you do not use it unless you know what you are doing.

   If the keyword Volume appears on the command line, the named Volume will be deleted from the catalog. If the keyword Pool appears on the command line, a Pool will be deleted. If the keyword Job appears on the command line, a Job and all its associated records (File and JobMedia) will be deleted from the catalog. If the keyword storage appears on the command line, the orphaned storage with the selected name will be deleted.

   The full form of this command is:

   .. code-block:: bconsole
      :caption: delete

      delete pool=<pool-name>
      delete volume=<volume-name> pool=<pool-name>
      delete JobId=<job-id> JobId=<job-id2> ...
      delete Job JobId=n,m,o-r,t ...
      delete storage=<storage-name>

   The first form deletes a Pool record from the catalog database. The second form deletes a Volume record from the specified pool in the catalog database. The third form deletes the specified Job record from the catalog database. The fourth form deletes JobId records for JobIds n, m, o, p, q, r, and t. Where each one of the n,m,... is, of course, a number. That is a "delete jobid" accepts lists and ranges of jobids. The last form deletes the selected storage from the database, only if it is orphaned, meaning if still persists in the database even though its configuration has been removed and there are no volumes or devices associated with it anymore.

disable
   :index:`\ <single: Console; Command; disable>`\  This command permits you to disable a Job for automatic scheduling. The job may have been previously enabled with the Job resource Enabled directive or using the console enable command. The next time the Director is reloaded or restarted, the Enable/Disable state will be set to the value in the Job resource (default enabled) as defined in the |dir| configuration.

   The full form of this command is:

   .. code-block:: bconsole
      :caption: disable

      disable job=<job-name>

enable
   :index:`\ <single: Console; Command; enable>`\  This command permits you to enable a Job for automatic scheduling. The job may have been previously disabled with the Job resource Enabled directive or using the console disable command. The next time the Director is reloaded or restarted, the Enable/Disable state will be set to the value in the Job resource (default enabled) as defined in the |dir| configuration.

   The full form of this command is:

   .. code-block:: bconsole
      :caption: enable

      enable job=<job-name>



.. _estimate:

estimate
   :index:`\ <single: Console; Command; estimate>`\  Using this command, you can get an idea how many files will be backed up, or if you are unsure about your Include statements in your FileSet, you can test them without doing an actual backup. The default is to assume a Full backup. However, you can override this by specifying a level=Incremental or level=Differential on the command line. A Job name must be specified or you will be prompted for one, and optionally a Client and FileSet may
   be specified on the command line. It then contacts the client which computes the number of files and bytes that would be backed up. Please note that this is an estimate calculated from the number of blocks in the file rather than by reading the actual bytes. As such, the estimated backup size will generally be larger than an actual backup.

   The ``estimate`` command can use the accurate code to detect changes and give a better estimation. You can set the accurate behavior on command line using ``accurate=yes/no`` or use the Job setting as default value.

   Optionally you may specify the keyword listing in which case, all the files to be backed up will be listed. Note, it could take quite some time to display them if the backup is large. The full form is:

   The full form of this command is:

   .. code-block:: bconsole
      :caption: estimate

      estimate job=<job-name> listing client=<client-name> accurate=<yes|no> fileset=<fileset-name> level=<level-name>

   Specification of the job is sufficient, but you can also override the client, fileset, accurate and/or level by specifying them on the estimate command line.

   As an example, you might do:

   .. code-block:: bconsole
      :caption: estimate: redirected output

      @output /tmp/listing
      estimate job=NightlySave listing level=Incremental
      @output

   which will do a full listing of all files to be backed up for the Job NightlySave during an Incremental save and put it in the file /tmp/listing. Note, the byte estimate provided by this command is based on the file size contained in the directory item. This can give wildly incorrect estimates of the actual storage used if there are sparse files on your systems. Sparse files are often found on 64 bit systems for certain system files. The size that is returned is the size Bareos will backup if
   the sparse option is not specified in the FileSet. There is currently no way to get an estimate of the real file size that would be found should the sparse option be enabled.

exit
   :index:`\ <single: Console; Command; exit>`\  This command terminates the console program.

export
   :index:`\ <single: Console; Command; export>`\  The export command is used to export tapes from an autochanger. Most Automatic Tapechangers offer special slots for importing new tape cartridges or exporting written tape cartridges. This can happen without having to set the device offline.

   The full form of this command is:

   .. code-block:: bconsole
      :caption: export

      export storage=<storage-name> srcslots=<slot-selection> [dstslots=<slot-selection> volume=<volume-name> scan]

   The export command does exactly the opposite of the import command. You can specify which slots should be transferred to import/export slots. The most useful application of the export command is the possibility to automatically transfer the volumes of a certain backup into the import/export slots for external storage.

   To be able to to this, the export command also accepts a list of volume names to be exported.

   Example:

   .. code-block:: bconsole
      :caption: export volume

      export volume=A00020L4|A00007L4|A00005L4

   Instead of exporting volumes by names you can also select a number of slots via the srcslots keyword and export those to the slots you specify in dstslots. The export command will check if the slots have content (e.g. otherwise there is not much to export) and if there are enough export slots and if those are really import/export slots.

   Example:

   .. code-block:: bconsole
      :caption: export slots

      export srcslots=1-2 dstslots=37-38

   To automatically export the Volumes used by a certain backup job, you can use the following RunScript in that job:

   .. code-block:: bconsole
      :caption: automatic export

      RunScript {
          Console = "export storage=TandbergT40 volume=%V"
          RunsWhen = After
          RunsOnClient = no
      }

   To send an e-mail notification via the Messages resource regarding export tapes you can use the Variable %V substitution in the Messages resource, which is implemented in Bareos 13.2. However, it does also work in earlier releases inside the job resources. So in versions prior to Bareos 13.2 the following workaround can be used:

   .. code-block:: bconsole
      :caption: e-mail notification via messages resource regarding export tapes

      RunAfterJob = "/bin/bash -c \"/bin/echo Remove Tape %V | \
      /usr/sbin/bsmtp -h localhost -f root@localhost -s 'Remove Tape %V' root@localhost \""

gui
   :index:`\ <single: Console; Command; gui>`\  Invoke the non-interactive gui mode. This command is only used when :command:`bconsole` is commanded by an external program.

help
   :index:`\ <single: Console; Command; help>`\  This command displays the list of commands available.

import
   :index:`\ <single: Console; Command; import>`\  The import command is used to import tapes into an autochanger. Most Automatic Tapechangers offer special slots for importing new tape cartridges or exporting written tape cartridges. This can happen without having to set the device offline.

   The full form of this command is:

   .. code-block:: bconsole
      :caption: import

      import storage=<storage-name> [srcslots=<slot-selection> dstslots=<slot-selection> volume=<volume-name> scan]

   To import new tapes into the autochanger, you only have to load the new tapes into the import/export slots and call import from the cmdline.

   The import command will automatically transfer the new tapes into free slots of the autochanger. The slots are filled in order of the slot numbers. To import all tapes, there have to be enough free slots to load all tapes.

   Example with a Library with 36 Slots and 3 Import/Export Slots:

   .. code-block:: bconsole
      :caption: import example

      *import storage=TandbergT40
      Connecting to Storage daemon TandbergT40 at bareos:9103 ...
      3306 Issuing autochanger "slots" command.
      Device "Drive-1" has 39 slots.
      Connecting to Storage daemon TandbergT40 at bareos:9103 ...
      3306 Issuing autochanger "listall" command.
      Connecting to Storage daemon TandbergT40 at bareos:9103 ...
      3306 Issuing autochanger transfer command.
      3308 Successfully transfered volume from slot 37 to 20.
      Connecting to Storage daemon TandbergT40 at bareos:9103 ...
      3306 Issuing autochanger transfer command.
      3308 Successfully transfered volume from slot 38 to 21.
      Connecting to Storage daemon TandbergT40 at bareos:9103 ...
      3306 Issuing autochanger transfer command.
      3308 Successfully transfered volume from slot 39 to 25.

   You can also import certain slots when you don’t have enough free slots in your autochanger to put all the import/export slots in.

   Example with a Library with 36 Slots and 3 Import/Export Slots importing one slot:

   .. code-block:: bconsole
      :caption: import example

      *import storage=TandbergT40 srcslots=37 dstslots=20
      Connecting to Storage daemon TandbergT40 at bareos:9103 ...
      3306 Issuing autochanger "slots" command.
      Device "Drive-1" has 39 slots.
      Connecting to Storage daemon TandbergT40 at bareos:9103 ...
      3306 Issuing autochanger "listall" command.
      Connecting to Storage daemon TandbergT40 at bareos:9103 ...
      3306 Issuing autochanger transfer command.
      3308 Successfully transfered volume from slot 37 to 20.

label
   :index:`\ <single: Console; Command; label>`\  :index:`\ <single: Console; Command; relabel>`\  This command is used to label physical volumes. The full form of this command is:

   .. code-block:: bconsole
      :caption: label

      label storage=<storage-name> volume=<volume-name> slot=<slot>

   If you leave out any part, you will be prompted for it. The media type is automatically taken from the Storage resource definition that you supply. Once the necessary information is obtained, the Console program contacts the specified Storage daemon and requests that the Volume be labeled. If the Volume labeling is successful, the Console program will create a Volume record in the appropriate Pool.

   The Volume name is restricted to letters, numbers, and the special characters hyphen (-), underscore (\_), colon (:), and period (.). All other characters including a space are invalid. This restriction is to ensure good readability of Volume names to reduce operator errors.

   Please note, when labeling a blank tape, Bareos will get read I/O error when it attempts to ensure that the tape is not already labeled. If you wish to avoid getting these messages, please write an EOF mark on your tape before attempting to label it:



   ::

             mt rewind
             mt weof



   The label command can fail for a number of reasons:

   #. The Volume name you specify is already in the Volume database.

   #. The Storage daemon has a tape or other Volume already mounted on the device, in which case you must unmount the device, insert a blank tape, then do the label command.

   #. The Volume in the device is already a Bareos labeled Volume. (Bareos will never relabel a Bareos labeled Volume unless it is recycled and you use the relabel command).

   #. There is no Volume in the drive.

   There are two ways to relabel a volume that already has a Bareos label. The brute force method is to write an end of file mark on the tape using the system mt program, something like the following:

   ::

             mt -f /dev/st0 rewind
             mt -f /dev/st0 weof

   For a disk volume, you would manually delete the Volume.

   Then you use the label command to add a new label. However, this could leave traces of the old volume in the catalog.

   The preferable method to relabel a Volume is to first purge the volume, either automatically, or explicitly with the :bcommand:`purge` command, then use the :bcommand:`relabel` command described below.

   If your autochanger has barcode labels, you can label all the Volumes in your autochanger one after another by using the :bcommand:`label barcodes` command. For each tape in the changer containing a barcode, Bareos will mount the tape and then label it with the same name as the barcode. An appropriate Media record will also be created in the catalog. Any barcode that begins with the same characters as specified on the "CleaningPrefix=xxx" (default is "CLN") directive in the
   Director’s Pool resource, will be treated as a cleaning tape, and will not be labeled. However, an entry for the cleaning tape will be created in the catalog. For example with:

   .. code-block:: bareosconfig
      :caption: Cleaning Tape

      Pool {
          Name ...
          Cleaning Prefix = "CLN"
      }

   Any slot containing a barcode of CLNxxxx will be treated as a cleaning tape and will not be mounted. Note, the full form of the command is:

   .. code-block:: bconsole
      :caption: label

      label storage=xxx pool=yyy slots=1-5,10 barcodes

list
   :index:`\ <single: Console; Command; list>`\  The list command lists the requested contents of the Catalog. The various fields of each record are listed on a single line. The various forms of the list command are:

   .. code-block:: bconsole
      :caption: list

      list jobs
      list jobid=<id>           (list jobid id)
      list ujobid=<unique job name> (list job with unique name)
      list job=<job-name>   (list all jobs with "job-name")
      list jobname=<job-name>  (same as above)
          In the above, you can add "limit=nn" to limit the output to nn jobs.
      list joblog jobid=<id> (list job output if recorded in the catalog)
      list jobmedia
      list jobmedia jobid=<id>
      list jobmedia job=<job-name>
      list files jobid=<id>
      list files job=<job-name>
      list pools
      list clients
      list jobtotals
      list volumes
      list volumes jobid=<id>
      list volumes pool=<pool-name>
      list volumes job=<job-name>
      list volume=<volume-name>
      list nextvolume job=<job-name>
      list nextvol job=<job-name>
      list nextvol job=<job-name> days=nnn

   What most of the above commands do should be more or less obvious. In general if you do not specify all the command line arguments, the command will prompt you for what is needed.

   The :bcommand:`list nextvol` command will print the Volume name to be used by the specified job. You should be aware that exactly what Volume will be used depends on a lot of factors including the time and what a prior job will do. It may fill a tape that is not full when you issue this command. As a consequence, this command will give you a good estimate of what Volume will be used but not a definitive answer. In addition, this command may have certain side effect because it
   runs through the same algorithm as a job, which means it may automatically purge or recycle a Volume. By default, the job specified must run within the next two days or no volume will be found. You can, however, use the days=nnn specification to specify up to 50 days. For example, if on Friday, you want to see what Volume will be needed on Monday, for job MyJob, you would use :bcommand:`list nextvol job=MyJob days=3`.

   If you wish to add specialized commands that list the contents of the catalog, you can do so by adding them to the :file:`query.sql` file. However, this takes some knowledge of programming SQL. Please see the :bcommand:`query` command below for additional information. See below for listing the full contents of a catalog record with the :bcommand:`llist` command.

   As an example, the command list pools might produce the following output:

   .. code-block:: bconsole
      :caption: list pools

      *<input>list pools</input>
      +------+---------+---------+---------+----------+-------------+
      | PoId | Name    | NumVols | MaxVols | PoolType | LabelFormat |
      +------+---------+---------+---------+----------+-------------+
      |    1 | Default |       0 |       0 | Backup   | *           |
      |    2 | Recycle |       0 |       8 | Backup   | File        |
      +------+---------+---------+---------+----------+-------------+

   As mentioned above, the list command lists what is in the database. Some things are put into the database immediately when Bareos starts up, but in general, most things are put in only when they are first used, which is the case for a Client as with Job records, etc.

   Bareos should create a client record in the database the first time you run a job for that client. Doing a status will not cause a database record to be created. The client database record will be created whether or not the job fails, but it must at least start. When the Client is actually contacted, additional info from the client will be added to the client record (a "uname -a" output).

   If you want to see what Client resources you have available in your conf file, you use the Console command show clients.

llist
   :index:`\ <single: Console; Command; llist>`\  The llist or "long list" command takes all the same arguments that the list command described above does. The difference is that the llist command list the full contents of each database record selected. It does so by listing the various fields of the record vertically, with one field per line. It is possible to produce a very large number of output lines with this command.

   If instead of the list pools as in the example above, you enter llist pools you might get the following output:

   .. code-block:: bconsole
      :caption: llist pools

      *<input>llist pools</input>
                PoolId: 1
                  Name: Default
               NumVols: 0
               MaxVols: 0
               UseOnce: 0
            UseCatalog: 1
       AcceptAnyVolume: 1
          VolRetention: 1,296,000
        VolUseDuration: 86,400
            MaxVolJobs: 0
           MaxVolBytes: 0
             AutoPrune: 0
               Recycle: 1
              PoolType: Backup
           LabelFormat: *

                PoolId: 2
                  Name: Recycle
               NumVols: 0
               MaxVols: 8
               UseOnce: 0
            UseCatalog: 1
       AcceptAnyVolume: 1
          VolRetention: 3,600
        VolUseDuration: 3,600
            MaxVolJobs: 1
           MaxVolBytes: 0
             AutoPrune: 0
               Recycle: 1
              PoolType: Backup
           LabelFormat: File

messages
   :index:`\ <single: Console; Command; messages>`\  This command causes any pending console messages to be immediately displayed.

memory
   :index:`\ <single: Console; Command; memory>`\  Print current memory usage.

mount
   :index:`\ <single: Console; Command; mount>`\  The mount command is used to get Bareos to read a volume on a physical device. It is a way to tell Bareos that you have mounted a tape and that Bareos should examine the tape. This command is normally used only after there was no Volume in a drive and Bareos requests you to mount a new Volume or when you have specifically unmounted a Volume with the :bcommand:`unmount` console command, which causes Bareos to close the drive. If
   you have an autoloader, the mount command will not cause Bareos to operate the autoloader unless you specify a slot and possibly a drive. The various forms of the mount command are:

   .. code-block:: bconsole
      :caption: mount

      mount storage=<storage-name> [slot=<num>] [drive=<num>]
      mount [jobid=<id> | job=<job-name>]

   If you have specified :config:option:`sd/device/AutomaticMount = yes`\ , under most circumstances, Bareos will automatically access the Volume unless you have explicitly unmounted it (in the Console program).

move
   :index:`\ <single: Console; Command; move>`\  The move command allows to move volumes between slots in an autochanger without having to leave the bconsole.

   To move a volume from slot 32 to slots 33, use:

   .. code-block:: bconsole
      :caption: move

      *<input>move storage=TandbergT40 srcslots=32 dstslots=33</input>
      Connecting to Storage daemon TandbergT40 at bareos:9103 ...
      3306 Issuing autochanger "slots" command.
      Device "Drive-1" has 39 slots.
      Connecting to Storage daemon TandbergT40 at bareos:9103 ...
      3306 Issuing autochanger "listall" command.
      Connecting to Storage daemon TandbergT40 at bareos:9103 ...
      3306 Issuing autochanger transfer command.
      3308 Successfully transfered volume from slot 32 to 33.


.. _ManualPruning:

prune
   :index:`\ <single: Console; Command; prune>`

   The Prune command allows you to safely remove expired database records from Jobs, Volumes and Statistics. This command works only on the Catalog database and does not affect data written to Volumes. In all cases, the Prune command applies a retention period to the specified records. You can Prune expired File entries from Job records; you can Prune expired Job records from the database, and you can Prune
   both expired Job and File records from specified Volumes.

   .. code-block:: bconsole
      :caption: prune

      prune files [client=<client>] [pool=<pool>] [yes] |
            jobs [client=<client>] [pool=<pool>] [jobtype=<jobtype>] [yes] |
            volume [=volume] [pool=<pool>] [yes] |
            stats [yes]

   For a Volume to be pruned, the volume status must be **Full**, **Used** or **Append** otherwise the pruning will not take place.


.. _bcommandPurge:

purge
   :index:`\ <single: Console; Command; purge>`

   The Purge command will delete associated catalog database records from Jobs and Volumes without considering the retention period. This command can be dangerous because you can delete catalog records associated with current backups of files, and we recommend that you do not use it unless you know what you are doing. The permitted forms of :bcommand:`purge` are:

   .. code-block:: bconsole
      :caption: purge

      purge [files [job=<job> | jobid=<jobid> | client=<client> | volume=<volume>]] |
            [jobs [client=<client> | volume=<volume>]] |
            [volume [=<volume>] [storage=<storage>] [pool=<pool>] [devicetype=<type>] [drive=<drivenum>] [action=<action>]] |
            [quota [client=<client>]]

   For the :bcommand:`purge` command to work on volume catalog database records the volume status must be **Append**, **Full**, **Used** or **Error**.

   The actual data written to the Volume will be unaffected by this command unless you are using the :config:option:`dir/pool/ActionOnPurge = Truncate`\  option.

   To ask Bareos to truncate your **Purged** volumes, you need to use the following command in interactive mode:

   .. code-block:: bconsole
      :caption: purge example

      *<input>purge volume action=truncate storage=File pool=Full</input>

   However, normally you should use the :bcommand:`purge` command only to purge a volume from the catalog and use the :bcommand:`truncate` command to truncate the volume on the |sd|.


resolve
   :index:`\ <single: Console; Command; resolve>`\  In the configuration files, Addresses can (and normally should) be specified as DNS names. As the different components of Bareos will establish network connections to other Bareos components, it is important that DNS name resolution works on involved components and delivers the same results. The :bcommand:`resolve` command can be used to test DNS resolution of a given hostname on director, storage daemon or client.

   .. code-block:: bconsole
      :caption: resolve example

      *<input>resolve www.bareos.com</input>
      bareos-dir resolves www.bareos.com to host[ipv4:84.44.166.242]

      *<input>resolve client=client1-fd www.bareos.com</input>
      client1-fd resolves www.bareos.com to host[ipv4:84.44.166.242]

      *<input>resolve storage=File www.bareos.com</input>
      bareos-sd resolves www.bareos.com to host[ipv4:84.44.166.242]


.. _section-bcommandQuery:

query
   :index:`\ <single: Console; Command; query>`
   This command reads a predefined SQL query from the query file (the name and location of the query file is defined with the QueryFile resource record in the Director’s configuration file). You are prompted to select a query from the file, and possibly enter one or more parameters, then the command is submitted to the Catalog database SQL engine.

quit
   :index:`\ <single: quit>`\  This command terminates the console program. The console program sends the quit request to the Director and waits for acknowledgment. If the Director is busy doing a previous command for you that has not terminated, it may take some time. You may quit immediately by issuing the .quit command (i.e. quit preceded by a period).

relabel
   :index:`\ <single: Console; Command; relabel>`\  This command is used to label physical volumes.

   The full form of this command is:

   .. code-block:: bconsole
      :caption: relabel

      relabel storage=<storage-name> oldvolume=<old-volume-name> volume=<new-volume-name> pool=<pool-name> [encrypt]

   If you leave out any part, you will be prompted for it. In order for the Volume (old-volume-name) to be relabeled, it must be in the catalog, and the volume status must be marked **Purged** or **Recycle**. This happens automatically as a result of applying retention periods or you may explicitly purge the volume using the :bcommand:`purge` command.

   Once the volume is physically relabeled, the old data previously written on the Volume is lost and cannot be recovered.

release
   :index:`\ <single: Console; Command; release>`\  This command is used to cause the Storage daemon to release (and rewind) the current tape in the drive, and to re-read the Volume label the next time the tape is used.

   .. code-block:: bconsole
      :caption: release

      release storage=<storage-name>

   After a release command, the device is still kept open by Bareos (unless :config:option:`sd/device/AlwaysOpen = no`\ ) so it cannot be used by another program. However, with some tape drives, the operator can remove the current tape and to insert a different one, and when the next Job starts, Bareos will know to re-read the tape label to find out what tape is mounted. If you want to be able to use the drive with another program (e.g. :command:`mt`), you
   must use the :bcommand:`unmount` command to cause Bareos to completely release (close) the device.

reload
   :index:`\ <single: Console; Command; reload>`\  The reload command causes the Director to re-read its configuration file and apply the new values. The new values will take effect immediately for all new jobs. However, if you change schedules, be aware that the scheduler pre-schedules jobs up to two hours in advance, so any changes that are to take place during the next two hours may be delayed. Jobs that have already been scheduled to run (i.e. surpassed their requested start time) will
   continue with the old values. New jobs will use the new values. Each time you issue a reload command while jobs are running, the prior config values will queued until all jobs that were running before issuing the reload terminate, at which time the old config values will be released from memory. The Directory permits keeping up to ten prior set of configurations before it will refuse a reload command. Once at least one old set of config values has been released it will again accept new reload
   commands.

   While it is possible to reload the Director’s configuration on the fly, even while jobs are executing, this is a complex operation and not without side effects. Accordingly, if you have to reload the Director’s configuration while Bareos is running, it is advisable to restart the Director at the next convenient opportunity.

rerun
   :index:`\ <single: Console; Command; rerun>`\  The rerun command allows you to re-run a Job with exactly the same setting as the original Job. In Bareos, the job configuration is often altered by job overrides. These overrides alter the configuration of the job just for one job run. If because of any reason, a job with overrides fails, it is not easy to restart a new job that is exactly configured as the job that failed. The whole job configuration is automatically set to the defaults
   and it is hard to configure everything like it was.

   By using the rerun command, it is much easier to rerun a job exactly as it was configured. You only have to specify the JobId of the failed job.

   .. code-block:: bconsole
      :caption: rerun

      rerun jobid=<jobid> since_jobid=<jobid> days=<nr_days> hours=<nr_hours> yes

   You can select the jobid(s) to rerun by using one of the selection criteria. Using jobid= will automatically select all jobs failed after and including the given jobid for rerunning. By using days= or hours=, you can select all failed jobids in the last number of days or number of hours respectively for rerunning.


.. _bcommandRestore:

restore
   :index:`\ <single: Restore>`
   :index:`\ <single: Console; Command; restore>`
   :index:`\ <single: Console; File Selection>`

   The restore command allows you to select one or more Jobs (JobIds) to be restored using various methods. Once the JobIds are selected, the File records for those Jobs are placed in an internal Bareos directory tree, and the restore enters a file selection mode that allows you to interactively walk up and down the
   file tree selecting individual files to be restored. This mode is somewhat similar to the standard Unix restore program’s interactive file selection mode.

   .. code-block:: bconsole
      :caption: restore

      restore storage=<storage-name> client=<backup-client-name>
        where=<path> pool=<pool-name> fileset=<fileset-name>
        restoreclient=<restore-client-name>
        restorejob=<job-name>
        select current all done

   Where current, if specified, tells the restore command to automatically select a restore to the most current backup. If not specified, you will be prompted. The all specification tells the restore command to restore all files. If it is not specified, you will be prompted for the files to restore. For details of the restore command, please see the :ref:`Restore Chapter <RestoreChapter>` of this manual.

   The client keyword initially specifies the client from which the backup was made and the client to which the restore will be make. However, if the restoreclient keyword is specified, then the restore is written to that client.

   The restore job rarely needs to be specified, as bareos installations commonly only have a single restore job configured. However, for certain cases, such as a varying list of RunScript specifications, multiple restore jobs may be configured. The restorejob argument allows the selection of one of these jobs.

   For more details, see the :ref:`Restore chapter <RestoreChapter>`.

run
   :index:`\ <single: Console; Command; run>`
   This command allows you to schedule jobs to be run immediately.

   The full form of the command is:

   .. code-block:: bconsole
      :caption: run

      run job=<job-name> client=<client-name> fileset=<fileset-name>
         level=<level> storage=<storage-name> where=<directory-prefix>
         when=<universal-time-specification> pool=<pool-name>
         pluginoptions=<plugin-options-string> accurate=<yes|no>
         comment=<text> spooldata=<yes|no> priority=<number>
         jobid=<jobid> catalog=<catalog> migrationjob=<job-name> backupclient=<client-name>
         backupformat=<format> nextpool=<pool-name> since=<universal-time-specification>
         verifyjob=<job-name> verifylist=<verify-list> migrationjob=<complete_name>
         yes

   Any information that is needed but not specified will be listed for selection, and before starting the job, you will be prompted to accept, reject, or modify the parameters of the job to be run, unless you have specified yes, in which case the job will be immediately sent to the scheduler.

   If you wish to start a job at a later time, you can do so by setting the When time. Use the mod option and select When (no. 6). Then enter the desired start time in YYYY-MM-DD HH:MM:SS format.

   The spooldata argument of the run command cannot be modified through the menu and is only accessible by setting its value on the intial command line. If no spooldata flag is set, the job, storage or schedule flag is used.

setbandwidth
   :index:`\ <single: Console; Command; setbandwidth>`\  This command (:sinceVersion:`12.4.1: setbandwidth`) is used to limit the bandwidth of a running job or a client.

   .. code-block:: bconsole
      :caption: setbandwidth

      setbandwidth limit=<nb> [jobid=<id> | client=<cli>]


.. _bcommandSetdebug:

setdebug
   :index:`\ <single: Console; Command; setdebug>`
   :index:`\ <single: Debug; setdebug>`
   :index:`\ <single: Debug; Windows>`
   :index:`\ <single: Windows; Debug>`

   This command is used to set the debug level in each daemon. The form of this command is:

   .. code-block:: bconsole
      :caption: setdebug

      setdebug level=nnn [trace=0/1 client=<client-name> | dir | director | storage=<storage-name> | all]

   Each of the daemons normally has debug compiled into the program, but disabled. There are two ways to enable the debug output.

   One is to add the -d nnn option on the command line when starting the daemon. The nnn is the debug level, and generally anything between 50 and 200 is reasonable. The higher the number, the more output is produced. The output is written to standard output.

   The second way of getting debug output is to dynamically turn it on using the Console using the :command:`setdebug level=nnn` command. If none of the options are given, the command will prompt you. You can selectively turn on/off debugging in any or all the daemons (i.e. it is not necessary to specify all the components of the above command).

   If trace=1 is set, then tracing will be enabled, and the daemon will be placed in trace mode, which means that all debug output as set by the debug level will be directed to his trace file in the current directory of the daemon. When tracing, each debug output message is appended to the trace file. You must explicitly delete the file when you are done.

   .. code-block:: bconsole
      :caption: set Director debug level to 100 and get messages written to his trace file

      *<input>setdebug level=100 trace=1 dir</input>
      level=100 trace=1 hangup=0 timestamp=0 tracefilename=/var/lib/bareos/bareos-dir.example.com.trace

.. _bcommandSetdevice:

setdevice
   :index:`\ <single: Console; Command; setdevice>`
   :index:`\ <single: Debug; setdevice>`
   :index:`\ <single: Debug; Windows>`
   :index:`\ <single: Windows; Debug>`

   This command is used to set :config:option:`sd/device/AutoSelect` of a device resource in the |sd|. This command can be used to temporarily disable that a device is automatically selected in an autochanger.

   The setting is only valid until the next restart of the |sd|. The form of this command is:

   .. code-block:: bconsole
      :caption: setdevice

      setdevice storage=<storage-name> device=<device-name> autoselect=<bool>

   Note: Consider the settings of :config:option:`dir/console/CommandACL` and :config:option:`dir/console/StorageACL`.

.. _bcommandSetIP:

setip
   :index:`\ <single: Console; Command; setip>`

   Sets new client address – if authorized.

   A console is authorized to use the SetIP command only if it has a Console resource definition in both the Director and the Console. In addition, if the console name, provided on the Name = directive, must be the same as a Client name, the user of that console is permitted to use the SetIP command to change the Address directive in the Director’s client resource to the IP address of the Console. This permits portables or other machines using DHCP (non-fixed IP addresses) to "notify" the
   Director of their current IP address.

show
   :index:`\ <single: Console; Command; show>`

   The show command will list the Director’s resource records as defined in the Director’s configuration.
   :bcommand:`help show` will show you all available options.The following keywords are accepted on the show command line:

   .. code-block:: bconsole

      *<input>help show</input>
        Command            Description
        =======            ===========
        show               Show resource records

      Arguments:
              catalog=<catalog-name> |
              client=<client-name> |
              ...
              storages |
              disabled [ clients | jobs | schedules ] |
              all [verbose]

   :bcommand:`show all` will show you all available resources.
   The **verbose** argument will show you also all configuration directives with there default value:

   .. code-block:: bconsole

      *<input>show client=bareos-fd verbose</input>
      Client {
        Name = "bareos-fd"
        Description = "Client resource of the Director itself."
        Address = "localhost"
        #  Port = 9102
        Password = "*************************************"
        #  Catalog = "MyCatalog"
        #  Passive = no
        #  ConnectionFromDirectorToClient = yes
        #  ConnectionFromClientToDirector = no
        #  Enabled = yes
        ...
      }

   If you are not using the default console, but a named console,
   ACLs are applied.
   Additionally, if the named console don't have the permission
   to run the :bcommand:`configure` command,
   some resources (like consoles and profiles) are not shown at all.

   Please don’t confuse this command with the :bcommand:`list` command, which displays the contents of the catalog.


sqlquery
   :index:`\ <single: Console; Command; sqlquery>`\  The sqlquery command puts the Console program into SQL query mode where each line you enter is concatenated to the previous line until a semicolon (;) is seen. The semicolon terminates the command, which is then passed directly to the SQL database engine. When the output from the SQL engine is displayed, the formation of a new SQL command begins. To terminate SQL query mode and return to the Console command prompt, you enter a period (.)
   in column 1.

   Using this command, you can query the SQL catalog database directly. Note you should really know what you are doing otherwise you could damage the catalog database. See the query command below for simpler and safer way of entering SQL queries.

   Depending on what database engine you are using (MySQL, PostgreSQL or SQLite), you will have somewhat different SQL commands available. For more detailed information, please refer to the MySQL, PostgreSQL or SQLite documentation.

status
   :index:`\ <single: Console; Command; status>`\

   This command will display the status of all components. For the director, it will display the next jobs that are scheduled during the next 24 hours as well as the status of currently running jobs. For the Storage Daemon, you will have drive status or autochanger content. The File Daemon will give you information about current jobs like average speed or file accounting. The full form of this command is:

   .. code-block:: bconsole
      :caption: status

      status [all | dir=<dir-name> | director | scheduler | schedule=<schedule-name> |
              client=<client-name> | storage=<storage-name> slots | subscriptions | configuration]

   If you do a status dir, the console will list any currently running jobs, a summary of all jobs scheduled to be run in the next 24 hours, and a listing of the last ten terminated jobs with their statuses. The scheduled jobs summary will include the Volume name to be used. You should be aware of two things: 1. to obtain the volume name, the code goes through the same code that will be used when the job runs, but it does not do pruning nor recycling of Volumes; 2. The Volume listed is at best a
   guess. The Volume actually used may be different because of the time difference (more durations may expire when the job runs) and another job could completely fill the Volume requiring a new one.

   In the Running Jobs listing, you may find the following types of information:

   .. code-block:: bconsole

      2507 Catalog MatouVerify.2004-03-13_05.05.02 is waiting execution
      5349 Full    CatalogBackup.2004-03-13_01.10.00 is waiting for higher
                   priority jobs to finish
      5348 Differe Minou.2004-03-13_01.05.09 is waiting on max Storage jobs
      5343 Full    Rufus.2004-03-13_01.05.04 is running

   Looking at the above listing from bottom to top, obviously JobId 5343 (Rufus) is running. JobId 5348 (Minou) is waiting for JobId 5343 to finish because it is using the Storage resource, hence the "waiting on max Storage jobs". JobId 5349 has a lower priority than all the other jobs so it is waiting for higher priority jobs to finish, and finally, JobId 2507 (MatouVerify) is waiting because only one job can run at a time, hence it is simply "waiting execution"

   If you do a status dir, it will by default list the first occurrence of all jobs that are scheduled today and tomorrow. If you wish to see the jobs that are scheduled in the next three days (e.g. on Friday you want to see the first occurrence of what tapes are scheduled to be used on Friday, the weekend, and Monday), you can add the days=3 option. Note, a days=0 shows the first occurrence of jobs scheduled today only. If you have multiple run statements, the first occurrence of each run
   statement for the job will be displayed for the period specified.

   If your job seems to be blocked, you can get a general idea of the problem by doing a status dir, but you can most often get a much more specific indication of the problem by doing a status storage=xxx. For example, on an idle test system, when I do status storage=File, I get:

   .. code-block:: bconsole
      :caption: status storage

      *<input>status storage=File</input>
      Connecting to Storage daemon File at 192.168.68.112:8103

      rufus-sd Version: 1.39.6 (24 March 2006) i686-pc-linux-gnu redhat (Stentz)
      Daemon started 26-Mar-06 11:06, 0 Jobs run since started.

      Running Jobs:
      No Jobs running.
      ====

      Jobs waiting to reserve a drive:
      ====

      Terminated Jobs:
       JobId  Level   Files          Bytes Status   Finished        Name
      ======================================================================
          59  Full        234      4,417,599 OK       15-Jan-06 11:54 usersave
      ====

      Device status:
      Autochanger "DDS-4-changer" with devices:
         "DDS-4" (/dev/nst0)
      Device "DDS-4" (/dev/nst0) is mounted with Volume="TestVolume002"
      Pool="*unknown*"
          Slot 2 is loaded in drive 0.
          Total Bytes Read=0 Blocks Read=0 Bytes/block=0
          Positioned at File=0 Block=0

      Device "File" (/tmp) is not open.
      ====

      In Use Volume status:
      ====

   Now, what this tells me is that no jobs are running and that none of the devices are in use. Now, if I unmount the autochanger, which will not be used in this example, and then start a Job that uses the File device, the job will block. When I re-issue the status storage command, I get for the Device status:

   .. code-block:: bconsole
      :caption: status storage

      *<input>status storage=File</input>
      ...
      Device status:
      Autochanger "DDS-4-changer" with devices:
         "DDS-4" (/dev/nst0)
      Device "DDS-4" (/dev/nst0) is not open.
          Device is BLOCKED. User unmounted.
          Drive 0 is not loaded.

      Device "File" (/tmp) is not open.
          Device is BLOCKED waiting for media.
      ====
      ...

   Now, here it should be clear that if a job were running that wanted to use the Autochanger (with two devices), it would block because the user unmounted the device. The real problem for the Job I started using the "File" device is that the device is blocked waiting for media – that is Bareos needs you to label a Volume.

   The command :bcommand:`status scheduler` (:sinceVersion:`12.4.4: status scheduler`) can be used to check when a certain schedule will trigger. This gives more information than :bcommand:`status director`.

   Called without parameters, :bcommand:`status scheduler` shows a preview for all schedules for the next 14 days. It first shows a list of the known schedules and the jobs that will be triggered by these jobs, and next, a table with date (including weekday), schedule name and applied overrides is displayed:

   .. code-block:: bconsole
      :caption: status scheduler

      *<input>status scheduler</input>
      Scheduler Jobs:

      Schedule               Jobs Triggered
      ===========================================================
      WeeklyCycle
                             BackupClient1

      WeeklyCycleAfterBackup
                             BackupCatalog

      ====

      Scheduler Preview for 14 days:

      Date                  Schedule                Overrides
      ==============================================================
      Di 04-Jun-2013 21:00  WeeklyCycle             Level=Incremental
      Di 04-Jun-2013 21:10  WeeklyCycleAfterBackup  Level=Full
      Mi 05-Jun-2013 21:00  WeeklyCycle             Level=Incremental
      Mi 05-Jun-2013 21:10  WeeklyCycleAfterBackup  Level=Full
      Do 06-Jun-2013 21:00  WeeklyCycle             Level=Incremental
      Do 06-Jun-2013 21:10  WeeklyCycleAfterBackup  Level=Full
      Fr 07-Jun-2013 21:00  WeeklyCycle             Level=Incremental
      Fr 07-Jun-2013 21:10  WeeklyCycleAfterBackup  Level=Full
      Sa 08-Jun-2013 21:00  WeeklyCycle             Level=Differential
      Mo 10-Jun-2013 21:00  WeeklyCycle             Level=Incremental
      Mo 10-Jun-2013 21:10  WeeklyCycleAfterBackup  Level=Full
      Di 11-Jun-2013 21:00  WeeklyCycle             Level=Incremental
      Di 11-Jun-2013 21:10  WeeklyCycleAfterBackup  Level=Full
      Mi 12-Jun-2013 21:00  WeeklyCycle             Level=Incremental
      Mi 12-Jun-2013 21:10  WeeklyCycleAfterBackup  Level=Full
      Do 13-Jun-2013 21:00  WeeklyCycle             Level=Incremental
      Do 13-Jun-2013 21:10  WeeklyCycleAfterBackup  Level=Full
      Fr 14-Jun-2013 21:00  WeeklyCycle             Level=Incremental
      Fr 14-Jun-2013 21:10  WeeklyCycleAfterBackup  Level=Full
      Sa 15-Jun-2013 21:00  WeeklyCycle             Level=Differential
      Mo 17-Jun-2013 21:00  WeeklyCycle             Level=Incremental
      Mo 17-Jun-2013 21:10  WeeklyCycleAfterBackup  Level=Full
      ====

   :bcommand:`status scheduler` accepts the following parameters:

   client=clientname
      shows only the schedules that affect the given client.

   job=jobname
      shows only the schedules that affect the given job.

   schedule=schedulename
      shows only the given schedule.

   days=number
      of days shows only the number of days in the scheduler preview. Positive numbers show the future, negative numbers show the past. days can be combined with the other selection criteria. days= can be combined with the other selection criteria.

   In case you are running a maintained version of Bareos, the command :bcommand:`status subscriptions` (:sinceVersion:`12.4.4: status subscriptions`) can help you to keep the overview over the subscriptions that are used.

   To enable this functionality, just add the configuration :config:option:`dir/director/Subscriptions`\  directive and specify the number of subscribed clients, for example:

   .. code-block:: bareosconfig
      :caption: enable subscription check

      Director {
         ...
         Subscriptions = 50
      }

   Using the console command :bcommand:`status subscriptions`, the status of the subscriptions can be checked any time interactively:

   .. code-block:: bconsole
      :caption: status subscriptions

      *<input>status subscriptions</input>
      Ok: available subscriptions: 8 (42/50) (used/total)

   Also, the number of subscriptions is checked after every job. If the number of clients is bigger than the configured limit, a Job warning is created a message like this:

   .. code-block:: bconsole
      :caption: subscriptions warning

      JobId 7: Warning: Subscriptions exceeded: (used/total) (51/50)

   Please note: Nothing else than the warning is issued, no enforcement on backup, restore or any other operation will happen.

   Setting the value for :config:option:`dir/director/Subscriptions = 0`\  disables this functionality:

   .. code-block:: bareosconfig
      :caption: disable subscription check

      Director {
         ...
         Subscriptions = 0
      }

   Not configuring the directive at all also disables it, as the default value for the Subscriptions directive is zero.


   Using the console command :bcommand:`status configuration` will show a list of deprecated configuration settings that were detected when loading the director's configuration. Be sure to enable access to the "configuration" command by using the according command ACL.

time
   :index:`\ <single: Console; Command; time>`\  The time command shows the current date, time and weekday.

trace
   :index:`\ <single: Console; Command; trace>`\  Turn on/off trace to file.

truncate
   :index:`\ <single: Console; Command; truncate>`\  :index:`\ <single: Disk; Freeing disk space>`\  :index:`\ <single: Disk; Freeing disk space>`\

.. _bcommandTruncate:



   If the status of a volume is **Purged**, it normally still contains data, even so it can not easily be accessed.

   .. code-block:: bconsole
      :caption: truncate

      truncate volstatus=Purged [storage=<storage>] [pool=<pool>]
               [volume=<volume>] [drive=<drivenum>] [yes]

   When using a disk volume (and other volume types also) the volume file still resides on the |sd|. If you want to reclaim disk space, you can use the :bcommand:`truncate volstatus=Purged` command. When used on a volume, it rewrites the header and by this frees the rest of the disk space.
   By default the first drive (number 0) of an autochanger is used. By specifying `drive=<drivenum>`, a different drive can be selected.

   If the volume you want to get rid of has not the **Purged** status, you first have to use the :bcommand:`prune volume` or even the :bcommand:`purge volume` command to free the volume from all remaining jobs.

   This command is available since Bareos :sinceVersion:`16.2.5: truncate command`.
   The *drive=<drivenum>* selection was added in  :sinceVersion:`20.0.2: truncate command drive selection`.

umount
   :index:`\ <single: Console; Command; umount>`\  Alias for :bcommand:`unmount`.

unmount
   :index:`\ <single: Console; Command; unmount>`\  This command causes the indicated Bareos Storage daemon to unmount the specified device. The forms of the command are the same as the mount command:

   .. code-block:: bconsole
      :caption: unmount

      unmount storage=<storage-name> [drive=<num>]
      unmount [jobid=<id> | job=<job-name>]

   Once you unmount a storage device, Bareos will no longer be able to use it until you issue a mount command for that device. If Bareos needs to access that device, it will block and issue mount requests periodically to the operator.

   If the device you are unmounting is an autochanger, it will unload the drive you have specified on the command line. If no drive is specified, it will assume drive 1.

   In most cases, it is preferable to use the :bcommand:`release` instead.


.. _UpdateCommand:

update
   :index:`\ <single: Console; Command; update>`

   This command will update the catalog for either a specific Pool record, a Volume record, or the Slots in an autochanger with barcode capability. In the case of updating a Pool record, the new information will be automatically taken from the corresponding Director’s configuration resource record. It can be used to increase the maximum number of volumes permitted or to set a maximum number of volumes. The
   following main keywords may be specified:

   -  volume

   -  pool

   -  slots

   -  iobid

   -  stats

   In the case of updating a Volume (:bcommand:`update volume`), you will be prompted for which value you wish to change. The following Volume parameters may be changed:

   ::

         Volume Status
         Volume Retention Period
         Volume Use Duration
         Maximum Volume Jobs
         Maximum Volume Files
         Maximum Volume Bytes
         Recycle Flag
         Recycle Pool
         Slot
         InChanger Flag
         Pool
         Volume Files
         Volume from Pool
         All Volumes from Pool
         All Volumes from all Pools



   For slots :bcommand:`update slots`, Bareos will obtain a list of slots and their barcodes from the Storage daemon, and for each barcode found, it will automatically update the slot in the catalog Media record to correspond to the new value. This is very useful if you have moved cassettes in the magazine, or if you have removed the magazine and inserted a different one. As the slot of each Volume is updated, the InChanger flag for that Volume will also be set, and any other
   Volumes in the Pool that were last mounted on the same Storage device will have their InChanger flag turned off. This permits Bareos to know what magazine (tape holder) is currently in the autochanger.

   If you do not have barcodes, you can accomplish the same thing by using the :bcommand:`update slots scan` command. The :strong:`scan` keyword tells Bareos to physically mount each tape and to read its VolumeName.

   For Pool :bcommand:`update pool`, Bareos will move the Volume record from its existing pool to the pool specified.

   For Volume from Pool, All Volumes from Pool and All Volumes from all Pools, the following values are updated from the Pool record: Recycle, RecyclePool, VolRetention, VolUseDuration, MaxVolJobs, MaxVolFiles, and MaxVolBytes.

   For updating the statistics, use :bcommand:`updates stats`, see :ref:`section-JobStatistics`.

   The full form of the update command with all command line arguments is:

   .. code-block:: bconsole
      :caption: update

      update  volume=<volume-name> [volstatus=<status>]
              [volretention=<time-def>] [pool=<pool-name>]
              [recycle=<yes/no>] [slot=<number>] [inchanger=<yes/no>] |
              pool=<pool-name> [maxvolbytes=<size>] [maxvolfiles=<nb>]
              [maxvoljobs=<nb>][enabled=<yes/no>] [recyclepool=<pool-name>]
              [actiononpurge=<action>] |
              slots [storage=<storage-name>] [scan] |
              jobid=<jobid> [jobname=<name>] [starttime=<time-def>]
              [client=<client-name>] [filesetid=<fileset-id>]
              [jobtype=<job-type>] |
              stats [days=<number>]

use
   :index:`\ <single: Console; Command; use>`
   This command allows you to specify which Catalog database to use. Normally, you will be using only one database so this will be done automatically. In the case that you are using more than one database, you can use this command to switch from one to another.

   .. code-block:: bconsole
      :caption: use

      use [catalog=<catalog>]

.. _var:

var
   :index:`\ <single: Console; Command; var>`
   This command takes a string or quoted string and does variable expansion on it mostly the same way variable expansion is done on the :config:option:`dir/pool/LabelFormat`\  string. The difference between the :bcommand:`var` command and the actual :config:option:`dir/pool/LabelFormat`\  process is that during the var command, no job is running so dummy values are
   used in place of Job specific variables.

version
   :index:`\ <single: Console; Command; version>`
   The command prints the Director’s version.

wait
   :index:`\ <single: Console; Command; wait>`\  The wait command causes the Director to pause until there are no jobs running. This command is useful when you wish to start a job and wait until that job completes before continuing. This command now has the following options:

   .. code-block:: bconsole
      :caption: wait

      wait [jobid=<jobid>] [jobuid=<unique id>] [job=<job name>]

   If specified with a specific JobId, ... the wait command will wait for that particular job to terminate before continuing.

whoami
   :index:`\ <single: Console; Command; whoami>`
   Print the name of the user associated with this console.


.. _dotcommands:

Special dot (.) Commands
~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Console; Command; . commands>`\

There is a list of commands that are prefixed with a period (.). These commands are intended to be used either by batch programs or graphical user interface front-ends. They are not normally used by interactive users. For details, see `Bareos Developer Guide (dot-commands) <../DeveloperGuide/api.html#dot-commands>`_.

.. _atcommands:

Special At (@) Commands
~~~~~~~~~~~~~~~~~~~~~~~

Normally, all commands entered to the Console program are immediately forwarded to the Director, which may be on another machine, to be executed. However, there is a small list of at commands, all beginning with an at character (@), that will not be sent to the Director, but rather interpreted by the Console program directly. Note, these commands are implemented only in the TTY console program and not in the Bat Console. These commands are:

@input <filename>
   :index:`\ <single: Console; Command; @input <filename>>`\  Read and execute the commands contained in the file specified.

@output <filename> <w|a>
   :index:`\ <single: Console; Command; @output <filename> <w|a>>`\  Send all following output to the filename specified either overwriting the file (w) or appending to the file (a). To redirect the output to the terminal, simply enter @output without a filename specification. WARNING: be careful not to overwrite a valid file. A typical example might be:



   ::

          @output /dev/null
          commands ...
          @output



@tee <filename> <w|a>
   :index:`\ <single: Console; Command; @tee <filename> <w|a>>`\  Send all subsequent output to both the specified file and the terminal. It is turned off by specifying @tee or @output without a filename.

@sleep <seconds>
   :index:`\ <single: Console; Command; @sleep <seconds>>`\  Sleep the specified number of seconds.

@time
   :index:`\ <single: Console; Command; @time>`\  Print the current time and date.

@version
   :index:`\ <single: Console; Command; @version>`\  Print the console’s version.

@quit
   :index:`\ <single: Console; Command; @quit>`\  quit

@exit
   :index:`\ <single: Console; Command; @exit>`\  quit

@# anything
   :index:`\ <single: Console; Command; @# anything>`\  Comment

@help
   :index:`\ <single: Console; Command; @help>`\  Get the list of every special @ commands.

@separator <char>
   :index:`\ <single: Console; Command; @separator>`\  When using bconsole with readline, you can set the command separator to one of those characters to write commands who require multiple input on one line, or to put multiple commands on a single line.

   ::

        !$%&'()*+,-/:;<>?[]^`{|}~

   Note, if you use a semicolon (;) as a separator character, which is common, you will not be able to use the sql command, which requires each command to be terminated by a semicolon.

Adding Volumes to a Pool
------------------------

:index:`\ <single: Console; Adding a Volume to a Pool>`\

.. TODO: move to another chapter

If you have used the label command to label a Volume, it will be automatically added to the Pool, and you will not need to add any media to the pool.

Alternatively, you may choose to add a number of Volumes to the pool without labeling them. At a later time when the Volume is requested by Bareos you will need to label it.

Before adding a volume, you must know the following information:

#. The name of the Pool (normally "Default")

#. The Media Type as specified in the Storage Resource in the Director’s configuration file (e.g. "DLT8000")

#. The number and names of the Volumes you wish to create.

For example, to add media to a Pool, you would issue the following commands to the console program:



::

   *add
   Enter name of Pool to add Volumes to: Default
   Enter the Media Type: DLT8000
   Enter number of Media volumes to create. Max=1000: 10
   Enter base volume name: Save
   Enter the starting number: 1
   10 Volumes created in pool Default
   *



To see what you have added, enter:



::

   *list media pool=Default
   +-------+----------+---------+---------+-------+------------------+
   | MedId | VolumeNa | MediaTyp| VolStat | Bytes | LastWritten      |
   +-------+----------+---------+---------+-------+------------------+
   |    11 | Save0001 | DLT8000 | Append  |     0 | 0000-00-00 00:00 |
   |    12 | Save0002 | DLT8000 | Append  |     0 | 0000-00-00 00:00 |
   |    13 | Save0003 | DLT8000 | Append  |     0 | 0000-00-00 00:00 |
   |    14 | Save0004 | DLT8000 | Append  |     0 | 0000-00-00 00:00 |
   |    15 | Save0005 | DLT8000 | Append  |     0 | 0000-00-00 00:00 |
   |    16 | Save0006 | DLT8000 | Append  |     0 | 0000-00-00 00:00 |
   |    17 | Save0007 | DLT8000 | Append  |     0 | 0000-00-00 00:00 |
   |    18 | Save0008 | DLT8000 | Append  |     0 | 0000-00-00 00:00 |
   |    19 | Save0009 | DLT8000 | Append  |     0 | 0000-00-00 00:00 |
   |    20 | Save0010 | DLT8000 | Append  |     0 | 0000-00-00 00:00 |
   +-------+----------+---------+---------+-------+------------------+
   *



Notice that the console program automatically appended a number to the base Volume name that you specify (Save in this case). If you don’t want it to append a number, you can simply answer 0 (zero) to the question "Enter number of Media volumes to create. Max=1000:", and in this case, it will create a single Volume with the exact name you specify.
