Test Coverage Analysis
~~~~~~~~~~~~~~~~~~~~~~

Bareos can be built with coverage analysis support.
When running the resulting binaries coverage reports of the code that has been run will be generated.
You can then further analyse these reports.

Prerequisites
^^^^^^^^^^^^^
You should already know how to :ref:`build Bareos and run tests<build-for-systemtest>`.
To perform coverage analysis you will need a supported compiler (e.g. GCC) and some kind of analysis tool.
For this documentation we will rely on :command:`gcov` and :command:`gcovr`.

Building with coverage support
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

First, you have to add the ``-Dcoverage=yes`` to the list of CMake parameters to enable report generation.
If you have specified custom ``CFLAGS`` or ``CXXFLAGS``, these might interfere.
So it is probably a good idea not do set ``CFLAGS`` or ``CXXFLAGS`` until you have analysis running.

Building the binaries will still work the same.
The noticeable difference is, that next to every object-file (``*.o``) you will now find a notes-file (``*.gcno``).
That file describes the object's block graph and line-numbers.

Collecting coverage information
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When running the binaries, you will experience a slight slowdown in execution speed in comparison to a non-coverage build.
When a program runs it will create or update the count-data-files (``*.gcda``) next to every object-file (``*.o``).
As existing files are updated, running multiple programs or a single program multiple times will accumulate data.

If you want to reset the coverage data, you need to remove all the count-data-files.
You can then run the programs again to collect new data.

For convienience there is the custom target ``cov-clean`` that will clean out count-data-files and the reports.

Plotting analysis data
^^^^^^^^^^^^^^^^^^^^^^
With the notes-files and count-data-files in place, you can now plot your data.
If you have a build of the sources from ``~/bareos`` in ``~/bareos/build`` you can produce a HTML report in ``~/bareos/coverage`` as follows:

.. code-block:: shell-session

   user@host:~$ gcovr \
                --html-details ~/bareos/coverage/index.html \
                --root ~/bareos \
                ~/bareos/build

You can also run :command:`make cov-report` which will invoke the command shown above.

A complete example
^^^^^^^^^^^^^^^^^^

Our aim is to check the code coverage that the ConfigParser unittests provide.

.. code-block:: shell-session

   user@host ~$ git clone https://github.com/bareos/bareos.git
   Cloning into 'bareos'...
   remote: Enumerating objects: 113830, done.
   [...]
   user@host ~$ cd bareos
   user@host bareos$ mkdir build
   user@host bareos$ cd build
   user@host build$ cmake -Dclient-only=yes -Dcoverage=yes ..
   Entering /home/user/bareos
   -- Found Git: /usr/bin/git (found version "2.27.0")
   -- Using version information from Git
   [[...]]
   -- Configuring done
   -- Generating done
   -- Build files have been written to: /home/user/bareos/build

   user@host build$ cmake --build .
   [  0%] Building C object core/src/fastlz/CMakeFiles/bareosfastlz.dir/src/fastlzlib.c.o
   [  0%] Building CXX object core/src/plugins/filed/CMakeFiles/cephfs-fd.dir/cephfs/cephfs-fd.cc.o
   [  1%] Building C object core/src/lmdb/CMakeFiles/bareoslmdb.dir/mdb.c.o
   [[...]]
   [100%] Built target test_fd_plugins
   [100%] Linking CXX executable test_config_parser_fd
   [100%] Built target test_config_parser_fd

   user@host build$ ctest -R gtest:ConfigParser
   Test project /home/user/bareos/build

       Start 26: gtest:ConfigParser.test_console_config
       Start 27: gtest:ConfigParser.test_filed_config
   1/2 Test #26: gtest:ConfigParser.test_console_config ...   Passed    0.04 sec
   2/2 Test #27: gtest:ConfigParser.test_filed_config .....   Passed    0.04 sec

   The following tests passed:
           gtest:ConfigParser.test_console_config
           gtest:ConfigParser.test_filed_config

   100% tests passed, 0 tests failed out of 2

   Total Test time (real) =   0.05 sec

   user@host build$ make cov-report
   [100%] Creating HTML coverage report in /home/user/bareos/build/coverage
   [100%] Built target cov-report

   user@host build$ xdg-open coverage/index.html
   [[report will open in your browser]]
