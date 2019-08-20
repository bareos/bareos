Bareos Developer Notes
======================

This document is intended mostly for developers and describes how you
can contribute to the Bareos project and the general framework of making
Bareos source changes.

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
thank Kern and all contributors to Bacula and its documentation. We
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
A detailed description can be found in the chapter :ref:`git-workflow`.
If you don't want to sign up to GitHub, you can also send us your patches via E-Mail in **git format-patch** format to the `bareos-devel`_ mailing list.

Please make sure to use the Bareos `Automatic Sourcecode Formatting`_
Don’t forget any Copyrights and acknowledgments if it isn’t 100% your code.
Also, include the Bareos copyright notice that can be found in every source file.

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


Memory Leaks
------------

Use standard C++11 resource management (RAII and smart pointers) to prevent memory leaks in general. If you need to detect memory leaks, you can just use ``valgrind`` to do so.

Guiding Principles
------------------

All new code should be written in modern C++11 following the `Google C++ Style Guide`_ and the `C++ Core Guidelines`_.

We like simple rather than complex code, but complex code is still better than complicated code.

Currently there is still a lot of old C++ and C code in the code base and a lot of existing code violates our `do's`_ and `don'ts`_. Therefore our long-term goal is to modernize the code-base to make it easier to understand, easier to maintain and better approachable for new developers.

Automatic Sourcecode Formatting
-------------------------------

All C/C++ code should be formatted properly based on the principles mentioned above. Therefore we provide a configuration file for **clang-format** that contains all formatting rules. The filename is ".clang-format" and it is located in the root directory of the bareos repo.

The configuration file will be automatically found and used by clang-format:

.. code-block:: bash
  :caption: Example shell script

  #!/bin/sh

  #format one sourcecode file in-place
  clang-format -i ./core/src/dird/dird_conf.cc


Formatting exceptions
---------------------

For some parts of code it works best to hand-optimize the formatting. We sometimes do this for larger tables and deeply nested brace initialization. If you need to hand-optimize make sure you add **clang-format off** and **clang-format on** comments so applying **clang-format** on your source will not undo your manual optimization. Please apply common sense and use this exception sparingly.

Sourcecode Comments
-------------------

Use ``/* */`` for multi-line comments.
Use ``//`` for single-line comments.

Do's
----

- write modern C++11
- prefer simple code
- write unit tests for your code
- use RAII_ whenever possible
- honor `Rule of three`_/`Rule of five`/`Rule of zero`
- use ``std::string`` instead of ``char*`` for strings where possible
- use `fixed width integer types`_ if the size of your integer matters
- when in doubt always prefer the standard library over a custom implementation
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
------

avoid ``new``
  Starting with C++11 there are smart pointers like ``shared_ptr`` and ``unique_ptr``.
  To create a ``shared_ptr`` you should use ``make_shared()`` from the standard library.
  We provide a backport of C++14's ``make_unique()`` in ``include/make_unique.h`` to create a ``unique_ptr``.
  If possible use ``unique_ptr`` instead of ``shared_ptr``.

avoid ``delete``
  You should use the RAII_ paradigm, so cleanup is handled automatically.

don't transfer ownership of heap memory without move semantics
  No returning of raw pointers where the caller is supposed to free the resource.

don't use C++14 or later
  Currently we support platforms where the newest available compiler supports only C++11.

don't use C string functions
  If you can, use ``std::string`` and don't rely on C string functions.

don't use the bareos replacements for C string functions.
  These are deprecated.

avoid the ``edit_*()`` functions from ``edit.cc``
  Just use the appropriate format string.
  This will also avoid the temporary buffer that is required otherwise.

avoid pool memory allocation
  The whole allocation library with ``get_pool_memory()`` and friends do not mix with RAII, so we will try to remove them step by step in the future.
  Avoid in new code if possible.

