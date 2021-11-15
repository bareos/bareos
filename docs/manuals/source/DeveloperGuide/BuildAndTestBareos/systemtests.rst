.. _BareosSystemtestsChapter:

Systemtests
===========

Systemtests use the locally compiled version of the bareos binaries
and run tests on them. Preparations also have been made to run the
tests on installed binaries (originating from packages).

Prepare the PostgreSQL database for systemtests
-----------------------------------------------

The |dir| requires a database backend. The default database is |postgresql|.
To run systemtests, some preparations are required.

The system user running the Bareos systemtests
must be given permission to create databases and database user:

.. code-block:: shell-session
   :caption: Configure a PostgreSQL Server for systemtests

   user@host:~$ sudo -u postgres createuser --createdb --createrole $USER


Build Bareos for Systemtests
----------------------------

This following shell script will show how to build the Bareos test-environment from source.

.. code-block:: bash
   :caption: Example shell script

   #!/bin/sh

   mkdir bareos-local-tests
   cd bareos-local-tests
   git clone https://github.com/bareos/bareos.git

   mkdir build
   cd build

   # configure build environment
   cmake -Dpostgresql=yes -Dsystemtest_db_user=$USER -Dtraymonitor=yes ../bareos

   # build Bareos
   make

   # run system tests
   make test


Running Systemtests
-------------------

Tests are structured in directories.
Each test directory either contain
a single test script, named :file:`testrunner`
or multiple scripts named :file:`testrunner-*`
together with optional scripts :file:`test-setup` and :file:`test-cleanup`.
Having multiple test scripts combined reduces the overhead
but makes the scripts slightly more complicated,
as they must be able to run in arbitrary order.

Each test directory is designed to be independent,
so that different tests can be run in parallel without interfering with each other.

Run all system tests
~~~~~~~~~~~~~~~~~~~~

.. code-block:: shell-session
   :caption: List available ctests

   user@host:~$ cd bareos-local-tests/build
   user@host:~/bareos-local-tests/build$ ctest --show-only
   Test project ~/bareos-local-tests/build
     Test   #1: system:acl
     Test   #2: system:ai-consolidate-ignore-duplicate-job
     Test   #3: system:autochanger (Disabled)
     Test   #4: system:bareos
     Test   #5: system:bareos-acl
     Test   #6: system:bconsole-pam (Disabled)
     Test   #7: system:bconsole-status-client
     ...
     Test  #58: system:reload:setup
     Test  #59: system:reload:add-client
     Test  #60: system:reload:add-duplicate-job
     Test  #61: system:reload:add-empty-job
     Test  #62: system:reload:add-second-director
     Test  #63: system:reload:add-uncommented-string
     Test  #64: system:reload:unchanged-config
     Test  #65: system:reload:cleanup
     ...


.. code-block:: shell-session
   :caption: Run all system tests

   user@host:~$ cd bareos-local-tests/build
   user@host:~/bareos-local-tests/build$ make test

   Running tests...
   Test project ~/bareos-local-tests/build
         Start  1: system:acl
    1/88 Test  #1: system:acl ...........   Passed   15.81 sec
         Start  2: system:ai-consolidate-ignore-duplicate-job
   ...


Instead of using :command:`make test`, :command:`ctest` can be directly invoked.
This offers some advantages, like being able to run multiple tests in parallel with
:command:`ctest -j <number of parallel tests>`.
Only jobs with names matching a certain regular expression can be run with
:command:`ctest -R`, and verbose test output can be enabled with :command:`ctest -V`.
Please refer to the ctest documentation.

Run a single system test
~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: shell-session
   :caption: Run a single system test by ctest

   user@host:~$ cd bareos-local-tests/build
   user@host:~/bareos-local-tests/build$ ctest --verbose --tests-regex acl
   UpdateCTestConfiguration  from :~/bareos-local-tests/build/DartConfiguration.tcl
   Parse Config file:~/bareos-local-tests/build/DartConfiguration.tcl
   UpdateCTestConfiguration  from :~/bareos-local-tests/build/DartConfiguration.tcl
   Parse Config file:~/bareos-local-tests/build/DartConfiguration.tcl
   Test project ~/bareos-local-tests/build
   Constructing a list of tests
   Done constructing a list of tests
   Updating test list for fixtures
   Added 0 tests to meet fixture requirements
   Checking test dependency graph...
   Checking test dependency graph end
   test 1
       Start 1: system:acl

   1: Test command: ~/bareos-local-tests/build/systemtests/tests/acl/testrunner
   1: Test timeout computed to be: 1500
   1: creating database (postgresql)
   1: running ~/bareos-local-tests/build/systemtests/scripts/setup
   1:
   1:
   1: === acl: starting at 16:09:46 ===
   1: =
   1: =
   1: =
   1: =
   1: === acl: OK at 16:09:56 ===
   1/1 Test #1: system:acl ........   Passed   10.90 sec

   The following tests passed:
           system:acl

   100% tests passed, 0 tests failed out of 1

   Total Test time (real) =  10.91 sec

or change into a test directory and run :command:`testrunner` directly:

.. code-block:: shell-session
   :caption: Run a single system test by testrunner

   user@host:~$ cd bareos-local-tests/build
   user@host:~/bareos-local-tests/build$ cd tests/acl
   user@host:~/bareos-local-tests/build/tests/acl$ ./testrunner
   creating database (postgresql)
   running ~/bareos-local-tests/build/systemtests/scripts/setup


   === acl: starting at 15:03:20 ===
   =
   =
   =
   =
   === acl: OK at 15:03:35 ===


