reST Overview
#############

.. highlight:: none

The outline for this chapter has been taken from
https://devguide.python.org/documenting/
and adapted and extended for our needs.

Use of whitespace
-----------------

All reST files use an indentation of 3 spaces; no tabs are allowed. The maximum line length is 80 characters for normal text, but tables, deeply indented code samples and long links may extend beyond that. Code example bodies should use normal Python 4-space indentation.

Make generous use of blank lines where applicable; they help group things together.


Sphinx naming
-------------

reST/Sphinx have some specific wording.

Role
~~~~

A role is an inline markup (see :ref:`RestOverview:Inline markup`\ ). Own roles can be created.
They are used inside other text structures.

Usage::

   :strong:`text`
   
Output:

   :strong:`text`

Directive
~~~~~~~~~

A directive is an Explicit Markup (see :ref:`RestOverview:Explicit markup`\ ). Own directives can be created.
Directives are written as a block.

Usage::

   .. directive:: arg1 arg2 ...
      :option1: value
      :option2: value
      :option5: value
      ...
      
      Multiline content,
      ...

``arg1, arg2, ...``
   Arguments. The last argument can contain spaces (depending on the directive implementation).
   
``:option1:``
   Options are optional.
      
Example:

.. literalinclude:: /example/code-block-bareosconfig.rst.inc
   :language: none

Output:

.. include:: /example/code-block-bareosconfig.rst.inc


Domain
~~~~~~

A domain is a collection of markup (reStructuredText directives and roles) to describe and link to objects belonging together.



Paragraphs
----------

The paragraph is the most basic block in a reST document.  Paragraphs are simply
chunks of text seperated by one or more blank lines.  As in Python, indentation
is significant in reST, so all lines of the same paragraph must be left-aligned
to the same level of indentation.


Inline markup
-------------

The standard reST inline markup is quite simple: use

* one asterisk: ``*text*`` for emphasis (italics),
* two asterisks: ``**text**`` or ``:strong:`text``` for strong emphasis (boldface), and
* backquotes: ````text```` for code samples.

If asterisks or backquotes appear in running text and could be confused with
inline markup delimiters, they have to be escaped with a backslash.

Be aware of some restrictions of this markup:

* it may not be nested,
* content may not start or end with whitespace: ``* text*`` is wrong,
* it must be seperated from surrounding text by non-word characters.  Use a
  backslash escaped space to work around that: ``thisis\ *one*\ word``.

These restrictions may be lifted in future versions of the docutils.

reST also allows for custom "interpreted text roles"', which signify that the
enclosed text should be interpreted in a specific way.  Sphinx uses this to
provide semantic markup and cross-referencing of identifiers, as described in
the appropriate section.  The general syntax is ``:rolename:`content```.


Lists, Definition Lists  and Quotes
-----------------------------------

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


Nested lists are possible, but be aware that they must be seperated from the
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


Literal block
-------------

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


Source Code
-----------

Source code will be formatted by the directive ``code-block``.

Example::

   .. code-block:: sh
      :caption: Titel
      
      Line 1
      Line 2
      ...

Output:

.. code-block:: sh
   :caption: Titel
      
   Line 1
   Line 2
   ...



Valid values for the highlighting language are:

  * ``none`` (no highlighting)
  * ``python`` (the default)
  * ``bareosconfig`` (Bareos configuration files or parts thereof)
  * ``bconsole`` (Bareos console (bconsole) sessions)
  * ``bareoslog`` (Bareos log files)
  * ``bareosmessage`` (Bareos messages)
  * ``sh`` (Shell scripts)
  * ``shell-session`` (Shell sessions)
  * ``dosbatch`` (DOS/Windows Batch file)
  * ``doscon`` (MSDOS sessions)  
  * ``cfg`` (Generic configuration file, mostly INI files)
  * ``sql`` (Generic SQL commands)
  * ``registry`` (Windows Registry files produced by regedit)
  * ``guess`` (let Pygments guess the lexer based on contents, only works with
    certain well-recognizable languages)
  * ``rest``
  * ``c``
  * ... and any other `lexer alias that Pygments supports
    <http://pygments.org/docs/lexers/>`_.

If the text resides in a seperate file, use::    

   .. literalinclude:: /include/config/backup-client1.cfg
      :language: bareosconfig

All included files should be located under :file:`/include`.

