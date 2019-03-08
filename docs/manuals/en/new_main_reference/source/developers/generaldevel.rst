Bareos Developer Notes
======================

This document is intended mostly for developers and describes how you
can contribute to the Bareos project and the general framework of making
Bareos source changes.


We can use blockdiag:

.. blockdiag::

    blockdiag admin {
      top_page -> config -> config_edit -> config_confirm -> top_page;
    }

.. blockdiag::

   blockdiag {
     // standard node shapes
     box [shape = box];
     square [shape = square];
     roundedbox [shape = roundedbox];
     dots [shape = dots];

     circle [shape = circle];
     ellipse [shape = ellipse];
     diamond [shape = diamond];
     minidiamond [shape = minidiamond];

     note [shape = note];
     mail [shape = mail];
     cloud [shape = cloud];
     actor [shape = actor];

     beginpoint [shape = beginpoint];
     endpoint [shape = endpoint];

     box -> square -> roundedbox -> dots;
     circle -> ellipse -> diamond -> minidiamond;
     note -> mail -> cloud -> actor;
     beginpoint -> endpoint;

     // node shapes for flowcharts
     condition [shape = flowchart.condition];
     database [shape = flowchart.database];
     terminator [shape = flowchart.terminator];
     input [shape = flowchart.input];

     loopin [shape = flowchart.loopin];
     loopout [shape = flowchart.loopout];

     condition -> database -> terminator -> input;
     loopin -> loopout;
   }



We can use actdiag:

.. actdiag::

   actdiag {
     write -> convert -> image

     lane user {
        label = "User"
        write [label = "Writing reST"];
        image [label = "Get diagram IMAGE"];
     }
     lane actdiag {
        convert [label = "Convert reST to Image"];
     }
   }

We can use seqdiag:


.. seqdiag::
   :desctable:

   seqdiag {
      A -> B -> C;
      A [description = "browsers in each client"];
      B [description = "web server"];
      C [description = "database server"];
   }

.. seqdiag::

   seqdiag {
     # define order of elements
     # seqdiag sorts elements by order they appear
     browser; database; webserver;

     browser  -> webserver [label = "GET /index.html"];
     browser <-- webserver;
     browser  -> webserver [label = "POST /blog/comment"];
                 webserver  -> database [label = "INSERT comment"];
                 webserver <-- database;
     browser <-- webserver;
   }


We can use nwdiag:

.. nwdiag::

   {
     network dmz {
         address = "210.x.x.x/24"

         web01 [address = "210.x.x.1"];
         web02 [address = "210.x.x.2"];
     }
     network internal {
         address = "172.x.x.x/24";

         web01 [address = "172.x.x.1"];
         db01;
         app01;
     }
   }

.. nwdiag::

   nwdiag {
     inet [shape = cloud];
     inet -- router;

     network {
       router;
       web01;
       web02;
     }
   }

We can use plantuml:

.. uml::
  :caption: This is an example UML diagram

  Dir -> Fd: Hello Fd
  Dir <- Fd: Hello Dir





History
-------


Bareos is a fork of the open source project Bacula version 5.2. In 2010
the Bacula community developer Marco van Wieringen started to collect
rejected or neglected community contributions in his own branch. This
branch was later on the base of Bareos and since then was enriched by a
lot of new features.

This documentation also bases on the original Bacula documentation, it
is technically also a fork of the documenation created following the
rules of the GNU Free Documentation License.

Original author of Bacula and it’s documentation is Kern Sibbald. We
thank Kern and all contributors to Bacula and it’s documentation. We
maintain a list of contributors to Bacula (until the time we’ve started
the fork) and Bareos in our AUTHORS file.

Contributions
-------------

Contributions to the Bareos project come in many forms: ideas,
participation in helping people on the `bareos-users`_ email list,
packaging Bareos binaries for the community, helping improve the
documentation, and submitting code.

Patches
-------

Patches should be sent as a pull-request to the master branch of the GitHub repository.
To do so, you will need an account on GitHub.
If you don't want to sign up to GitHub, you can also send us your patches via E-Mail in **git format-patch** format to the `bareos-devel`_ mailing list.

Please make sure to use the Bareos `formatting standards`_. 
Don’t forget any Copyrights and acknowledgments if it isn’t 100% your code.
Also, include the Bareos copyright notice that can be found in every source file.

