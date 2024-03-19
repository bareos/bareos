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
is technically also a fork of the documentation created following the
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

Please make sure to use the Bareos `Automatic Sourcecode Formatting`_.
Don’t forget any Copyrights and acknowledgments if it isn’t 100% your code.
Also, include the Bareos copyright notice that can be found in every source file.

Bug Database
------------

We had an historical bug database located at https://bugs.bareos.org/\ , which is now
in read-only mode.
All new bug are now collected using the `github issues`_ component
The first thing is if you want to take over a bug, rather than just make
a comment, you should assign the bug to yourself. This helps other
developers know that you are the principal person to deal with the bug.
You can do so by going into the issue and use ``assign yourself`` in ``Assignees`` section.

Working with labels
~~~~~~~~~~~~~~~~~~~

Generally, the label ``bug`` and ``need triage`` are already set when a user add an issue.

The following non exhaustive list, explain what label can be used for sorting and
qualifying the issue.

- ``bug``: Something isn't working
- ``need triage``: No one has looked at this issue yet
- ``documentation``: Improvements or additions to documentation
- ``duplicate``: This issue or pull request already exists
- ``enhancement``: New feature or request
- ``good first issue``: Good for newcomers
- ``help wanted``: Extra attention is needed
- ``invalid``: This doesn't seem right
- ``question``: Further information is requested
- ``wontfix``: This will not be worked on

Usually issue related to a PR will be closed when the PR will be merged.
We also have a mechanism that will close any issue still open after a certain days
of non activity.
We generally close bug reports rather quickly, even without confirmation,
especially if we have run tests and can see that for us the problem is
fixed or not reproducible.
However, in doing so, it avoids misunderstandings if you leave a
note while you are closing the bug that says something to the following
effect:

      "We are closing this bug because... If for some reason, it does
      not fix your problem, please feel free to reopen it, or to open a new
      bug report describing the problem“.

We do not recommend that you attempt to edit any of the bug notes that
have been submitted, nor to delete them or make them private. In fact,
if someone accidentally makes a bug note private, you should ask the
reason and if at all possible (with his agreement) make the bug note
public.


If the user has not properly filled in most of the important fields
(platform, OS, Product Version, ...) please do not hesitate to politely ask him to do so.
The same applies to a support request (we answer only bugs), you might give
the user a tip, but please politely refer him to the manual, the `bareos-users`_
mailing list and maybe the `commercial support`_.

.. _bareos-users:       https://groups.google.com/forum/#!forum/bareos-users
.. _commercial support: https://www.bareos.com/product/support/
.. _github issues:      https://github.com/bareos/bareos/issues/


Reporting security issues
~~~~~~~~~~~~~~~~~~~~~~~~~

If you want to report a security-related problem, please take a look
at our `security policy`_.

.. _security policy: https://github.com/bareos/bareos/security/policy


Memory Leaks
------------

Use standard C++17 resource management (RAII and smart pointers) to prevent memory leaks
in general. If you need to detect memory leaks, you can just use ``valgrind`` to do so.

We also use sanitizers to detect memory leaks. To enable them, ensure you install
``libasan``, ``libubsan``, and ``libtsan`` and then enable sanitizers in the cmake
arguments with ``-DENABLE_SANITIZERS=YES``.


Guiding Principles
------------------

All new code should be written in modern C++17 following the `Google C++ Style Guide`_
and the `C++ Core Guidelines`_.

We like simple rather than complex code, but complex code is still better than
complicated code.

Currently there is still a lot of old C++ and C code in the code base and a lot of
existing code violates our `do's`_ and `don'ts`_. Therefore our long-term goal is to
modernize the code-base to make it easier to understand, easier to maintain and better
approachable for new developers.

Boy Scout Rule
--------------

Before submitting your pull request, please ensure that you have followed
the Boy Scout Rule:

> **"Leave the codebase better than you found it."**

The Boy Scout Rule encourages contributors to make small improvements or clean-ups
while working on a task without being explicitly requested. By following this rule,
we can collectively improve the quality, readability, and maintainability of the
codebase over time.

To apply the Boy Scout Rule, consider the following guidelines:

- **Clean up code**: Review the code you are modifying and check if there are any areas
  that could be improved, such as variable naming, code formatting, or removing
  unnecessary comments.