The beginning :file:`/` means, root directory of the documenation source directory.
Without it, the path is relative to the directory of the including file.

Configuration files should be located under :file:`/include/config/`\ .

Hyperlinks
----------

External links
~~~~~~~~~~~~~~

Use ```Link text <http://target>`_`` for inline web links.  If the link text
should be the web address, you don't need special markup at all, the parser
finds links and mail addresses in ordinary text.

Example:

.. literalinclude:: /example/urls.rst.inc
   :language: none

Output:

.. include:: /example/urls.rst.inc


Internal links
~~~~~~~~~~~~~~

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

.. warning::

   If the label is not defined immediately before a section title and no link text is given,
   ``:ref:`` will fail and not create a link at all.
   Instead it produces a warning and writes the parameter as plain text.

In the Bareos documentation we use the Sphinx extension **sphinx.ext.autosectionlabel**
with the parameter ``autosectionlabel_prefix_document = True``.

With this, all section titles do automatically create a section label.
This causes warnings when there are multiple sections with the same title.
To reduce the number of warnings, the parameter ``autosectionlabel_prefix_document = True`` is set,
This prefix each section label with the name (including the relative path from the top directory) of the document it is in, followed by a colon.

Example::

  :ref:`RestOverview:Cross-linking markup`
  
This will link to :ref:`RestOverview:Cross-linking markup`\ .



Sections
--------

Section headers are created by underlining (and optionally overlining) the section title with a punctuation character, at least as long as the text:

::

   #################
   This is a heading
   #################

Normally, there are no heading levels assigned to certain characters as the structure is determined from the succession of headings. However, this is our convention:

* ``#`` with overline, for parts
* ``=`` for chapters
* ``-``, for sections
* ``~``, for subsections
* ``^``, for subsubsections
* ``'``, for paragraphs

