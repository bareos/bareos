.. _basejobs:

File Deduplication using Base Jobs
==================================

:index:`\ <single: Base Jobs>`\  :index:`\ <single: File Deduplication>`\ 

A base job is sort of like a Full save except that you will want the FileSet to contain only files that are unlikely to change in the future (i.e. a snapshot of most of your system after installing it). After the base job has been run, when you are doing a Full save, you specify one or more Base jobs to be used. All files that have been backed up in the Base job/jobs but not modified will then be excluded from the backup. During a restore, the Base jobs will be automatically pulled in where
necessary.

Imagine having 100 nearly identical Windows or Linux machine containing the OS and user files. Now for the OS part, a Base job will be backed up once, and rather than making 100 copies of the OS, there will be only one. If one or more of the systems have some files updated, no problem, they will be automatically backuped.

A new Job directive :strong:`Base=JobX,JobY,...`\  permits to specify the list of files that will be used during Full backup as base.

::

   Job {
      Name = BackupLinux
      Level= Base
      ...
   }

   Job {
      Name = BackupZog4
      Base = BackupZog4, BackupLinux
      Accurate = yes
      ...
   }

In this example, the job ``BackupZog4`` will use the most recent version of all files contained in ``BackupZog4`` and ``BackupLinux`` jobs. Base jobs should have run with :strong:`Level=Base`\  to be used.

By default, Bareos will compare permissions bits, user and group fields, modification time, size and the checksum of the file to choose between the current backup and the BaseJob file list. You can change this behavior with the ``BaseJob`` FileSet option. This option works like the :strong:`Verify`\ , that is described in the :ref:`FileSet <DirectorResourceFileSet>` chapter.

::

   FileSet {
     Name = Full
     Include = {
       Options {
          BaseJob  = pmugcs5
          Accurate = mcs
          Verify   = pin5
       }
       File = /
     }
   }



   .. warning::

      The current implementation doesn't permit to scan
   volume with :command:`bscan`. The result wouldn't permit to restore files easily.


