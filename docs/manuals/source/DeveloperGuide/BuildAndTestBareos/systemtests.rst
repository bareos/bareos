.. _BareosSystemtestsChapter:

Build Bareos for systemtests
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This following shell script will show how to build the **Bareos test-environment** from source.

.. code-block:: bash
  :caption: Example shell script

  #!/bin/sh

  mkdir bareos-local-tests
  cd bareos-local-tests
  git clone https://github.com/bareos/bareos.git

  mkdir build
  cd build

  # configure build environment
  cmake -Dpostgresql=yes -Dtraymonitor=yes ../bareos

  # build Bareos
  make

  # run system tests
  make test

Configure (cmake) build settings
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Bareos cmake configuration allows a lot of different defines to be set.
For the test-environment, we use the minimal defines required to run the tests.

When interested in the cmake defines for the Bareos production packages,
please refer to the corresponding build desciptions:

  * Debian Packages: `debian/rules <https://github.com/bareos/bareos/blob/master/core/debian/rules>`__
  * RPM Packages: `core/platforms/packaging/bareos.spec <https://github.com/bareos/bareos/blob/master/core/platforms/packaging/bareos.spec>`__


Using ccache (compiler cache)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Bareos can be built using ccache and using it will improve build times for repeated builds a lot.
Running cmake will autodetect and use ccache if it is available.
To get the most out of ccache, you should configure it to work correctly with Bareos.

base_dir
   Set this to a common directory where your checked out sources and cmake binary-dir live.
   Your homedirectoy is probably a good starting point.
   This setting lets ccache ignore the path to files below this ``base_dir``.
   This makes sure you will get a cache hit even if the path to the source files changes.
hash_dir
   By disabling this, the current working directory will be ignored.
   In case of cmake the working directory does not matter, so ignoring this should be safe and will improve cache hits
sloppiness = file_macro
   This makes sure the value that ``__FILE__`` expands to is ignored when caching.
   You may end up with binaries that contain other paths in ``__FILE__``, but Bareos only uses this to determine a relative path so this should not hurt.
   If you're using a modern compiler that supports ``-ffile-prefix-map`` this should not be required anymore.

.. code-block:: ini
  :caption: Example ccache.conf

  base_dir = /path/to/common/topdir
  hash_dir = false
  sloppiness = file_macro

Systemtests
~~~~~~~~~~~

Systemtests use the locally compiled version of the bareos binaries
and run tests on them. Preparations also have been made to run the
tests on installed binaries (originating from packages).

The tests were designed to be completely independent, so that they
can be run in parallel without interfering with each other.

The Bareos Systemtest approach is intended to substitute the older :ref:`BareosRegressionTestingChapter` approach.


Run all system tests
^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell-session
   :caption: List available ctests

   user@host:~$ cd bareos-local-tests/build
   user@host:~/bareos-local-tests/build$ ctest --show-only
   Test project ~/bareos-local-tests/build
     Test  #1: system:backup-bareos-test
     Test  #2: system:backup-bareos-passive-test
     Test  #3: system:multiplied-device-test
     Test  #4: system:virtualfull
     Test  #5: system:virtualfull-bscan
   ...

.. code-block:: shell-session
   :caption: Run all system tests

   user@host:~$ cd bareos-local-tests/build
   user@host:~/bareos-local-tests/build$ make test

   Running tests...
   Test project ~/bareos-local-tests/build
         Start  1: system:backup-bareos-test
    1/11 Test  #1: system:backup-bareos-test ...........   Passed   15.81 sec
         Start  2: system:backup-bareos-passive-test
   ...

Instead of using :command:`make test`, :command:`ctest` can directly invoked.
This offers some advantages, like being able to run multiple tests in parallel with
:command:`ctest -j <number of parallel tests>`.
Only jobs with names matching a certain regular expression can be run with
:command:`ctest -R`, and verbose test output can be enabled with :command:`ctest -V`.
Please refer to the ctest documentation.

