.. _CriticalChapter:

Critical Items to Implement Before Production
=============================================

:index:`\ <single: Production; Critical Items to Implement Before>` :index:`\ <single: Critical Items to Implement Before Production>`

We recommend you take your time before implementing a production on a Bareos backup system since Bareos is a rather complex program, and if you make a mistake, you may suddenly find that you cannot restore your files in case of a disaster. This is especially true if you have not previously used a major backup product.

If you follow the instructions in this chapter, you will have covered most of the major problems that can occur. It goes without saying that if you ever find that we have left out an important point, please inform us, so that we can document it to the benefit of everyone.



.. _Critical:



Critical Items
--------------

:index:`\ <single: Critical Items>`

The following assumes that you have installed Bareos, you more or less understand it, you have at least worked through the tutorial or have equivalent experience, and that you have set up a basic production configuration. If you haven’t done the above, please do so and then come back here. The following is a sort of checklist that points with perhaps a brief explanation of why you should do it. In most cases, you will find the details elsewhere in the manual. The order is more or less the order
you would use in setting up a production system (if you already are in production, use the checklist anyway).

-  Test your tape drive for compatibility with Bareos by using the test command of the :ref:`btape <btape>` program.

-  Test the end of tape handling of your tape drive by using the fill command in the :ref:`btape <btape>` program.

-  Do at least one restore of files. If you backup multiple OS types (Linux, Solaris, HP, MacOS, FreeBSD, Win32, ...), restore files from each system type. The :ref:`Restoring Files <RestoreChapter>` chapter shows you how.

-  Write a bootstrap file to a separate system for each backup job. See :config:option:`dir/job/WriteBootstrap`\  directive and more details are available in the :ref:`BootstrapChapter` chapter. Also, the default :file:`bareos-dir.conf` comes with a Write Bootstrap directive defined. This allows you to recover the state of your system as of the last backup.

-  Backup your catalog. An example of this is found in the default bareos-dir.conf file. The backup script is installed by default and should handle any database, though you may want to make your own local modifications. See also :ref:`Backing Up Your Bareos Database <BackingUpBareos>` for more information.

-  Write a bootstrap file for the catalog. An example of this is found in the default bareos-dir.conf file. This will allow you to quickly restore your catalog in the event it is wiped out – otherwise it is many excruciating hours of work.

-  Make a copy of the bareos-dir.conf, bareos-sd.conf, and bareos-fd.conf files that you are using on your server. Put it in a safe place (on another machine) as these files can be difficult to reconstruct if your server dies.

-  Bareos assumes all filenames are in UTF-8 format. This is important when saving the filenames to the catalog. For Win32 machine, Bareos will automatically convert from Unicode to UTF-8, but on Unix, Linux, \*BSD, and MacOS X machines, you must explicitly ensure that your locale is set properly. Typically this means that the LANG environment variable must end in .UTF-8. A full example is en_US.UTF-8. The exact syntax may vary a bit from OS to OS, and exactly how you define it will also vary.

   On most modern Win32 machines, you can edit the conf files with notepad and choose output encoding UTF-8.

Recommended Items
-----------------

:index:`\ <single: Recommended Items>`

Although these items may not be critical, they are recommended and will help you avoid problems.

-  Read the :ref:`QuickStartChapter` chapter

-  After installing and experimenting with Bareos, read and work carefully through the examples in the :ref:`TutorialChapter` chapter of this manual.

-  Learn what each of the :ref:`section-Utilities` does.

-  | Set up reasonable retention periods so that your catalog does not grow to be too big. See the following three chapters:
   | :ref:`RecyclingChapter`,
   | :ref:`DiskChapter`,
   | :ref:`PoolsChapter`.

If you absolutely must implement a system where you write a different tape each night and take it offsite in the morning. We recommend that you do several things:

-  Write a bootstrap file of your backed up data and a bootstrap file of your catalog backup to a external media like CDROM or USB stick, and take that with the tape. If this is not possible, try to write those files to another computer or offsite computer, or send them as email to a friend. If none of that is possible, at least print the bootstrap files and take that offsite with the tape. Having the bootstrap files will make recovery much easier.

-  It is better not to force Bareos to load a particular tape each day. Instead, let Bareos choose the tape. If you need to know what tape to mount, you can print a list of recycled and appendable tapes daily, and select any tape from that list. Bareos may propose a particular tape for use that it considers optimal, but it will accept any valid tape from the correct pool.


