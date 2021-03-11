PoC code to implement a Bareos commandline tool that allows to trigger
common tasks from a Bareos client.
bsmc stands for "Bareos simple management cli"

Implemented are show scheduler, trigger incremental backup and restore a
single given file. Further the bsmc archive command backups all given
filenames and directories using a predefined archive job.

Sample Calls:
```
bsmc rest filename
bsmc incr
bsmc query sched
bsmc archive /etc/vimrc /etc/bash.bashrc /etc/profile.d/ 
```

Query for lost files and restore:
```
[root@titania etc]# rm -f vimrc
[root@titania etc]# bsmc query lost
./vimrc
[root@titania etc]# bsmc rest vimrc
INFO bsmc.runJob: Job scheduled with JobId: 166
INFO bsmc.runJob: OK: Job 166 finished with with status T
[root@titania etc]# ls -l vimrc
-rw-r--r--. 1 root root 2029 15. Okt 11:36 vimrc
```

You need a config file
/etc/bareos/bsmc.conf

This contains information about how to connect to the director and
basic information about the client itself like fd name and jobs that
are associated with this client.

Sample bsmc.conf:
```
[director]
server=laptop.fqhn
name=laptop-dir
password=geheim

[client]
# fd name
name=laptop-fd
jobs=BackupClient1,some,other,jobs
# this job will be used for 'bsmc archive' calls
archivejob=archive
# Used as includefile by the archivejob and will be temporarily created
# and filled by 'bsmc archive' calls
archivefilelist=/tmp/bsmc.archive
```

Sample configuration for an archive job to be in included in your bareos-dir.conf is here:
etc/bareos/bareos-dir.d/archivejob.conf