If you plan on doing significant development work over a period of time, after having your first patch reviewed and approved, you will be eligible for GitHub write access so that you can commit your changes directly into branches of the main GitHub repository.

Commit message guideline
~~~~~~~~~~~~~~~~~~~~~~~~
Start with a subject on a single line.
If your commit changes a specific component of bareos try to mention it at the start of the subject.

Next comes an empty line.

If your commit fixes an existing bug, add a line in the format ``Fixes #12345: The exact title of the bug you fix.``.
After this you can just write your detailed commit information.

We strongly encourage you to keep the subject down to 50 characters and to wrap your text at the 50 character boundary.
However, this is by no means enforced.

::

  lib: do a sample commit

  Fixes #12345: Really nasty library needs a sample commit.

  This patch fixes a bug in one of the libraries.
  Before we applied this specific change, the 
  library was completely okay, but in desperate
  need of a sample commit.



Bug Database
------------

We have a bug database which is at https://bugs.bareos.org.

The first thing is if you want to take over a bug, rather than just make
a note, you should assign the bug to yourself. This helps other
developers know that you are the principal person to deal with the bug.
You can do so by going into the bug and clicking on the **Update Issue**
button. Then you simply go to the **Assigned To** box and select your
name from the drop down box. To actually update it you must click on the
**Update Information** button a bit further down on the screen, but if
you have other things to do such as add a Note, you might wait before
clicking on the **Update Information** button.

Generally, we set the **Status** field to either acknowledged,
confirmed, or feedback when we first start working on the bug. Feedback
is set when we expect that the user should give us more information.

Normally, once you are reasonably sure that the bug is fixed, and a
patch is made and attached to the bug report, and/or in the Git, you can
close the bug. If you want the user to test the patch, then leave the
bug open, otherwise close it and set **Resolution** to **Fixed**. We
generally close bug reports rather quickly, even without confirmation,
especially if we have run tests and can see that for us the problem is
fixed. However, in doing so, it avoids misunderstandings if you leave a
note while you are closing the bug that says something to the following
effect: We are closing this bug because... If for some reason, it does
not fix your problem, please feel free to reopen it, or to open a new
bug report describing the problem“.

We do not recommend that you attempt to edit any of the bug notes that
have been submitted, nor to delete them or make them private. In fact,
if someone accidentally makes a bug note private, you should ask the
reason and if at all possible (with his agreement) make the bug note
public.

If the user has not properly filled in most of the important fields (platform, OS, Product Version, ...) please do not hesitate to politely ask him to do so.
The same applies to a support request (we answer only bugs), you might give the user a tip, but please politely refer him to the manual, the `bareos-users`_ mailing list and maybe the `commercial support`_.

.. _bareos-users:       https://groups.google.com/forum/#!forum/bareos-users
.. _commercial support: https://www.bareos.com/en/Support.html

Developing Bareos
-----------------

Compiling
~~~~~~~~~

There are several ways to locally compile (and install) Bareos

Option one: Local creation of Debian-packages from the cloned sourcecode
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Cloning and initial steps
'''''''''''''''''''''''''

::

    sudo apt-get install git dpkg-dev devscripts fakeroot
    git clone https://github.com/bareos/bareos
    cd bareos/core
    dpkg-checkbuilddeps

You then need to install all packages that dpkg-checkbuilddeps lists as
required

Changelog preparation
'''''''''''''''''''''

::

    # prepares the changelog for Debian, only neccesary on initial install
    cp -a platforms/packaging/bareos.changes debian/changelog
    # You need to manually change the version number in debian/changelog.
    # gets current version number from src/include/version.h and includes it
    VERSION=$(sed -n -r 's/#define VERSION "(.*)"/\1/p'  src/include/version.h)
    dch -v $VERSION "Switch version number"

Creation of Debian-packages
'''''''''''''''''''''''''''

::

    # creates Debian-packages and stores them in ..
    fakeroot debian/rules binary

Option two: Compiling and installing Bareos locally on Debian
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**Disclaimer: This process makes use of development-oriented compiler
flags. If you want to compile Bareos to be similar to a Bareos compiled
with production intent, please refer to section “Using the same flags as
in production”.**

.. _cloning-and-initial-steps-1:

Cloning and initial steps
'''''''''''''''''''''''''

::

    git clone https://github.com/bareos/bareos
    cd bareos/core

