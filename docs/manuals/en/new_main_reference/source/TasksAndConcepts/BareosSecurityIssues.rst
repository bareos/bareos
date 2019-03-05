.. _SecurityChapter:

Bareos Security Issues
======================

:index:`[TAG=Security] <single: Security>`

-  Security means being able to restore your files, so read the :ref:`Critical Items Chapter <Critical>` of this manual.

-  The clients (bareos-fd) must run as root to be able to access all the system files.

-  It is not necessary to run the Director as root.

-  It is not necessary to run the Storage daemon as root, but you must ensure that it can open the tape drives, which are often restricted to root access by default. In addition, if you do not run the Storage daemon as root, it will not be able to automatically set your tape drive parameters on most OSes since these functions, unfortunately require root access.

-  You should restrict access to the Bareos configuration files, so that the passwords are not world-readable. The Bareos daemons are password protected using CRAM-MD5 (i.e. the password is not sent across the network). This will ensure that not everyone can access the daemons. It is a reasonably good protection, but can be cracked by experts.

-  If you are using the recommended ports 9101, 9102, and 9103, you will probably want to protect these ports from external access using a firewall and/or using tcp wrappers (etc/hosts.allow).

-  By default, all data that is sent across the network is unencrypted. However, Bareos does support TLS (transport layer security) and can encrypt transmitted data. Please read the :ref:`TLS (SSL) Communications Encryption <CommEncryption>` section of this manual.

-  You should ensure that the Bareos working directories are readable and writable only by the Bareos daemons.

-  The default Bareos :command:`grant_bareos_privileges` script grants all permissions to use the MySQL (and PostgreSQL) database without a password. If you want security, please tighten this up!

-  Don’t forget that Bareos is a network program, so anyone anywhere on the network with the console program and the Director’s password can access Bareos and the backed up data.

-  You can restrict what IP addresses Bareos will bind to by using the appropriate DirAddress, FDAddress, or SDAddress records in the respective daemon configuration files.

.. _wrappers:

Configuring and Testing TCP Wrappers
------------------------------------

:index:`[TAG=TCP Wrappers] <single: TCP Wrappers>` :index:`[TAG=Wrappers->TCP] <pair: Wrappers; TCP>` :index:`[TAG=libwrappers] <single: libwrappers>`

The TCP wrapper functionality is available on different platforms. Be default, it is activated on Bareos for Linux. With this enabled, you may control who may access your daemons. This control is done by modifying the file: /etc/hosts.allow. The program name that Bareos uses when applying these access restrictions is the name you specify in the daemon configuration file (see below for examples). You must not use the twist option in your /etc/hosts.allow or it will terminate the Bareos daemon
when a connection is refused.





.. _section-SecureEraseCommand:

Secure Erase Command
--------------------

From https://en.wikipedia.org/w/index.php?title=Data_erasure&oldid=675388437:

   Strict industry standards and government regulations are in place that force organizations to mitigate the risk of unauthorized exposure of confidential corporate and government data. Regulations in the United States include HIPAA (Health Insurance Portability and Accountability Act); FACTA (The Fair and Accurate Credit Transactions Act of 2003); GLB (Gramm-Leach Bliley); Sarbanes-Oxley Act (SOx); and Payment Card Industry Data Security Standards (PCI DSS) and the Data Protection Act in the
   United Kingdom. Failure to comply can result in fines and damage to company reputation, as well as civil and criminal liability.

Bareos supports the secure erase of files that usually are simply deleted. Bareos uses an external command to do the secure erase itself.

This makes it easy to choose a tool that meets the secure erase requirements.

To configure this functionality, a new configuration directive with the name :strong:`Secure Erase Command` has been introduced.

This directive is optional and can be configured in:

-  

   :config:option:`dir/director/SecureEraseCommand`\ 

-  

   :config:option:`sd/storage/SecureEraseCommand`\ 

-  

   :config:option:`fd/client/SecureEraseCommand`\ 

This directive configures the secure erase command globally for the daemon it was configured in.

If set, the secure erase command is used to delete files instead of the normal delete routine.

If files are securely erased during a job, the secure delete command output will be shown in the job log.

.. code-block:: sh
   :caption: bareos.log

   08-Sep 12:58 win-fd JobId 10: secure_erase: executing C:/cygwin64/bin/shred.exe "C:/temp/bareos-restores/C/Program Files/Bareos/Plugins/bareos_fd_consts.py"
   08-Sep 12:58 win-fd JobId 10: secure_erase: executing C:/cygwin64/bin/shred.exe "C:/temp/bareos-restores/C/Program Files/Bareos/Plugins/bareos_sd_consts.py"
   08-Sep 12:58 win-fd JobId 10: secure_erase: executing C:/cygwin64/bin/shred.exe "C:/temp/bareos-restores/C/Program Files/Bareos/Plugins/bpipe-fd.dll"

The current status of the secure erase command is also shown in the output of status director, status client and status storage.

If the secure erase command is configured, the current value is printed.

Example:

.. code-block:: sh

   * <input>status dir</input>
   backup1.example.com-dir Version: 15.3.0 (24 August 2015) x86_64-suse-linux-gnu suse openSUSE 13.2 (Harlequin) (x86_64)
   Daemon started 08-Sep-15 12:50. Jobs: run=0, running=0 mode=0 db=sqlite3
    Heap: heap=290,816 smbytes=89,166 max_bytes=89,166 bufs=334 max_bufs=335
    secure erase command='/usr/bin/wipe -V'

Example for Secure Erase Command Settings:

Linux:
   :strong:`Secure Erase Command = "/usr/bin/wipe -V"`

Windows:
   :strong:`Secure Erase Command = "C:/cygwin64/bin/shred.exe"`

Our tests with the :command:`sdelete` command was not successful, as :command:`sdelete` seems to stay active in the background. 

\appendix

