Tests
=====

Unit Tests
----------
Bareos unit tests are usually written in C++ using Google Test.
The unit tests reside in ``core/src/tests``. If Google Test is available on your system, the tests are compiled during the normal build process of Bareos.
Unit tests can be run using ``make test`` or ``ctest``.

There are many theoretical approaches how to write unit tests. However, in general we use unit tests for software components such as classes and functions as well as for simple integration tests. A unit test should follow the F.I.R.S.T. principle (Fast, Independent, Repeatable, Self-Validating, Timely).

Adding a new C++ Test
~~~~~~~~~~~~~~~~~~~~~
To add a new test, you create your sourcefiles in ``core/src/tests`` and register the test in ``CMakeLists.txt`` in that directory. The easiest way is to copy an existing test sourcefile and the related lines in ``core/src/CMakeLists.txt``.

For general advice on how to use the Google Test framework see this documentation: `Googletest Primer <https://github.com/google/googletest/blob/main/docs/primer.md>`_

Adding tests in general
~~~~~~~~~~~~~~~~~~~~~~~
Unittests in other languages i.e. Python can be established using the add_test and set_property commands of cmake. The following cmake code adds a Python script to the test suite. The actual test in the source is disabled by default.

.. code-block:: shell-session
   :caption: core/src/plugins/filed/python/CMakeLists.txt

   add_test(
     NAME python-simple-test-example
     COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/test/simple-test-example.py
     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
   )
   set_property(
     #add current directory for documentation only
     TEST python-simple-test-example PROPERTY ENVIRONMENT PYTHONPATH=./
   )
   set_property(
     TEST python-simple-test-example PROPERTY DISABLED true
   )

.. code-block:: shell-session
   :caption: core/src/plugins/filed/python/test/simple-test-example.py

    #!/usr/bin/env python

    from sys import exit

    if __name__ == "__main__":
        print("--- Hello World ---")
        exit(0)

In this case only the return value of the script is evaluated: 0 for success and 1 for failure.
