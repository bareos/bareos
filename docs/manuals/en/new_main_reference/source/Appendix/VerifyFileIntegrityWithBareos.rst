.. _VerifyChapter:

Verify File Integrity with Bareos
=================================

:index:`\ <single: Security; Using Bareos to Improve Computer>` :index:`\ <single: Verify; File Integrity>`

Since Bareos maintains a catalog of files, their attributes, and either SHA1 or MD5 signatures, it can be an ideal tool for improving computer security. This is done by making a snapshot of your system files with a Verify Job and then checking the current state of your system against the snapshot, on a regular basis (e.g. nightly).

The first step is to set up a Verify Job and to run it with:



::

   Level = InitCatalog



The InitCatalog level tells Bareos simply to get the information on the specified files and to put it into the catalog. That is your database is initialized and no comparison is done. The InitCatalog is normally run one time manually.

Thereafter, you will run a Verify Job on a daily (or whatever) basis with:



::

   Level = Catalog



The Level = Catalog level tells Bareos to compare the current state of the files on the Client to the last InitCatalog that is stored in the catalog and to report any differences. See the example below for the format of the output.

You decide what files you want to form your "snapshot" by specifying them in a FileSet resource, and normally, they will be system files that do not change, or that only certain features change.

Then you decide what attributes of each file you want compared by specifying comparison options on the Include statements that you use in the FileSet resource of your Catalog Jobs.

The Details
-----------

:index:`\ <single: Verify; Details>`

In the discussion that follows, we will make reference to the Verify Configuration Example that is included below in the A Verify Configuration Example section. You might want to look it over now to get an idea of what it does.

The main elements consist of adding a schedule, which will normally be run daily, or perhaps more often. This is provided by the VerifyCycle Schedule, which runs at 5:05 in the morning every day.

Then you must define a Job, much as is done below. We recommend that the Job name contain the name of your machine as well as the word Verify or Check. In our example, we named it MatouVerify. This will permit you to easily identify your job when running it from the Console.

You will notice that most records of the Job are quite standard, but that the FileSet resource contains verify=pins1 option in addition to the standard signature=SHA1 option. If you don’t want SHA1 signature comparison, and we cannot imagine why not, you can drop the signature=SHA1 and none will be computed nor stored in the catalog. Or alternatively, you can use verify=pins5 and signature=MD5, which will use the MD5 hash algorithm. The MD5 hash computes faster than SHA1, but is
cryptographically less secure.

The verify=pins1 is ignored during the InitCatalog Job, but is used during the subsequent Catalog Jobs to specify what attributes of the files should be compared to those found in the catalog. pins1 is a reasonable set to begin with, but you may want to look at the details of these and other options. They can be found in the :ref:`FileSet Resource <FileSetResource>` section of this manual. Briefly, however, the p of the pins1 tells Verify to compare the permissions bits, the i is to
compare inodes, the n causes comparison of the number of links, the s compares the file size, and the 1 compares the SHA1 checksums (this requires the signature=SHA1 option to have been set also).

You must also specify the Client and the Catalog resources for your Verify job, but you probably already have them created for your client and do not need to recreate them, they are included in the example below for completeness.

As mentioned above, you will need to have a FileSet resource for the Verify job, which will have the additional verify=pins1 option. You will want to take some care in defining the list of files to be included in your FileSet. Basically, you will want to include all system (or other) files that should not change on your system. If you select files, such as log files or mail files, which are constantly changing, your automatic Verify job will be constantly finding differences. The objective in
forming the FileSet is to choose all unchanging important system files. Then if any of those files has changed, you will be notified, and you can determine if it changed because you loaded a new package, or because someone has broken into your computer and modified your files. The example below shows a list of files that I use on my Red Hat 7.3 system. Since I didn’t spend a lot of time working on it, it probably is missing a few important files (if you find one, please send it to me). On the
other hand, as long as I don’t load any new packages, none of these files change during normal operation of the system.

Running the Verify
------------------

:index:`\ <single: Verify; Running>`

The first thing you will want to do is to run an InitCatalog level Verify Job. This will initialize the catalog to contain the file information that will later be used as a basis for comparisons with the actual file system, thus allowing you to detect any changes (and possible intrusions into your system).

The easiest way to run the InitCatalog is manually with the console program by simply entering run. You will be presented with a list of Jobs that can be run, and you will choose the one that corresponds to your Verify Job, MatouVerify in this example.



::

   The defined Job resources are:
        1: MatouVerify
        2: usersrestore
        3: Filetest
        4: usersave
   Select Job resource (1-4): 1