- **Refactor when appropriate**: If you find a block of code that you can refactor to
  improve its clarity or efficiency, take the initiative to make those changes.
- **Fix adjacent issues**: If you notice any related issues or bugs while working on
  your task, address them if it’s within the scope of your current changes.
  This proactive approach helps prevent future problems.
- **Update documentation**: If you modify a part of the codebase that affects the
  existing documentation, ensure that relevant documentation is updated accordingly.

Remember, the goal of the Boy Scout Rule is to foster continuous improvement and
create a more sustainable and maintainable codebase. By leaving the code better
than you found it, you contribute to the overall health and longevity of the project.


Usage of C++ Exceptions
-----------------------

We encourage developers to use C++ exceptions for the reason of simplicity and
readability of the code. In contrast to long if/else constructs C++ exceptions
are the state-of-the-art error handling mechanism of this programming language.
With exceptions it is easier to transport errors and information about errors
from the lowest stack level to the uppermost function.

In order to avoid memory leaks it is very advisable to use RAII or smart pointers
for memory allocation. With regard to legacy code it is important to understand how
memory management in detail works before throwing exceptions across several stack
levels and causing leaks by accident.

General advice, many examples and debunked myths about C++ Exceptions can be
found here: https://isocpp.org/wiki/faq/exceptions.


Automatic Sourcecode Formatting
-------------------------------

All C/C++ code should be formatted properly based on the principles mentioned above.
Therefore we provide a configuration file for **clang-format** that contains all
formatting rules. The filename is ".clang-format" and it is located in the root
directory of the bareos repo.

The configuration file will be automatically found and used by clang-format:

.. code-block:: bash
  :caption: Example shell script

  #!/bin/sh

  #format one sourcecode file in-place
  clang-format -i ./core/src/dird/dird_conf.cc

The Bareos project has bundled its automatic sourcecode formatting into one tool: ``bareos-check-sources``.
https://github.com/bareos/bareos/blob/master/devtools/pip-tools/README.md describes how to use it. In short:

.. code-block:: shell-session

   $ cd devtools/pip-tools
   $ pipenv sync
   $ pipenv shell
   (pip-tools)$ bareos-check-sources --since-merge --diff
   (pip-tools)$ bareos-check-sources --since-merge --modify


Formatting exceptions
---------------------

For some parts of code it works best to hand-optimize the formatting. We sometimes do
this for larger tables and deeply nested brace initialization. If you need to
hand-optimize make sure you add **clang-format off** and **clang-format on** comments
so applying **clang-format** on your source will not undo your manual optimization.
Please apply common sense and use this exception sparingly.


Sourcecode Comments
-------------------

Use ``/* */`` for multi-line comments.
Use ``//`` for single-line comments.

SQL queries
-----------

Developers will have to use SQL queries to get data from the database. When you navigate
the current code you might get a bit confused as there are different ways to do it:
First, there are direct queries written within the functions that need them. Second,
there are functions within the ``cats`` library containing ready made queries that get
called. And finally there are the generated SQL files within the ``cats/dml`` folder
that get invoked in certain situations.

Until we decide on a unified way to handle sql queries, we advise the following:

- If your queries are trivial, you can put them as a string within the code you are
  writing, make sure you wrap them in a function, and make sure it can be reused,
- If you are dealing with long and convoluted queries, write them within the ``cats/dml``
  folder and update the related files and enums.

Do's
----

- write modern C++17
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
  If possible use ``unique_ptr`` instead of ``shared_ptr``.

avoid ``delete``
  You should use the RAII_ paradigm, so cleanup is handled automatically.

don't transfer ownership of heap memory without move semantics
  No returning of raw pointers where the caller is supposed to free the resource.

don't use C string functions
  If you can, use ``std::string`` and don't rely on C string functions.

don't use the bareos replacements for C string functions.
  These are deprecated.

avoid the ``edit_*()`` functions from ``edit.cc``
  Just use the appropriate format string.
  This will also avoid the temporary buffer that is required otherwise.

avoid pool memory allocation
  The whole allocation library with ``get_pool_memory()`` and friends do not mix with
  RAII, so we will try to remove them step by step in the future.
  Avoid in new code if possible.
