General
#######

.. highlight:: none


Use of whitespace
-----------------

From https://devguide.python.org/documenting/#use-of-whitespace.

All reST files use an indentation of 3 spaces; no tabs are allowed. The maximum line length is 80 characters for normal text, but tables, deeply indented code samples and long links may extend beyond that. Code example bodies should use normal Python 4-space indentation.

Make generous use of blank lines where applicable; they help group things together.

(A sentence-ending period may be followed by one or two spaces; while reST ignores the second space, it is customarily put in by some users, for example to aid Emacs’ auto-fill mode.)


Paragraphs
----------

The paragraph is the most basic block in a reST document.  Paragraphs are simply
chunks of text separated by one or more blank lines.  As in Python, indentation
is significant in reST, so all lines of the same paragraph must be left-aligned
to the same level of indentation.


Inline markup
-------------

The standard reST inline markup is quite simple: use

* one asterisk: ``*text*`` for emphasis (italics),
* two asterisks: ``**text**`` for strong emphasis (boldface), and
* backquotes: ````text```` for code samples.

If asterisks or backquotes appear in running text and could be confused with
inline markup delimiters, they have to be escaped with a backslash.

Be aware of some restrictions of this markup:

* it may not be nested,
* content may not start or end with whitespace: ``* text*`` is wrong,
* it must be separated from surrounding text by non-word characters.  Use a
  backslash escaped space to work around that: ``thisis\ *one*\ word``.

These restrictions may be lifted in future versions of the docutils.

reST also allows for custom "interpreted text roles"', which signify that the
enclosed text should be interpreted in a specific way.  Sphinx uses this to
provide semantic markup and cross-referencing of identifiers, as described in
the appropriate section.  The general syntax is ``:rolename:`content```.


Lists and Quotes
----------------

List markup is natural: just place an asterisk at the start of a paragraph and
indent properly.  The same goes for numbered lists; they can also be
automatically numbered using a ``#`` sign::

   * This is a bulleted list.
   * It has two items, the second
     item uses two lines.

   1. This is a numbered list.
   2. It has two items too.

   #. This is a numbered list.
   #. It has two items too.


Nested lists are possible, but be aware that they must be separated from the
parent list items by blank lines::

   * this is
   * a list

     * with a nested list
     * and some subitems

   * and here the parent list continues

Definition lists are created as follows::

   term (up to a line of text)
      Definition of the term, which must be indented

      and can even consist of multiple paragraphs

   next term
      Description.


Paragraphs are quoted by just indenting them more than the surrounding
paragraphs.


Source Code
-----------

Literal code blocks are introduced by ending a paragraph with the special marker
``::``.  The literal block must be indented::

   This is a normal text paragraph. The next paragraph is a code sample::

      It is not processed in any way, except
      that the indentation is removed.

      It can span multiple lines.

   This is a normal text paragraph again.

The handling of the ``::`` marker is smart:

* If it occurs as a paragraph of its own, that paragraph is completely left
  out of the document.
* If it is preceded by whitespace, the marker is removed.
* If it is preceded by non-whitespace, the marker is replaced by a single
  colon.

That way, the second sentence in the above example's first paragraph would be
rendered as "The next paragraph is a code sample:".


Hyperlinks
----------

External links
^^^^^^^^^^^^^^

Use ```Link text <http://target>`_`` for inline web links.  If the link text
should be the web address, you don't need special markup at all, the parser
finds links and mail addresses in ordinary text.

Internal links
^^^^^^^^^^^^^^

Internal linking is done via a special reST role, see the section on specific
markup, :ref:`doc-ref-role`.


.. _doc-ref-role:

Cross-linking markup
--------------------

To support cross-referencing to arbitrary sections in the documentation, the
standard reST labels are "abused" a bit: Every label must precede a section
title; and every label name must be unique throughout the entire documentation
source.

You can then reference to these sections using the ``:ref:`label-name``` role.

Example::

   .. _my-reference-label:

   Section to cross-reference
   --------------------------

   This is the text of the section.

   It refers to the section itself, see :ref:`my-reference-label`.

The ``:ref:`` invocation is replaced with the section title.

Alternatively, you can reference any label (not just section titles)
if you provide the link text ``:ref:`link text <reference-label>```.



Sections
--------

From https://devguide.python.org/documenting/#sections.

Section headers are created by underlining (and optionally overlining) the section title with a punctuation character, at least as long as the text:

::

   =================
   This is a heading
   =================

Normally, there are no heading levels assigned to certain characters as the structure is determined from the succession of headings. However, for the Python documentation, here is a suggested convention:

* ``#`` with overline, for parts
* ``*`` with overline, for chapters
* ``=``, for sections
* ``-``, for subsections
* ``^``, for subsubsections
* ``"``, for paragraphs   



File/Directory/Path
-------------------

File and directories (or generally paths) are formated by ``:file:`` inline markup.