For verbose output, set ``export REGRESS_DEBUG=1`` before running :command:`testrunner`.


The test environment persists after runnning a test.
So to further debug a problem,
the Bareos daemons can be started again,
and a :command:`bconsole` session can be used to retrieve information:


.. code-block:: shell-session
   :caption: Doing manual tests in a test-environment

   user@host:~$ cd bareos-local-tests/build
   user@host:~/bareos-local-tests/build$ cd tests/acl
   user@host:~/bareos-local-tests/build/tests/acl$ bin/bareos status
   bareos-dir is stopped
   bareos-sd is stopped
   bareos-fd is stopped
   user@host:~/bareos-local-tests/build/tests/acl$ bin/bareos start
   Starting the  Storage daemon
   Starting the  File daemon
   Starting the  Director daemon
   Checking Configuration and Database connection ...
   user@host:~/bareos-local-tests/build/tests/acl$ bin/bareos status
   bareos-dir (pid 2782) is running...
   bareos-sd (pid 2761) is running...
   bareos-fd (pid 2770) is running...
   user@host:~/bareos-local-tests/build/tests/acl$ bin/bconsole
   Connecting to Director localhost:42001
    Encryption: TLS_CHACHA20_POLY1305_SHA256
   1000 OK: bareos-dir Version: 19.1.2 (01 February 2019)
   self-compiled binary
   self-compiled binaries are UNSUPPORTED by bareos.com.
   Get official binaries and vendor support on https://www.bareos.com
   You are connected using the default console

   Enter a period to cancel a command.
   *list jobs
   Automatically selected Catalog: MyCatalog
   Using Catalog "MyCatalog"
   +-------+------------------+-----------+---------------------+------+-------+----------+----------+-----------+
   | JobId | Name             | Client    | StartTime           | Type | Level | JobFiles | JobBytes | JobStatus |
   +-------+------------------+-----------+---------------------+------+-------+----------+----------+-----------+
   | 1     | backup-bareos-fd | bareos-fd | 2019-08-15 15:04:37 | B    | F     | 21       | 138399   | T         |
   | 2     | RestoreFiles     | bareos-fd | 2019-08-15 15:04:41 | R    | F     | 21       | 138399   | T         |
   +-------+------------------+-----------+---------------------+------+-------+----------+----------+-----------+
   *

Add a systemtest
~~~~~~~~~~~~~~~~

If possible extend a systemtest already containing multiple scripts
by adding another :file:`testrunner-*` script to the test directory.

If this is not reasonable, a new systemtest is best created
by copying the existing systemtest
that matches the desired type of the new systemtest most.

The new test directory has to be listed
in :file:`systemtests/tests/CMakeLists.txt`.

Taking into concern system dependencies it could be necessary to disable
a test if the appropriate prerequisites for a test are not met. In this case
the test should be displayed as disabled when running the tests.

Adapt the test configuration and the :file:`testrunner` script to your requirements.

Directory Structures
~~~~~~~~~~~~~~~~~~~~

Running cmake in the systemtest subdirectory will create the tests in the
build tree that is party symmetrical to the source tree as you can see on the
next diagrams.

Directory Structure (Source)
''''''''''''''''''''''''''''

::

      systemtests/tests/acl/
      |-- etc
      |   `-- bareos            -- bareos config for this test
      |       |-- bareos-dir.d
      |       |   |-- catalog
      |       |   |-- client
      |       |   |-- console
      |       |   |-- director
      |       |   |-- fileset
      |       |   |-- job
      |       |   |-- jobdefs
      |       |   |-- messages
      |       |   |-- pool
      |       |   |-- profile
      |       |   `-- storage
      |       |-- bareos-fd.d
      |       |   |-- client
      |       |   |-- director
      |       |   `-- messages
      |       |-- bareos-sd.d
      |       |   |-- device
      |       |   |-- director
      |       |   |-- messages
      |       |   `-- storage
      |       |-- bconsole.conf.in
      |       `-- tray-monitor.d
      |           |-- client
      |           |-- director
      |           |-- monitor
      |           `-- storage
      `-- testrunner            -- the main script for this test

      or

      |-- test-cleanup          -- optional, falls back to ../../scripts/cleanup
      |-- test-setup            -- optional, falls back to ../../scripts/start_bareos.sh
      |-- testrunner-test1      -- script for test1
      |-- testrunner-test2      -- script for test2
      `-- ...                   -- more test scripts possible


Directory Structure (Build)
''''''''''''''''''''''''''''

::

      systemtests/tests/acl/
      |-- bin
      |-- etc
      |   `-- bareos
      |       |-- bareos-dir.d
      |       |   |-- additional_test_config
      |       |   |-- catalog
      |       |   |-- client
      |       |   |-- console
      |       |   |-- director
      |       |   |-- fileset
      |       |   |-- job
      |       |   |-- jobdefs
      |       |   |-- messages
      |       |   |-- pool
      |       |   |-- profile
      |       |   `-- storage
      |       |-- bareos-fd.d
      |       |   |-- client
      |       |   |-- director
      |       |   `-- messages
      |       |-- bareos-sd.d
      |       |   |-- device
      |       |   |-- director
      |       |   |-- messages
      |       |   `-- storage
      |       `-- tray-monitor.d
      |           |-- client
      |           |-- director
      |           |-- monitor
      |           `-- storage
      |-- log
      |-- piddir
      |-- python-modules
      |-- sbin
      |-- storage
      |-- tmp
      `-- working
