Bareos Regression Testing
=========================

.. note::

   While Bareos Regression Testing is still in use,
   new tests should be written as :ref:`DeveloperGuide/BuildAndTestBareos:Systemtests`.

Setting up Regession Testing
----------------------------

This document is intended mostly for developers who wish to ensure that
their changes to Bareos don’t introduce bugs in the base code. However,
you don’t need to be a developer to run the regression scripts, and we
recommend them before putting your system into production, and before
each upgrade, especially if you build from source code. They are simply
shell scripts that drive Bareos through bconsole and then typically
compare the input and output with **diff**.

To get started, we recommend that you create a directory named
**bareos**, under which you will put the current source code and the
current set of regression scripts. Below, we will describe how to set
this up.

The top level directory that we call **bareos** can be named anything
you want. Note, all the standard regression scripts run as non-root and
can be run on the same machine as a production Bareos system (the
developers run it this way).

To create the directory structure for the current trunk and to clone the
repository, do the following (note, we assume you are working in your
home directory in a non-root account):

::

    git clone https://github.com/bareos/bareos.git

This will create the directory :file:`bareos`. The above should be
needed only once. Thereafter to update to the latest code, you do:

::

    cd bareos
    git pull

There are two different aspects of regression testing that this document
will discuss: 1. Running the Regression Script, 2. Writing a Regression
test.

Running the Regression Script
-----------------------------

There are a number of different tests that may be run, such as: the
standard set that uses disk Volumes and runs under any userid; a small
set of tests that write to tape; another set of tests where you must be
root to run them. Normally, I run all my tests as non-root and very
rarely run the root tests. The tests vary in length, and running the
full tests including disk based testing, tape based testing, autochanger
based testing, and multiple drive autochanger based testing can take 3
or 4 hours.

Setting the Configuration Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There is nothing you need to change in the source directory.

To begin:

::

    cd bareos/regress

The very first time you are going to run the regression scripts, you
will need to create a custom config file for your system. We suggest
that you start by:

::

    cp prototype.conf config

Then you can edit the **config** file directly.

::

    # Where to get the source to be tested
    BAREOS_SOURCE="${HOME}/bareos/core"

    # Where to send email   !!!!! Change me !!!!!!!
    EMAIL=your-name@your-domain.com
    SMTP_HOST="localhost"

    TAPE_DRIVE="/dev/nst0"
    # if you don't have an autochanger set AUTOCHANGER to /dev/null
    AUTOCHANGER="/dev/sg0"
    # For two drive tests -- set to /dev/null if you do not have it
    TAPE_DRIVE1="/dev/null"

    # This must be the path to the autochanger including its name
    AUTOCHANGER_PATH="/usr/sbin/mtx"

    # Set what backend to use "postresql" "mysql" or "sqlite3"
    DBTYPE="postgresql"

    # Set your database here
    #WHICHDB="--with-${DBTYPE}=${SQLITE3_DIR}"
    WHICHDB="--with-${DBTYPE}"

    # Set this to "--with-tcp-wrappers" or "--without-tcp-wrappers"
    TCPWRAPPERS="--with-tcp-wrappers"

    # Set this to "" to disable OpenSSL support, "--with-openssl=yes"
    # to enable it, or provide the path to the OpenSSL installation,
    # eg "--with-openssl=/usr/local"
    OPENSSL="--with-openssl"

    # You may put your real host name here, but localhost or 127.0.0.1
    # is valid also and it has the advantage that it works on a
    # non-networked machine
    HOST="localhost"

-  **BAREOS_SOURCE** should be the full path to the Bareos source code
   that you wish to test. It will be loaded configured, compiled, and
   installed with the “make setup” command, which needs to be done only
   once each time you change the source code.

-  **EMAIL** should be your email addres. Please remember to change this
   or I will get a flood of unwanted messages. You may or may not want
   to see these emails. In my case, I don’t need them so I direct it to
   the bit bucket.

-  **SMTP_HOST** defines where your SMTP server is.