Backslahes (Windows paths) ``\`` have to written as ``\\``:

.. \newcommand{\directory}[1]{\path|#1|}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\directory\{(.*?)\}`#:file:`\1`#g'   ${DESTFILE}

.. code-block:: sh

   :file:`/var/lib/pgsql/data`

The output should look like this:
  
.. include:: include/file.rst

The formatting looks a follows:

.. literalinclude:: include/file.rst



Host name
---------

If you want to display a hostname, the following formatting should be followed:

.. \newcommand{\host}[1]{\path|#1|}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\host\{(.*?)\}`#:strong:`\1`#g' ${DESTFILE}
   
   Note: :strong:`XXX` is identical with **XXX**.

.. code-block:: sh

   :strong:`client1.example.com`

The output should look like this:

:strong:`client1.example.com`

.. list-table:: Common host names
   :header-rows: 1

   * - Host name
     - Description
   * - :strong:`bareos-dir.example.com`
     - Bareos Director host
   * - :strong:`bareos-sd.example.com`
     - Bareos Storage Daemon host, if only one Storage Daemon is used.
   * - :strong:`bareos-sd1.example.com`, :strong:`bareos-sd2.example.com`, ...
     - Bareos Storage Daemon host, if multiple Storage Daemons are used.
   * - :strong:`bareos-sd-tape.example.com`
     - Bareos Storage Daemon with a specific backend.
   * - :strong:`client1.example.com`, :strong:`client2.example.com`, ...
     - Bareos File Daemon

     
System User
-----------

If you want to display a system user name, the following formatting should be followed:

.. \newcommand{\user}[1]{\path|#1|}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\user\{(.*)\}`#**\1**#g'   ${DESTFILE}
   ${PERL} 's#\\user\{(.*)\}#**\1**#g'   ${DESTFILE}

.. code-block:: sh

   **root**

The output should look like this:

**root**



Operating System
----------------

If you want to display an operating system name, the following formatting should be used:

.. \newcommand{\os}[2]{\ifthenelse{\isempty{#2}}{%
    \path|#1|\index[general]{Platform!#1}%
    }{%
    \path|#1 #2|\index[general]{Platform!#1!#2}%
    }}

    Post Conversion Changes
    ${PERL} 's#\\os\{(.*?)\}\{}#:strong:`\1`#g' ${DESTFILE}
    ${PERL} 's#\\os\{(.*?)\}\{(.*?)}#:strong:`\1 \2`#g' ${DESTFILE}

.. code-block:: sh

   :strong:`Fedora 21`

The output should look like this:

:strong:`Fedora 21`



Software Package
----------------

If you want to display a software package name (mostly Linux), the following formatting should be used:

.. \newcommand{\package}[1]{\index[general]{Package!#1}\path|#1|\xspace}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\package\{(.*?)\}`#**\1**#g'   ${DESTFILE}
   ${PERL} 's#:raw-latex:`\\package\{(.*?)\}`#**\1**#g'   ${DESTFILE}

.. code-block:: sh

   **bareos-common**

The output should look like this:

**bareos-common**



Warning
-------

If you want to display a warning, the following formatting should be used:

.. \newcommand{\warning}[1]{Please note! \textit{#1}}

   Post Conversion Changes
   ${PERL0} 's#:raw-latex:`\\warning\{(.*?)\}`#\n.. warning:: \n  \1#ims' ${DESTFILE}
   ${PERL0} 's#\\warning\{(.*?)\}#\n.. warning:: \n  \1#ims'  ${DESTFILE}

.. code-block:: sh

   .. warning::

   This record ignores retention periods!

The output should look like this:

.. warning::

   This record ignores retention periods!
   
   
Windows Registry Key
--------------------

If you want to display a path where registry key is defined, this formatting should be used.

.. \newcommand{\registrykey}[1]{\path|#1|}

   Pre Conversion Changes
   perl -pe 's#registrykey#textbf#g'

.. code-block:: sh

   ``HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\BackupRestore\FilesNotToBackup``

The output should look like this:

``HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\BackupRestore\FilesNotToBackup``


Environment Variable
--------------------

If you want to display an (Unix) environment variable name, the following formating should be followed:


..   \newcommand{\variable}[1]{\path|#1|}
  perl -pe 's#\\variable\{\$#\\textbf\{\\\$#g'

.. code-block:: sh

  ``$HOSTNAME``
  
  ``$VERSION="18.2.5"``

The output should look like this:

``$HOSTNAME``
  
``$VERSION="18.2.5"``

Quote
------

If you want to quote a certain text, the following formatting should be used:

.. \newcommand{\bquote}[1]{``#1''}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\bquote\{(.*?)\}`#:emphasis:`\1`#g' ${DESTFILE}

.. code-block:: sh

   ''Application Managed''

The output should look like this:

''Application Managed''