Next, the console program will show you the basic parameters of the Job and ask you:



::

   Run Verify job
   JobName:  MatouVerify
   FileSet:  Verify Set
   Level:    Catalog
   Client:   MatouVerify
   Storage:  DLTDrive
   Verify Job:
   Verify List: /tmp/regress/working/MatouVerify.bsr
   OK to run? (yes/mod/no): mod



Here, you want to respond mod to modify the parameters because the Level is by default set to Catalog and we want to run an InitCatalog Job. After responding mod, the console will ask:



::

   Parameters to modify:
        1: Level
        2: Storage
        3: Job
        4: FileSet
        5: Client
        6: When
        7: Priority
        8: Pool
        9: Verify Job
   Select parameter to modify (1-5): 1



you should select number 2 to modify the Level, and it will display:



::

   Levels:
        1: Initialize Catalog
        2: Verify Catalog
        3: Verify Volume to Catalog
        4: Verify Disk to Catalog
        5: Verify Volume Data (not yet implemented)
   Select level (1-4): 1



Choose item 1, and you will see the final display:



::

   Run Verify job
   JobName:  MatouVerify
   FileSet:  Verify Set
   Level:    Initcatalog
   Client:   MatouVerify
   Storage:  DLTDrive
   Verify Job:
   Verify List: /tmp/regress/working/MatouVerify.bsr
   OK to run? (yes/mod/no): yes



at which point you respond yes, and the Job will begin.

Thereafter the Job will automatically start according to the schedule you have defined. If you wish to immediately verify it, you can simply run a Verify Catalog which will be the default. No differences should be found.

To use a previous job, you can add ``jobid=xxx`` option in run command line. It will run the Verify job against the specified job.

::

   *run jobid=1 job=MatouVerify
   Run Verify job
   JobName:     MatouVerify
   Level:       Catalog
   Client:      127.0.0.1-fd
   FileSet:     Full Set
   Pool:        Default (From Job resource)
   Storage:     File (From Job resource)
   Verify Job:  MatouVerify.2010-09-08_15.33.33_03
   Verify List: /tmp/regress/working/MatouVerify.bsr
   When:        2010-09-08 15:35:32
   Priority:    10
   OK to run? (yes/mod/no):

What To Do When Differences Are Found
-------------------------------------

:index:`\ <single: Verify; Differences>`

If you have setup your messages correctly, you should be notified if there are any differences and exactly what they are. For example, below is the email received after doing an update of OpenSSH:



::

   HeadMan: Start Verify JobId 83 Job=RufusVerify.2002-06-25.21:41:05
   HeadMan: Verifying against Init JobId 70 run 2002-06-21 18:58:51
   HeadMan: File: /etc/pam.d/sshd
   HeadMan:       st_ino   differ. Cat: 4674b File: 46765
   HeadMan: File: /etc/rc.d/init.d/sshd
   HeadMan:       st_ino   differ. Cat: 56230 File: 56231
   HeadMan: File: /etc/ssh/ssh_config
   HeadMan:       st_ino   differ. Cat: 81317 File: 8131b
   HeadMan:       st_size  differ. Cat: 1202 File: 1297
   HeadMan:       SHA1 differs.
   HeadMan: File: /etc/ssh/sshd_config
   HeadMan:       st_ino   differ. Cat: 81398 File: 81325
   HeadMan:       st_size  differ. Cat: 1182 File: 1579
   HeadMan:       SHA1 differs.
   HeadMan: File: /etc/ssh/ssh_config.rpmnew
   HeadMan:       st_ino   differ. Cat: 812dd File: 812b3
   HeadMan:       st_size  differ. Cat: 1167 File: 1114
   HeadMan:       SHA1 differs.
   HeadMan: File: /etc/ssh/sshd_config.rpmnew
   HeadMan:       st_ino   differ. Cat: 81397 File: 812dd
   HeadMan:       st_size  differ. Cat: 2528 File: 2407
   HeadMan:       SHA1 differs.
   HeadMan: File: /etc/ssh/moduli
   HeadMan:       st_ino   differ. Cat: 812b3 File: 812ab
   HeadMan: File: /usr/bin/scp
   HeadMan:       st_ino   differ. Cat: 5e07e File: 5e343
   HeadMan:       st_size  differ. Cat: 26728 File: 26952
   HeadMan:       SHA1 differs.
   HeadMan: File: /usr/bin/ssh-keygen
   HeadMan:       st_ino   differ. Cat: 5df1d File: 5e07e
   HeadMan:       st_size  differ. Cat: 80488 File: 84648
   HeadMan:       SHA1 differs.
   HeadMan: File: /usr/bin/sftp
   HeadMan:       st_ino   differ. Cat: 5e2e8 File: 5df1d
   HeadMan:       st_size  differ. Cat: 46952 File: 46984
   HeadMan:       SHA1 differs.
   HeadMan: File: /usr/bin/slogin
   HeadMan:       st_ino   differ. Cat: 5e359 File: 5e2e8
   HeadMan: File: /usr/bin/ssh
   HeadMan:       st_mode  differ. Cat: 89ed File: 81ed
   HeadMan:       st_ino   differ. Cat: 5e35a File: 5e359
   HeadMan:       st_size  differ. Cat: 219932 File: 234440
   HeadMan:       SHA1 differs.
   HeadMan: File: /usr/bin/ssh-add
   HeadMan:       st_ino   differ. Cat: 5e35b File: 5e35a
   HeadMan:       st_size  differ. Cat: 76328 File: 81448
   HeadMan:       SHA1 differs.
   HeadMan: File: /usr/bin/ssh-agent
   HeadMan:       st_ino   differ. Cat: 5e35c File: 5e35b
   HeadMan:       st_size  differ. Cat: 43208 File: 47368
   HeadMan:       SHA1 differs.
   HeadMan: File: /usr/bin/ssh-keyscan
   HeadMan:       st_ino   differ. Cat: 5e35d File: 5e96a
   HeadMan:       st_size  differ. Cat: 139272 File: 151560
   HeadMan:       SHA1 differs.
   HeadMan: 25-Jun-2002 21:41
   JobId:                  83
   Job:                    RufusVerify.2002-06-25.21:41:05
   FileSet:                Verify Set
   Verify Level:           Catalog
   Client:                 RufusVerify
   Start time:             25-Jun-2002 21:41
   End time:               25-Jun-2002 21:41
   Files Examined:         4,258
   Termination:            Verify Differences



