Trouble
=======

toctree vs include
------------------

include
"""""""

If a source file add another file via **include**,
it will be integrated into the original source file (and not handled as a seperate file).

As sphinx-build scans all matchin files during the build process, it takes toctree inclusion into account, but not plain **include** statements. As result, it will see the content of the included file twice: one as included file as part of the source file, once by the scan process during the build. Sphinx will then complain about double defined heading and references.

To avoid this, included files should use a different extension as the normal files (included by toctree).

  * Extention of the master file and files included by **toctree**: ``*.rst``
  * Extention of files included by **include**: ``*.rst.inc``

..

   does include keep the document structre as toctree does?


toctree
"""""""

* When using (nested) toctrees, make sure to define a section header above it. Otherwise the header line of the result document will show **<no title>** in the hierarchie of headings.

* All files included by toctree start at the same section level. If the sections of a document should start at a lower level, a nested toctree element is needed.

* **Important**: files names of files included via toctree are part of the URL of the document. That means, they are also part of the URL of the references/sections. And the URL will change when a section is moved to another place. Therefore inclusion via **toctree should be avoided**!
