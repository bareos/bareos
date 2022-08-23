.. _documentationstyleguide/bareosspecificformatting/links:

Links
=====

bugs.bareos.org
---------------

If you want to reference an issue that is tracked on https://bugs.bareos.org/, the following formatting should be used:

.. literalinclude:: /DocumentationStyleGuide/example/mantis-issue.rst.inc
   :language: none

This will be displayed as:

.. include:: /DocumentationStyleGuide/example/mantis-issue.rst.inc

Github
------

Pull request
~~~~~~~~~~~~

Refer to a Github Pull request by:

.. literalinclude:: /DocumentationStyleGuide/example/github-pull-request.rst.inc
   :language: none

This will be displayed as:

.. include:: /DocumentationStyleGuide/example/github-pull-request.rst.inc


Commit
~~~~~~

Refer to a Github Commit by:

.. literalinclude:: /DocumentationStyleGuide/example/github-commit.rst.inc
   :language: none

This will be displayed as:

.. include:: /DocumentationStyleGuide/example/github-commit.rst.inc


Download Urls
-------------

The role ``:downloadUrls:`` expands to the download URL of the current build. It uses variables set in Jenkins for this.
Some downloads are provided from https://download.bareos.org/, others from https://download.bareos.com/. The role detects if a package will be published on download.bareos.org only or additionally on download.bareos.com.

If a release is on both download servers, it will combine them with **or**.

.. literalinclude:: /DocumentationStyleGuide/example/downloadUrls.rst.inc
   :language: none
   
will be displayed as:

https://download.bareos.org/bareos/release/21/Debian_11/ or https://download.bareos.com/bareos/release/21/Debian_11/

For the current build, the output is:

.. include:: /DocumentationStyleGuide/example/downloadUrls.rst.inc

If only the base release URL should be displayed, use **.** as parameter:

.. literalinclude:: /DocumentationStyleGuide/example/downloadUrls-none.rst.inc
   :language: none

The output will look like this:

.. include:: /DocumentationStyleGuide/example/downloadUrls-none.rst.inc