-  **SQLITE_DIR** should be the full path to the sqlite package, must be
   build before running a Bareos regression, if you are using SQLite.
   This variable is ignored if you are using MySQL or PostgreSQL. To use
   PostgreSQL, edit the Makefile and change (or add)
   WHICHDB?=“``--``\ with-postgresql”. For MySQL use
   “WHICHDB=”\ ``--``\ with-mysql``.

   The advantage of using SQLite is that it is totally independent of
   any installation you may have running on your system, and there is no
   special configuration or authorization that must be done to run it.
   With both MySQL and PostgreSQL, you must pre-install the packages,
   initialize them and ensure that you have authorization to access the
   database and create and delete tables.

-  **TAPE_DRIVE** is the full path to your tape drive. The base set of
   regression tests do not use a tape, so this is only important if you
   want to run the full tests. Set this to /dev/null if you do not have
   a tape drive.

-  **TAPE_DRIVE1** is the full path to your second tape drive, if have
   one. The base set of regression tests do not use a tape, so this is
   only important if you want to run the full two drive tests. Set this
   to /dev/null if you do not have a second tape drive.

-  **AUTOCHANGER** is the name of your autochanger control device. Set
   this to /dev/null if you do not have an autochanger.

-  **AUTOCHANGER_PATH** is the full path including the program name for
   your autochanger program (normally **mtx**. Leave the default value
   if you do not have one.

-  **TCPWRAPPERS** defines whether or not you want the ./configure to be
   performed with tcpwrappers enabled.

-  **OPENSSL** used to enable/disable SSL support for Bareos
   communications and data encryption.

-  **HOST** is the hostname that it will use when building the scripts.
   The Bareos daemons will be named <HOST>-dir, <HOST>-fd, … It is also
   the name of the HOST machine that to connect to the daemons by the
   network. Hence the name should either be your real hostname (with an
   appropriate DNS or /etc/hosts entry) or **localhost** as it is in the
   default file.

-  **bin** is the binary location.

-  **scripts** is the bareos scripts location (where we could find
   database creation script, autochanger handler, etc.)

Building the Test Bareos
~~~~~~~~~~~~~~~~~~~~~~~~

Once the above variables are set, you can build the setup by entering:

::

    make setup

This will setup the regression testing and you should not need to do
this again unless you want to change the database or other regression
configuration parameters.

Setting up your SQL engine
~~~~~~~~~~~~~~~~~~~~~~~~~~

If you are using SQLite or SQLite3, there is nothing more to do; you can
simply run the tests as described in the next section.

If you are using MySQL or PostgreSQL, you will need to establish an
account with your database engine for the user name **regress** and you
will need to manually create a database named **regress** that can be
used by user name regress, which means you will have to give the user
regress sufficient permissions to use the database named regress. There
is no password on the regress account.

You have probably already done this procedure for the user name and
database named bareos. If not, the manual describes roughly how to do
it, and the scripts in bareos/regress/build/src/cats named
create_mysql_database, create_postgresql_database,
grant_mysql_privileges, and grant_postgresql_privileges may be of a help
to you.

Generally, to do the above, you will need to run under root to be able
to create databases and modify permissions within MySQL and PostgreSQL.

It is possible to configure MySQL access for database accounts that
require a password to be supplied. This can be done by creating a
 /.my.cnf file which supplies the credentials by default to the MySQL
commandline utilities.

::

    [client]
    host     = localhost
    user     = regress
    password = asecret

A similar technique can be used PostgreSQL regression testing where the
database is configured to require a password. The  /.pgpass file should
contain a line with the database connection properties.

::

    hostname:port:database:username:password

Running the Disk Only Regression
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The simplest way to copy the source code, configure it, compile it, link
it, and run the tests is to use a helper script:

::

    ./do_disk

This will run the base set of tests using disk Volumes. If you are
testing on a non-Linux machine several of the of the tests may not be
run. In any case, as we add new tests, the number will vary. It will
take about 1 hour and you don’t need to be root to run these tests (I
run under my regular userid). The result should be something similar to:

::

    Test results
      ===== auto-label-test OK 12:31:33 =====
      ===== backup-bareos-test OK 12:32:32 =====
      ===== bextract-test OK 12:33:27 =====
      ===== bscan-test OK 12:34:47 =====
      ===== bsr-opt-test OK 12:35:46 =====
      ===== compressed-test OK 12:36:52 =====
      ===== compressed-encrypt-test OK 12:38:18 =====
      ===== concurrent-jobs-test OK 12:39:49 =====
      ===== data-encrypt-test OK 12:41:11 =====
      ===== encrypt-bug-test OK 12:42:00 =====
      ===== fifo-test OK 12:43:46 =====
      ===== backup-bareos-fifo OK 12:44:54 =====
      ===== differential-test OK 12:45:36 =====
      ===== four-concurrent-jobs-test OK 12:47:39 =====
      ===== four-jobs-test OK 12:49:22 =====
      ===== incremental-test OK 12:50:38 =====
      ===== query-test OK 12:51:37 =====
      ===== recycle-test OK 12:53:52 =====
      ===== restore2-by-file-test OK 12:54:53 =====
      ===== restore-by-file-test OK 12:55:40 =====
      ===== restore-disk-seek-test OK 12:56:29 =====
      ===== six-vol-test OK 12:57:44 =====
      ===== span-vol-test OK 12:58:52 =====
      ===== sparse-compressed-test OK 13:00:00 =====
      ===== sparse-test OK 13:01:04 =====
      ===== two-jobs-test OK 13:02:39 =====
      ===== two-vol-test OK 13:03:49 =====
      ===== verify-vol-test OK 13:04:56 =====
      ===== weird-files2-test OK 13:05:47 =====
      ===== weird-files-test OK 13:06:33 =====
      ===== migration-job-test OK 13:08:15 =====
      ===== migration-jobspan-test OK 13:09:33 =====
      ===== migration-volume-test OK 13:10:48 =====
      ===== migration-time-test OK 13:12:59 =====
      ===== hardlink-test OK 13:13:50 =====
      ===== two-pool-test OK 13:18:17 =====
      ===== fast-two-pool-test OK 13:24:02 =====
      ===== two-volume-test OK 13:25:06 =====
      ===== incremental-2disk OK 13:25:57 =====
      ===== 2drive-incremental-2disk OK 13:26:53 =====
      ===== scratch-pool-test OK 13:28:01 =====
    Total time = 0:57:55 or 3475 secs

and the working tape tests are run with

::

    make full_test

    Test results

      ===== Bareos tape test OK =====
      ===== Small File Size test OK =====
      ===== restore-by-file-tape test OK =====
      ===== incremental-tape test OK =====
      ===== four-concurrent-jobs-tape OK =====
      ===== four-jobs-tape OK =====

Each separate test is self contained in that it initializes to run
Bareos from scratch (i.e. newly created database). It will also kill any
Bareos session that is currently running. In addition, it uses ports
8101, 8102, and 8103 so that it does not intefere with a production
system.

Alternatively, you can do the ./do_disk work by hand with:

::

    make setup

The above will then copy the source code within the regression tree (in
directory regress/build), configure it, and build it. There should be no
errors. If there are, please correct them before continuing. From this
point on, as long as you don’t change the Bareos source code, you should
not need to repeat any of the above steps. If you pull down a new
version of the source code, simply run **make setup** again.

Once Bareos is built, you can run the basic disk only non-root
regression test by entering:

::

    make test

Other Tests
~~~~~~~~~~~

There are a number of other tests that can be run as well. All the tests
are a simply shell script keep in the regress directory. For example the
”make test`\` simply executes **./all-non-root-tests**. The other tests,
which are invoked by directly running the script are:

all_non-root-tests
    All non-tape tests not requiring root. This is the standard set of
    tests, that in general, backup some data, then restore it, and
    finally compares the restored data with the original data.
all-root-tests
    All non-tape tests requiring root permission. These are a relatively
    small number of tests that require running as root. The amount of
    data backed up can be quite large. For example, one test backs up
    /usr, another backs up /etc. One or more of these tests reports an
    error – I’ll fix it one day.
all-non-root-tape-tests
    All tape test not requiring root. There are currently three tests,
    all run without being root, and backup to a tape. The first two
    tests use one volume, and the third test requires an autochanger,
    and uses two volumes. If you don’t have an autochanger, then this
    script will probably produce an error.
all-tape-and-file-tests
    All tape and file tests not requiring root. This includes just about
    everything, and I don’t run it very often.

If a Test Fails
~~~~~~~~~~~~~~~

If you one or more tests fail, the line output will be similar to:

::

      !!!!! concurrent-jobs-test failed!!! !!!!!

If you want to determine why the test failed, you will need to rerun the
script with the debug output turned on. You do so by defining the
environment variable **REGRESS_DEBUG** with commands such as:

::

    REGRESS_DEBUG=1
    export REGRESS_DEBUG

Then from the “regress” directory (all regression scripts assume that
you have “regress” as the current directory), enter:

::

    tests/test-name

where test-name should be the name of a test script – for example:
**tests/backup-bareos-test**.

Testing a Binary Installation
-----------------------------

If you have installed your Bareos from a binary release such as (rpms or
debs), you can still run regression tests on it. First, make sure that
your regression **config** file uses the same catalog backend as your
installed binaries. Then define the variables ``bin`` and ``scripts``
variables in your config file.

Example:

::

    bin=/usr/sbin/
    scripts=/usr/lib/bareos/scripts/

The ``./scripts/prepare-other-loc`` will tweak the regress scripts to
use your binary location. You will need to run it manually once before
you run any regression tests.

::

    $ ./scripts/prepare-other-loc
    $ ./tests/backup-bareos-test
    ...

All regression scripts must be run by hand or by calling the test
scripts. These are principally scripts that begin with **all_…** such as
**all_disk_tests**, **./all_test** …

None of the **./do_disk**, **./do_all**, **./nightly…** scripts will
work.

If you want to switch back to running the regression scripts from
source, first remove the **bin** and **scripts** variables from your
**config** file and rerun the ``make setup`` step.

Running a Single Test
---------------------

If you wish to run a single test, you can simply:

::

    cd regress
    tests/<name-of-test>

or, if the source code has been updated, you would do:

::

    cd bareos
    git pull
    cd regress
    make setup
    tests/backup-to-null

Writing a Regression Test
-------------------------

Any developer, who implements a major new feature, should write a
regression test that exercises and validates the new feature. Each
regression test is a complete test by itself. It terminates any running
Bareos, initializes the database, starts Bareos, then runs the test by
using the console program.

Running the Tests by Hand
~~~~~~~~~~~~~~~~~~~~~~~~~

You can run any individual test by hand by cd’ing to the **regress**
directory and entering:

::

    tests/<test-name>

Directory Structure
~~~~~~~~~~~~~~~~~~~

The directory structure of the regression tests is:

::

      regress                - Makefile, scripts to start tests
        |------ scripts      - Scripts (and old configuration files)
        |------ tests        - All test scripts are here
        |------ configs      - configuration files (for newer tests)
        |
        |------------------ -- All directories below this point are used
        |                       for testing, but are created from the
        |                       above directories and are removed with
        |                       "make distclean"
        |
        |------ bin          - This is the install directory for
        |                        Bareos to be used testing
        |------ build        - Where the Bareos source build tree is
        |------ tmp          - Most temp files go here
        |------ working      - Bareos working directory
        |------ weird-files  - Weird files used in two of the tests.

Adding a New Test
~~~~~~~~~~~~~~~~~

If you want to write a new regression test, it is best to start with one
of the existing test scripts, and modify it to do the new test.

When adding a new test, be extremely careful about adding anything to
any of the daemons’ configuration files. The reason is that it may
change the prompts that are sent to the console. For example, adding a
Pool means that the current scripts, which assume that Bareos
automatically selects a Pool, will now be presented with a new prompt,
so the test will fail. If you need to enhance the configuration files,
consider making your own versions.

Running a Test Under The Debugger
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can run a test under the debugger (actually run a Bareos daemon
under the debugger) by first setting the environment variable
**REGRESS_WAIT** with commands such as:

::

    REGRESS_WAIT=1
    export REGRESS_WAIT

Then executing the script. When the script prints the following line:

::

    Start Bareos under debugger and enter anything when ready ...

You start the Bareos component you want to run under the debugger in a
different shell window. For example:

::

    cd .../regress/bin
    gdb bareos-sd
    (possibly set breakpoints, ...)
    run -s -f

Then enter any character in the window with the above message. An error
message will appear saying that the daemon you are debugging is already
running, which is the case. You can simply ignore the error message.