::

   ####
   Part
   ####
   
   Chapter
   =======

   Section
   -------

   Subsection
   ~~~~~~~~~~

   Subsubsection
   ^^^^^^^^^^^^^

   Paragraph
   '''''''''


This convention has be introduced from the conversion of thw original LaTex source to RST,
as :program:`pandoc` has created RST file with this section markers (except of parts, which are not created at all).

.. note::

   With RST, there is no leaving out a section level.
   If you write a chapter it is not possible to continue with a paragraph.
   Instead the next section must be of the type section.
   
   If you try to do it overwise (chapter 1 ``=`` -> paragraph ``'``),
   the ''paragraph'' is treated as a section.
   And if you continue by another chapter (in the same file) (chapter 2 ``=`` -> section ``-``),
   :program:`sphinx-build` got confused and at least produces a warning (`Title level inconsistent`)
   and possibly renders the result incorrectly.



Explicit Markup
---------------

“Explicit markup” is used in reST for most constructs that need special handling, such as footnotes, specially-highlighted paragraphs, comments, and generic directives.

An explicit markup block begins with a line starting with .. followed by whitespace and is terminated by the next paragraph at the same level of indentation. (There needs to be a blank line between explicit markup and normal paragraphs. This may all sound a bit complicated, but it is intuitive enough when you write it.)

Directives
~~~~~~~~~~

A directive is a generic block of explicit markup. Besides roles, it is one of the extension mechanisms of reST, and Sphinx makes heavy use of it.

Basically, a directive consists of a name, arguments, options and content. (Keep this terminology in mind, it is used in the next chapter describing custom directives.) Looking at this example,

::

  .. function:: foo(x)
                foo(y, z)
     :bar: no

   Return a line of text input from the user.

function is the directive name. It is given two arguments here, the remainder of the first line and the second line, as well as one option bar (as you can see, options are given in the lines immediately following the arguments and indicated by the colons).

The directive content follows after a blank line and is indented relative to the directive start.


Comments
~~~~~~~~

Every explicit markup block which isn’t a valid markup construct (like the footnotes above) is regarded as a comment.

However, it must have some text in the ".. " line, otherwise it is ignored, and content will be displayed (indented).



File/Directory/Path
-------------------

File and directories (or generally paths) are formated by ``:file:`` inline markup.

Backslahes (Windows paths) ``\`` have to written as ``\\``:

.. \newcommand{\directory}[1]{\path|#1|}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\directory\{(.*?)\}`#:file:`\1`#g'   ${DESTFILE}

.. code-block:: sh

   :file:`/var/lib/pgsql/data`

The formatting looks a follows:

.. literalinclude:: /example/file.rst.inc

Output:
  
.. include:: /example/file.rst.inc



Note
----

If you want to display a note, the following formatting should be used::

   .. note::

   It is important to ...

The output should look like this:

.. note::

   It is important to ...




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
   
   
Quote
------

.. \newcommand{\bquote}[1]{``#1''}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\bquote\{(.*?)\}`#:emphasis:`\1`#g' ${DESTFILE}

If you want to quote a certain text, the following formatting should be used::

   ''Application Managed''

The output should look like this:

''Application Managed''



Index
-----

``.. index::``
~~~~~~~~~~~~~~

Some roles and directives do already create indices automatically.

However, there is also an explicit directive available, to make the index more
comprehensive and enable index entries in documents where information is not
mainly contained in information units.

The directive is ``index`` and contains one or more index entries.  Each entry
consists of a type and a value, separated by a colon.

For example::

   .. index::
      single: execution; context
      triple: module; search; path

This directive contains five entries, which will be converted to entries in the
generated index which link to the exact location of the index statement (or, in
case of offline media, the corresponding page number).

Since index directives generate cross-reference targets at their location in the source,
it makes sense to put them before the thing they refer to – e.g. a heading, as in the example above.

The possible entry types are:

single
   Creates a single index entry.  Can be made a subentry by separating the
   subentry text with a semicolon (this notation is also used below to describe
   what entries are created).
pair
   ``pair: loop; statement`` is a shortcut that creates two index entries,
   namely ``loop; statement`` and ``statement; loop``.
triple
   Likewise, ``triple: module; search; path`` is a shortcut that creates three
   index entries, which are ``module; search path``, ``search; path, module``
   and ``path; module search``.
see
    ``see: entry; other`` creates an index entry that refers from ``entry`` to
    ``other``.
seealso
    Like ``see``, but inserts "see also" instead of "see".

You can mark up "main" index entries by prefixing them with an exclamation
mark.  The references to "main" entries are emphasized in the generated
index.  For example, if two pages contain ::

      .. index:: Python

and one page contains ::

      .. index:: ! Python

then the backlink to the latter page is emphasized among the three backlinks.
   
For index directives containing only "single" entries, there is a shorthand
notation::

   .. index:: BNF, grammar, syntax, notation

This creates four index entries.

``:index:``
~~~~~~~~~~~

While the :rst:dir:`index` directive is a block-level markup and links to the
beginning of the next paragraph, there is also a corresponding role that sets
the link target directly where it is used.

The content of the role can be a simple phrase, which is then kept in the
text and used as an index entry.  It can also be a combination of text and
index entry, styled like with explicit targets of cross-references.  In that
case, the "target" part can be a full entry as described for the directive
above.  For example::

    This is a normal reST :index:`paragraph` that contains several
    :index:`index entries <pair: index; entry>`.

.. note::

   The ``:index:`` role must contain text. This text will be printed and referenced by the index.
   
    
Converted (Latex2Sphinx) Indices
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Neither of ``.. index::`` nor ``:index:`` correspond well to a Latex ``\index``\ .

A Latex index will not be printed and the index will refer to the exact location.
In the Latex documentation indices have also been used inside of other structures (e.g. tables),
which prevents an easy automatic convertation to ``.. index::``.

To work around these problem, following substitution have been used::

  \index{Console!Command!restore}
  
is substitted by::

  :index:`[TAG=Console->Command->restore] <triple: Console; Command; restore>`

Disadvantages:
   The text ``[TAG=Console->Command->restore]`` will be shown.

When the Sphinx Bareos Manual is edited manually,
these ''TAGs'' can be replaced to some better fitting indices.

Note: ``triple`` is not required. The original behavior would be more like ``single``. However, we used triple to use some of the Sphinx advantages.


Image
-----

Image can be included by::

   .. image:: /include/images/bareos-webui-jobs.*
      :caption: Bareos WebUI: Job Overview
      :width: 80.0%
      
All images should be located in the :file:`/include/images/` directory.
Sphinx supports a number of image format.
With the file extension :file:`.*` Sphinx chooses the best one.
In general, images should be stored as SVG.


Table
-----

Sphinx offeres multiple methods to create tables, see https://www.sphinx-doc.org/en/2.0/usage/restructuredtext/basics.html#tables\ .

Choose whatever sucks least.
