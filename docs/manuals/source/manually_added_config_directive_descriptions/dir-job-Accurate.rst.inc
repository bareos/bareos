

.. _accuratemode:

 

.. _accurate:

 In accurate mode, the File daemon knowns exactly which files were present after the last backup. So it is able to handle deleted or renamed files.

When restoring a FileSet for a specified date (including "most recent"), Bareos is able to restore exactly the files and directories that existed at the time of the last backup prior to that date including ensuring that deleted files are actually deleted, and renamed directories are restored properly.

When doing :ref:`VirtualFull <VirtualFull>` backups, it is advised to use the accurate mode, otherwise the VirtualFull might contain already deleted files.

However, using the accurate mode has also disadvantages:

-  The File daemon must keep data concerning all files in memory. So If you do not have sufficient memory, the backup may either be terribly slow or fail. For 500.000 files (a typical desktop linux system), it will require approximately 64 Megabytes of RAM on your File daemon to hold the required information.

