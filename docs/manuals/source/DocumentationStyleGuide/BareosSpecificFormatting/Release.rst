Releases
========

Functional Changes
------------------

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

.. literalinclude:: /DocumentationStyleGuide/example/sinceVersion-documentation-sphinx.rst.inc
   :language: none

This will be displayed as:

.. include:: /DocumentationStyleGuide/example/sinceVersion-documentation-sphinx.rst.inc


Note
~~~~

``:sinceVersion:`` is a legacy role. Sphinx itself offers

  * https://www.sphinx-doc.org/en/master/usage/restructuredtext/directives.html#directive-versionadded
  * https://www.sphinx-doc.org/en/master/usage/restructuredtext/directives.html#directive-versionchanged
  * https://www.sphinx-doc.org/en/master/usage/restructuredtext/directives.html#directive-deprecated

Until defined otherwise, you should use ``:sinceVersion:`` instead of these.


Reference to a Release
----------------------

To refer to a release, use::

    :ref:`bareos-<VERSION>`

Example:

  This refers to :ref:`Release 20.0.0`\ .

Output:

This refers to :ref:`Release 20.0.0`\ .


URLs
----

Release URL - Bareos.org
~~~~~~~~~~~~~~~~~~~~~~~~

If you want to display the release download URL from the Bareos.org site, the following formatting should be used:

.. \newcommand{\releaseUrlDownloadBareosOrg}[1]{%
   \url{http://download.bareos.org/bareos/release/#1/}%
   }

   Post Conversion Changes
   ${PERL} 's#\\releaseUrlDownloadBareosOrg\{(.*?)\}#//http://download.bareos.org/bareos/release/\1/#g' ${DESTFILE}

http://download.bareos.org/bareos/release/18.2/


Release URL - Bareos.com
~~~~~~~~~~~~~~~~~~~~~~~~

If you want to display the release download URL from the Bareos.com site, the following formatting should be used:

.. \newcommand{\releaseUrlDownloadBareosOrg}[1]{%
   \url{http://download.bareos.org/bareos/release/#1/}%
   }

   Post Conversion Changes
   ${PERL} 's#\\releaseUrlDownloadBareosCom\{(.*?)\}#//https://download.bareos.com/bareos/release/\1/#g' ${DESTFILE}


https://download.bareos.com/bareos/release/18.2/
