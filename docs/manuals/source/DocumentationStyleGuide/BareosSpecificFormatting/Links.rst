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

.. literalinclude:: /DocumentationStyleGuide/example/downloadUrls.rst.inc
   :language: none
   
For the current build, the output is:

.. include:: /DocumentationStyleGuide/example/downloadUrls.rst.inc

If only the base release URL should be displayed, use **.** as parameter:

.. literalinclude:: /DocumentationStyleGuide/example/downloadUrls-none.rst.inc
   :language: none

The output will look like this:

.. include:: /DocumentationStyleGuide/example/downloadUrls-none.rst.inc
