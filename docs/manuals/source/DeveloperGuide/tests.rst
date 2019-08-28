Tests
=====

Unit Tests
----------
Bareos unit tests are usually written in C++ using Google Test.
The unit tests reside in ``core/src/tests`` and are compiled with the rest of Bareos if Google Test is available on your system.
Unit tests can be run using ``make test`` or ``ctest``.

Adding a new Test
~~~~~~~~~~~~~~~~~
To add a new test, you create your sourcefiles in ``core/src/tests`` and register the test in ``CMakeLists.txt`` in that directory.
There is also a helper script ``add_new_unit_test.sh`` that will setup a test from a template and register it in ``CMakeLists.txt``.
