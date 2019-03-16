Bareos Versions
===============

If the documenation wants to point out,
that a feature has been added
or some functionality changed in a specific version,
use the following role:

If you want to point out a limitation in the Bareos functionality, the following formatting should be used::

   :sinceVersion:`<version>: <Summary>`

version
   Bareos version number, e.g. ''18.2.5''.

Summary
   A short summary.
   
This role will only display ''Version >= <version>''. The <Summary> will appear only in the index.

The index will contain entries:

* bareos-<version>

  * Summary
   
and

* Summary

  * bareos-<version>

Example:

.. literalinclude:: /example/sinceVersion-configuration-scheme.rst.inc
   :language: none
   
This will be displayed as:

.. include:: /example/sinceVersion-configuration-scheme.rst.inc


Note
----

``:sinceVersion:`` is a legacy role. Sphinx itself offer

  * https://www.sphinx-doc.org/en/master/usage/restructuredtext/directives.html#directive-versionadded
  * https://www.sphinx-doc.org/en/master/usage/restructuredtext/directives.html#directive-versionchanged
  * https://www.sphinx-doc.org/en/master/usage/restructuredtext/directives.html#directive-deprecated
  
Until defined otherwise, you should use ``:sinceVersion:`` instead of these.
   