Compiling and locally installing Bareos
'''''''''''''''''''''''''''''''''''''''

::

    #!/bin/bash
    export CFLAGS="-g -Wall"
    export CXXFLAGS="-g -Wall"

    # specifies the directory in which bareos will be installed
    DESTDIR=~/bareos

    mkdir $DESTDIR

    CMAKE_BUILDDIR=cmake-build

    mkdir ${CMAKE_BUILDDIR}
    pushd ${CMAKE_BUILDDIR}

    # In a normal installation, Dbaseport=9101 is used. However, for testing purposes, we make use of port 8001.
    cmake  .. \
      -DCMAKE_VERBOSE_MAKEFILE=ON \
      -DBUILD_SHARED_LIBS:BOOL=ON \
      -Dbaseport=8001 \
      -DCMAKE_INSTALL_PREFIX:PATH=$DESTDIR \
      -Dprefix=$DESTDIR \
      -Dworkingdir=$DESTDIR/var/ \
      -Dpiddir=$DESTDIR/var/ \
      -Dconfigtemplatedir=$DESTDIR/lib/defaultconfigs \
      -Dsbin-perm=755 \
      -Dpython=yes \
      -Dsmartalloc=yes \
      -Ddisable-conio=yes \
      -Dreadline=yes \
      -Dbatch-insert=yes \
      -Ddynamic-cats-backends=yes \
      -Ddynamic-storage-backends=yes \
      -Dscsi-crypto=yes \
      -Dlmdb=yes \
      -Dndmp=yes \
      -Dipv6=yes \
      -Dacl=yes \
      -Dxattr=yes \
      -Dpostgresql=yes \
      -Dmysql=yes \
      -Dsqlite3=yes \
      -Dtcp-wrappers=yes \
      -Dopenssl=yes \
      -Dincludes=yes

    popd

You will now have to do the following:

::

    # This path corresponds to the $CMAKE_BUILDDIR variable. If you used a directory other than the default ```cmake-build```, you will have to alter the path accordingly.
    cd cmake-build
    make
    make install

Configuring Bareos
''''''''''''''''''

Before you can successfully use your local installation, it requires
additional configuration.

::

    # You have to move to the local installation directory. This path corresponds to the $DESTDIR variable. If you used a directory other than the default ```~/baroes```, you will have to alter the path accordingly.
    cd ~/bareos
    # copy configuration files, only neccesary on initial install
    cp -a lib/defaultconfigs/* etc/bareos/

You will have to replace
``dbdriver = "XXX_REPLACE_WITH_DATABASE_DRIVER_XXX"`` with ``sqlite3``
or other. The file can be found at
``etc/bareos/bareos-dir.d/catalog/MyCatalog.conf``

::

    # sets up server
    # creates bareos database (requires sqlite3 package in case of sqlite3 installation)
    lib/bareos/scripts/create_bareos_database
    lib/bareos/scripts/make_bareos_tables
    lib/bareos/scripts/grant_bareos_privileges

Launching your local Bareos installation
''''''''''''''''''''''''''''''''''''''''

::

    # launches director in debug mode in foreground
    sbin/bareos-dir -f -d100
    # displays status of bareos daemons
    lib/bareos/scripts/bareos status
    # The start command launches both the daemons and the director, if not already launched. We launched the director seperately for debugging purposes.
    lib/bareos/scripts/bareos start '
    # launches bconsole to connect to director
    bin/bconsole

Using the same flags as in production
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You can find the compilation flags that are used in production in the
following locations:

Debian-packages
'''''''''''''''

You can find the flags used for compiling for Debian in
`debian/rules <https://github.com/bareos/bareos/blob/master/core/debian/rules>`__.

RPM-packages
''''''''''''

You can find the flags used for compiling rpm-packages in
`core/platforms/packaging/bareos.spec <https://github.com/bareos/bareos/blob/master/core/platforms/packaging/bareos.spec>`__.

Debugging
~~~~~~~~~

Probably the first thing to do is to turn on debug output.

A good place to start is with a debug level of 20 as in **./startit
-d20**. The startit command starts all the daemons with the same debug
level. Alternatively, you can start the appropriate daemon with the
debug level you want. If you really need more info, a debug level of 60
is not bad, and for just about everything a level of 200.

Using a Debugger
~~~~~~~~~~~~~~~~

If you have a serious problem such as a segmentation fault, it can
usually be found quickly using a good multiple thread debugger such as
**gdb**. For example, suppose you get a segmentation violation in
**bareos-dir**. You might use the following to find the problem:

::
  <start the Storage and File daemons>
  cd dird
  gdb ./bareos-dir
  run -f -s -c ./dird.conf
  <it dies with a segmentation fault>

The **-f** option is specified on the **run** command to inhibit **dird** from going into the background.
You may also want to add the **-s** option to the run command to disable signals which can potentially interfere with the debugging.

As an alternative to using the debugger, each **Bareos** daemon has a
built in back trace feature when a serious error is encountered. It
calls the debugger on itself, produces a back trace, and emails the
report to the developer. For more details on this, please see the
chapter in the main Bareos manual entitled “What To Do When Bareos
Crashes (Kaboom)”.

Memory Leaks
~~~~~~~~~~~~
Most of Bareos still uses SmartAlloc.
This tracks memory allocation and allows you to detect memory leaks.
However, newer code should not use SmartAlloc, but use standard C++11 resource management (RAII).
If you need to detect memory leaks, you can just use ``valgrind`` to do so.

Guiding Principles
~~~~~~~~~~~~~~~~~~
All new code should be written in modern C++11 following the `Google C++ Style Guide`_ and the `C++ Core Guidelines`_.

We consider simple better than complex, but complex code is still better than complicated code.

Currently there is still a lot of old C++ and C code in the code base and a lot of existing code violates our `do's`_ and `don'ts`_.
Our long-term goal is to modernize the code-base to make it easier to understand, easier to maintain and better approachable for new developers.

Formatting Standards
~~~~~~~~~~~~~~~~~~~~

We find it very hard to adapt to different styles of code formatting, so we agreed on a set of rules.
Instead of describing these in a lenghty set of rules, we provide a configuration file for **clang-format** in our repository that we use to format the code.
All code should be reformatted using **clang-format** before committing.

For some parts of code it works best to hand-optimize the formatting.
We sometimes do this for larger tables and deeply nested brace initialization.
If you need to hand-optimize make sure you add **clang-format off** and **clang-format on** comments so applying **clang-format** on your source will not undo your manual optimization.
Please apply common sense and use this exception sparingly.

Prefer ``/* */`` for code comments.
You can use ``//`` for single-line comments.

Do's
~~~~
- write modern C++11
- prefer simple code
- write unit tests for your code
- use RAII_ whenever possible
- honor `Rule of three`_/`Rule of five`/`Rule of zero`
- use ``std::string`` instead of ``char*`` for strings where possible
- use `fixed width integer types`_ if the size of your integer matters
- when in doubt always prefer the standard library above a custom implementation
- follow the `Google C++ Style Guide`_
- follow the `C++ Core Guidelines`_
- get in touch with us on the `bareos-devel`_ mailing list

.. _RAII:                      https://en.cppreference.com/w/cpp/language/raii
.. _Rule of three:             https://en.cppreference.com/w/cpp/language/rule_of_three
.. _fixed width integer types: https://en.cppreference.com/w/cpp/types/integer
.. _Google C++ Style Guide:    https://google.github.io/styleguide/cppguide.html
.. _C++ Core Guidelines:       http://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines
.. _bareos-devel:              https://groups.google.com/forum/#!forum/bareos-devel

Don'ts
~~~~~~
avoid ``new``
  We provide a backport of C++14's ``make_unique()`` in ``make_unique.h``.
  Whereever possible this should be used instead of ``new``.

avoid ``delete``
  Cleanup should be handled automatically with RAII.

don't transfer ownership of heap memory without move semantics
  No returning of raw pointers where the caller is supposed to free()

don't use C++14 or later
  Currently we support platforms where the newest available compiler supports only C++11.

don't use C string functions
  If you can, use ``std::string`` and don't rely on C's string functions.
  If you can't use the bareos replacement for the string function.
  This is usually just prefixed with a "b".
  These replacements will play nicely with the (now deprecated) smart allocator.

avoid the ``edit_*()`` functions from ``edit.cc``
  Just use the appropriate format string.
  This will also avoid the temporary buffer that is required otherwise.

don't subclass ``SmartAlloc``
  It forces the use of ancient memory management methods and severely limits the capabilities of your class

avoid smart allocation
  The whole smart allocation library with ``get_pool_memory()``, ``sm_free()`` and friends does not mix with RAII, so we will try to remove them step by step in the future.
  Avoid in new code if possible.
  