At this point, it was obvious that these files were modified during installation of the RPMs. If you want to be super safe, you should run a Verify Level=Catalog immediately before installing new software to verify that there are no differences, then run a Verify Level=InitCatalog immediately after the installation.

To keep the above email from being sent every night when the Verify Job runs, we simply re-run the Verify Job setting the level to InitCatalog (as we did above in the very beginning). This will re-establish the current state of the system as your new basis for future comparisons. Take care that you don’t do an InitCatalog after someone has placed a Trojan horse on your system!

If you have included in your FileSet a file that is changed by the normal operation of your system, you will get false matches, and you will need to modify the FileSet to exclude that file (or not to Include it), and then re-run the InitCatalog.

The FileSet that is shown below is what I use on my Red Hat 7.3 system. With a bit more thought, you can probably add quite a number of additional files that should be monitored.

A Verify Configuration Example
------------------------------

:index:`\ <single: Verify; Example>`



::

   Schedule {
     Name = "VerifyCycle"
     Run = Level=Catalog sun-sat at 5:05
   }
   Job {
     Name = "MatouVerify"
     Type = Verify
     Level = Catalog                     # default level
     Client = MatouVerify
     FileSet = "Verify Set"
     Messages = Standard
     Storage = DLTDrive
     Pool = Default
     Schedule = "VerifyCycle"
   }
   #
   # The list of files in this FileSet should be carefully
   # chosen. This is a good starting point.
   #
   FileSet {
     Name = "Verify Set"
     Include {
       Options {
         verify=pins1
         signature=SHA1
       }
       File = /boot
       File = /bin
       File = /sbin
       File = /usr/bin
       File = /lib
       File = /root/.ssh
       File = /home/user/.ssh
       File = /var/named
       File = /etc/sysconfig
       File = /etc/ssh
       File = /etc/security
       File = /etc/exports
       File = /etc/rc.d/init.d
       File = /etc/sendmail.cf
       File = /etc/sysctl.conf
       File = /etc/services
       File = /etc/xinetd.d
       File = /etc/hosts.allow
       File = /etc/hosts.deny
       File = /etc/hosts
       File = /etc/modules.conf
       File = /etc/named.conf
       File = /etc/pam.d
       File = /etc/resolv.conf
     }
     Exclude = { }
   }
   Client {
     Name = MatouVerify
     Address = lmatou
     Catalog = Bareos
     Password = ""
     File Retention = 80d                # 80 days
     Job Retention = 1y                  # one year
     AutoPrune = yes                     # Prune expired Jobs/Files
   }
   Catalog {
     Name = Bareos
     dbname = verify; user = bareos; password = ""
   }






