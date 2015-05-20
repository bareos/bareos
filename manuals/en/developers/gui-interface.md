Implementing a GUI Interface
============================

General
-------

This document is intended mostly for developers who wish to develop a
new GUI interface to <span>**Bareos**</span>.

### Minimal Code in Console Program

Until now, I have kept all the Catalog code in the Directory (with the
exception of dbcheck and bscan). This is because at some point I would
like to add user level security and access. If we have code spread
everywhere such as in a GUI this will be more difficult. The other
advantage is that any code you add to the Director is automatically
available to both the tty console program and other programs. The major
disadvantage is it increases the size of the code – however, compared to
Networker the Bareos Director is really tiny.

### GUI Interface is Difficult

Interfacing to an interactive program such as Bareos can be very
difficult because the interfacing program must interpret all the prompts
that may come. This can be next to impossible. There are are a number of
ways that Bareos is designed to facilitate this:

-   The Bareos network protocol is packet based, and thus pieces of
    information sent can be ASCII or binary.

-   The packet interface permits knowing where the end of a list is.

-   The packet interface permits special “signals” to be passed rather
    than data.

-   The Director has a number of commands that are non-interactive. They
    all begin with a period, and provide things such as the list of all
    Jobs, list of all Clients, list of all Pools, list of all Storage,
    ... Thus the GUI interface can get to virtually all information that
    the Director has in a deterministic way. See
    <span>\<</span>bareos-source<span>\></span>/src/dird/ua\_dotcmds.c
    for more details on this.

-   Most console commands allow all the arguments to be specified on the
    command line: e.g. <span>**run job=NightlyBackup level=Full**</span>

One of the first things to overcome is to be able to establish a
conversation with the Director. Although you can write all your own
code, it is probably easier to use the Bareos subroutines. The following
code is used by the Console program to begin a conversation.

    static BSOCK *UA_sock = NULL;
    static JCR *jcr;
    ...
      read-your-config-getting-address-and-pasword;
      UA_sock = bnet_connect(NULL, 5, 15, "Director daemon", dir->address,
                              NULL, dir->DIRport, 0);
       if (UA_sock == NULL) {
          terminate_console(0);
          return 1;
       }
       jcr.dir_bsock = UA_sock;
       if (!authenticate_director(\&jcr, dir)) {
          fprintf(stderr, "ERR=%s", UA_sock->msg);
          terminate_console(0);
          return 1;
       }
       read_and_process_input(stdin, UA_sock);
       if (UA_sock) {
          bnet_sig(UA_sock, BNET_TERMINATE); /* send EOF */
          bnet_close(UA_sock);
       }
       exit 0;

Then the read\_and\_process\_input routine looks like the following:

       get-input-to-send-to-the-Director;
       bnet_fsend(UA_sock, "%s", input);
       stat = bnet_recv(UA_sock);
       process-output-from-the-Director;

For a GUI program things will be a bit more complicated. Basically in
the very inner loop, you will need to check and see if any output is
available on the UA\_sock. For an example, please take a look at the WX
GUI interface code in:

Bvfs API {#sec:bvfs}
--------

To help developers of restore GUI interfaces, we have added new *dot
commands* that permit browsing the catalog in a very simple way.

Bat has now a bRestore panel that uses Bvfs to display files and
directories.

![image](\idir bat-brestore) [fig:batbrestore]

The Bvfs module works correctly with BaseJobs, Copy and Migration jobs.

This project was funded by Bareos Systems.

### General notes {#general-notes .unnumbered}

-   All fields are separated by a tab

-   You can specify `limit=` and `offset=` to list smoothly records in
    very big directories

-   All operations (except cache creation) are designed to run instantly

-   At this time, Bvfs works faster on PostgreSQL than MySQL catalog. If
    you can contribute new faster SQL queries we will be happy, else
    don’t complain about speed.

-   The cache creation is dependent of the number of directories. As
    Bvfs shares information accross jobs, the first creation can be slow

-   All fields are separated by a tab

-   Due to potential encoding problem, it’s advised to allways use
    pathid in queries.

### Get dependent jobs from a given JobId {#get-dependent-jobs-from-a-given-jobid .unnumbered}

Bvfs allows you to query the catalog against any combination of jobs.
You can combine all Jobs and all FileSet for a Client in a single
session.

To get all JobId needed to restore a particular job, you can use the
`.bvfs_get_jobids` command.

    .bvfs_get_jobids jobid=num [all]

    .bvfs_get_jobids jobid=10
    1,2,5,10
    .bvfs_get_jobids jobid=10 all
    1,2,3,5,10

In this example, a normal restore will need to use JobIds 1,2,5,10 to
compute a complete restore of the system.

With the `all` option, the Director will use all defined FileSet for
this client.

### Generating Bvfs cache {#generating-bvfs-cache .unnumbered}

The `.bvfs_update` command computes the directory cache for jobs
specified in argument, or for all jobs if unspecified.

    .bvfs_update [jobid=numlist]

Example:

    .bvfs_update jobid=1,2,3

You can run the cache update process in a RunScript after the catalog
backup.

### Get all versions of a specific file {#get-all-versions-of-a-specific-file .unnumbered}

Bvfs allows you to find all versions of a specific file for a given
Client with the `.bvfs_version` command. To avoid problems with
encoding, this function uses only PathId and FilenameId. The jobid
argument is mandatory but unused.

    .bvfs_versions client=filedaemon pathid=num filenameid=num jobid=1
    PathId FilenameId FileId JobId LStat Md5 VolName Inchanger
    PathId FilenameId FileId JobId LStat Md5 VolName Inchanger
    ...

Example:

    .bvfs_versions client=localhost-fd pathid=1 fnid=47 jobid=1
    1  47  52  12  gD HRid IGk D Po Po A P BAA I A   /uPgWaxMgKZlnMti7LChyA  Vol1  1

### List directories {#list-directories .unnumbered}

Bvfs allows you to list directories in a specific path.

    .bvfs_lsdirs pathid=num path=/apath jobid=numlist limit=num offset=num
    PathId  FilenameId  FileId  JobId  LStat  Path
    PathId  FilenameId  FileId  JobId  LStat  Path
    PathId  FilenameId  FileId  JobId  LStat  Path
    ...

You need to `pathid` or `path`. Using `path=` will list “/” on Unix and
all drives on Windows. If FilenameId is 0, the record listed is a
directory.

    .bvfs_lsdirs pathid=4 jobid=1,11,12
    4       0       0       0       A A A A A A A A A A A A A A     .
    5       0       0       0       A A A A A A A A A A A A A A     ..
    3       0       0       0       A A A A A A A A A A A A A A     regress/

In this example, to list directories present in `regress/`, you can use

    .bvfs_lsdirs pathid=3 jobid=1,11,12
    3       0       0       0       A A A A A A A A A A A A A A     .
    4       0       0       0       A A A A A A A A A A A A A A     ..
    2       0       0       0       A A A A A A A A A A A A A A     tmp/

### List files {#list-files .unnumbered}

Bvfs allows you to list files in a specific path.

    .bvfs_lsfiles pathid=num path=/apath jobid=numlist limit=num offset=num
    PathId  FilenameId  FileId  JobId  LStat  Path
    PathId  FilenameId  FileId  JobId  LStat  Path
    PathId  FilenameId  FileId  JobId  LStat  Path
    ...

You need to `pathid` or `path`. Using `path=` will list “/” on Unix and
all drives on Windows. If FilenameId is 0, the record listed is a
directory.

    .bvfs_lsfiles pathid=4 jobid=1,11,12
    4       0       0       0       A A A A A A A A A A A A A A     .
    5       0       0       0       A A A A A A A A A A A A A A     ..
    1       0       0       0       A A A A A A A A A A A A A A     regress/

In this example, to list files present in `regress/`, you can use

    .bvfs_lsfiles pathid=1 jobid=1,11,12
    1   47   52   12    gD HRid IGk BAA I BMqcPH BMqcPE BMqe+t A     titi
    1   49   53   12    gD HRid IGk BAA I BMqe/K BMqcPE BMqe+t B     toto
    1   48   54   12    gD HRie IGk BAA I BMqcPH BMqcPE BMqe+3 A     tutu
    1   45   55   12    gD HRid IGk BAA I BMqe/K BMqcPE BMqe+t B     ficheriro1.txt
    1   46   56   12    gD HRie IGk BAA I BMqe/K BMqcPE BMqe+3 D     ficheriro2.txt

### Restore set of files {#restore-set-of-files .unnumbered}

Bvfs allows you to create a SQL table that contains files that you want
to restore. This table can be provided to a restore command with the
file option.

    .bvfs_restore fileid=numlist dirid=numlist hardlink=numlist path=b2num
    OK
    restore file=?b2num ...

To include a directory (with `dirid`), Bvfs needs to run a query to
select all files. This query could be time consuming.

`hardlink` list is always composed of a serie of two numbers (jobid,
fileindex). This information can be found in the LinkFI field of the
LStat packet.

The `path` argument represents the name of the table that Bvfs will
store results. The format of this table is `b2[0-9]+`. (Should start by
b2 and followed by digits).

Example:

    .bvfs_restore fileid=1,2,3,4 hardlink=10,15,10,20 jobid=10 path=b20001
    OK

### Cleanup after Restore {#cleanup-after-restore .unnumbered}

To drop the table used by the restore command, you can use the
`.bvfs_cleanup` command.

    .bvfs_cleanup path=b20001

### Clearing the BVFS Cache {#clearing-the-bvfs-cache .unnumbered}

To clear the BVFS cache, you can use the `.bvfs_clear_cache` command.

    .bvfs_clear_cache yes
    OK