Run a single system test
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: shell-session
   :caption: Run a single system test by ctest

   user@host:~$ cd bareos-local-tests/build
   user@host:~/bareos-local-tests/build$ ctest --verbose --tests-regex backup-bareos-test
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
       Start 1: system:backup-bareos-test

   1: Test command: ~/bareos-local-tests/build/systemtests/tests/backup-bareos-test/testrunner
   1: Test timeout computed to be: 1500
   1: creating database (postgresql)
   1: running ~/bareos-local-tests/build/systemtests/scripts/setup
   1:
   1:
   1: === backup-bareos-test: starting at 16:09:46 ===
   1: =
   1: =
   1: =
   1: =
   1: === backup-bareos-test: OK at 16:09:56 ===
   1/1 Test #1: system:backup-bareos-test ........   Passed   10.90 sec

   The following tests passed:
           system:backup-bareos-test

   100% tests passed, 0 tests failed out of 1

   Total Test time (real) =  10.91 sec

or change into a test directory and run :command:`testrunner` directly:

.. code-block:: shell-session
   :caption: Run a single system test by testrunner

   user@host:~$ cd bareos-local-tests/build
   user@host:~/bareos-local-tests/build$ cd tests/backup-bareos-test
   user@host:~/bareos-local-tests/build/tests/backup-bareos-test$ ./testrunner
   creating database (postgresql)
   running ~/bareos-local-tests/build/systemtests/scripts/setup


   === backup-bareos-test: starting at 15:03:20 ===
   =
   =
   =
   =
   === backup-bareos-test: OK at 15:03:35 ===


For verbose output, set ``export REGRESS_DEBUG=1`` before running :command:`testrunner`.


The test environment persists after runnning a test.
So to further debug a problem,
the Bareos daemons can be started again,
and a :command:`bconsole` session can be used to retrieve information:


.. code-block:: shell-session
   :caption: Doing manual tests in a test-environment

   user@host:~$ cd bareos-local-tests/build
   user@host:~/bareos-local-tests/build$ cd tests/backup-bareos-test
   user@host:~/bareos-local-tests/build/tests/backup-bareos-test$ bin/bareos status
   bareos-dir is stopped
   bareos-sd is stopped
   bareos-fd is stopped
   user@host:~/bareos-local-tests/build/tests/backup-bareos-test$ bin/bareos start
   Starting the  Storage daemon
   Starting the  File daemon
   Starting the  Director daemon
   Checking Configuration and Database connection ...
   user@host:~/bareos-local-tests/build/tests/backup-bareos-test$ bin/bareos status
   bareos-dir (pid 2782) is running...
   bareos-sd (pid 2761) is running...
   bareos-fd (pid 2770) is running...
   user@host:~/bareos-local-tests/build/tests/backup-bareos-test$ bin/bconsole
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
^^^^^^^^^^^^^^^^

To add a systemtest it is advisable to copy one of the existing systemtests
that matches the desired type of the new systemtest most.

The new test has to be listed in the CMakeLists.txt file in the systemtests
folder.

Taking into concern system dependencies it could be neccessary to disable
a test if the appropriate prerequisites for a test are not met. In this case
the test should be displayed as disabled when running the tests.

Therefore CMakeLists.txt contains two lists of tests. One for enabled tests
which will run properly and another for disabled tests.

Each test has a script named *testrunner* which contains all code to run a
systemtest.

Directory Structures
^^^^^^^^^^^^^^^^^^^^

Running cmake in the systemtest subdirectory will create the tests in the
build tree that is party symmetrical to the source tree as you can see on the
next diagrams.

Directory Structure (Source)
''''''''''''''''''''''''''''

::

      systemtests/tests/backup-bareos-test/
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

Directory Structure (Build)
''''''''''''''''''''''''''''

::

      systemtests/tests/backup-bareos-test/
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
