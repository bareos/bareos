Gotchas
=======

sphinx-build
------------

A normal execution of :program:`sphinx-build` will not show all warnings,
as it interprets only changed files.

To check for warnings, :program:`touch` all source reST files
or run :program:`make clean` before.

Not shown text
--------------

When using :ref:`RestOverview:Directives`, they are prepanded by the comment directives: ''.. ''

For this reason, any mispelled directives will not be shown at all (and not even are warning is shown during sphinx-build).

Similar things happen, is a space is forgotten or a space or line break too many is used.

Help: I'm not aware about a reasonalbe approach to avoid this in reST.


Undefined link targets
----------------------

For example, config directives can be defined and later on referenced by::

   :config:option:`...`

If the link target :ref:`BareosSpecificFormatting/BareosConfiguration:Reference to a Resource Directive` is unknown,
the displayed text will not alter.

There will not even be a warning during Sphinx build,
making it really hard to find typos in this kind of references.

TODO: example


toctree vs include
------------------

include
~~~~~~~

If a source file add another file via **include**,
it will be integrated into the original source file (and not handled as a seperate file).
For this reason it is important, that the included document must match the sectioning scheme of the including document.

To avoid :program:`sphinx-build` warnings, included files should use a different extension as the normal files (included by toctree).

  * File extention of the master file and files included by **toctree**: ``*.rst``
  * File extention of files included by **include**: ``*.rst.inc``

Background: As sphinx-build scans all matching files during the build process, it is aware about documents included by  toctree, but not about plain **include** statements. As result, it will see the content of the included file twice: one as included file as part of the source file, once by the scan process during the build. Sphinx will then complain about double defined heading and references.

  
toctree
~~~~~~~

* When using (nested) toctrees, make sure to define a section header above it. Otherwise the header line of the result document will show **<no title>** in the hierarchie of headings.

* All files included by toctree start at the same section level. If the sections of a document should start at a lower level, a nested toctree element is needed.

   * For this reason, an included document is not required to use the same sectioning scheme as the including document.

* **Important**: files names of files included via toctree are part of the URL of the document. That means, they are also part of the URL of the references/sections. And the URL will change when a section is moved to another place.



``:option:``
------------

Don't use the ``:options:`` role, as this tries to refers to a option definition, which we don't have.
Also, as we document different programs, using option would require to prefix it with the program name, see https://www.sphinx-doc.org/en/1.7/domains.html#directive-program.

This only applies to ``:options:`` without domain. We do you ``:config:option:`` in :ref:`BareosSpecificFormatting/BareosConfiguration:Bareos Configuration`.
