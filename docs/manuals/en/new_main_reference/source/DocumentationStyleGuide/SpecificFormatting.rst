Specific Formatting
===================

Configuration
-------------

Configuration File
~~~~~~~~~~~~~~~~~~

Configuration file (snippets) should be formatted as

.. literalinclude:: /DocumentationStyleGuide/example/code-block-cfg-mysql.rst.inc
   :language: none
   
This will be displayed as:

.. include:: /DocumentationStyleGuide/example/code-block-cfg-mysql.rst.inc

The code-block highlighting scheme should be suitable for all kind of configuration files, especially ini files. However, Bareos configuration resources, we use our own scheme,
see :ref:`DocumentationStyleGuide/BareosSpecificFormatting/BareosConfiguration:Bareos Configuration Resource`.

.. seealso:: :ref:`DocumentationStyleGuide/BareosSpecificFormatting/BareosConfiguration:Bareos Configuration Resource`


Configuration Directive
~~~~~~~~~~~~~~~~~~~~~~~

.. \newcommand{\configline}[1]{\path|#1|}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\configline\{(.*?)\}`#:strong:`\1`#g'  ${DESTFILE}

If you want to display a refer to a single configuration option, the following formatting should be used::

   ``host = s3.amazonaws.com``

The output should look like this:

``host = s3.amazonaws.com``

However, for a Bareos configuration directive with value use :ref:`DocumentationStyleGuide/BareosSpecificFormatting/BareosConfiguration:Resource Directive With Value`\ .


System Command
--------------

If only a single command should be shown, or you just want to refer to a system command from the text use:

..  \newcommand{\command}[1]{\path|#1|}

    Post Conversion Changes:
    ${PERL} 's#:raw-latex:`\\command\{(.*?)\}`#:program:`\1`#g'  ${DESTFILE}

::

   :command:`bareos-dbcheck`

The output will look like this:

:command:`bareos-dbcheck`



Unix Command Session
--------------------

If you want to display a unix command, then it needs to be put in a code block.

Example:

.. literalinclude:: /DocumentationStyleGuide/example/code-block-shell-session.rst.inc
   :language: none
   
This will be displayed as:

.. include:: /DocumentationStyleGuide/example/code-block-shell-session.rst.inc


Unix Commmand Prompt
~~~~~~~~~~~~~~~~~~~~

Unix command prompts should look identical thoughout the documentation.
Normally, we assume a root console on Linux: ``root@host:~#``\ . Mind a space after the ``#``.

The hostname **host** is used, if it is a generic host.
If the command runs on a specific Bareos server,
the specific host name from :ref:`DocumentationStyleGuide/BareosSpecificFormatting/BareosHostNames:Bareos Host Names` should be used.


Windows
-------

Windows Registry Key
~~~~~~~~~~~~~~~~~~~~

.. \newcommand{\registrykey}[1]{\path|#1|}

   Pre Conversion Changes
   perl -pe 's#registrykey#textbf#g'

If you want to display a path where registry key is defined, this formatting should be used::

   ``HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\BackupRestore\FilesNotToBackup``

The output should look like this:

``HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\BackupRestore\FilesNotToBackup``


Environment Variable
--------------------

If you want to display an (Unix) environment variable name, the following formating should be followed:


..   \newcommand{\variable}[1]{\path|#1|}
  perl -pe 's#\\variable\{\$#\\textbf\{\\\$#g'

::

  ``$HOSTNAME``
  
  ``VERSION="18.2.5"``

The output should look like this:

``$HOSTNAME``
  
``VERSION="18.2.5"``



SQL
---

Example:

.. literalinclude:: /DocumentationStyleGuide/example/code-block-sql-create-index.rst.inc
   :language: none
   
This will be displayed as:

.. include:: /DocumentationStyleGuide/example/code-block-sql-create-index.rst.inc


Logging
-------

Currently, only Bareos logs (:ref:`DocumentationStyleGuide/BareosSpecificFormatting/BareosLogging:Bareos Logging`)
are part of the documentation. General logs are currently not used.

If they should be used in the future, use

.. literalinclude:: /DocumentationStyleGuide/example/logfile.rst.inc
   :language: none

The output will look like this:

.. include:: /DocumentationStyleGuide/example/logfile.rst.inc

If logfiles should be shown more often, an own language marker can be implemented for it.


Name
----

In general, specific named items should be written as::

   **name**

The output should look like this:

**name**

Especially it should be used for:

* Argument, used synonym with parameter (has been ``\argument{<name>}`` in Latex)
* Database Table (has been ``\dbtable{<name>}`` in Latex)
* Database Table Column (has been ``\dbcolumn{<name>}`` in Latex)
* Parameter, like command line parameter or settings for Bareos configuration directives (has been ``\parameter{<name>}`` in Latex)
* Plugin Events (has been ``\pluginevent{<name>}`` in Latex)
* Storage Daemon Backend (has been ``\sdBackend{type}{subtype}`` in Latex. Did create indices in the past.)
* Software Package (e.g. package **bareos-common**, has been ``\package{<name>}`` in Latex. Did create indices in the past.)
* System User (e.g. user **root**, has been ``\user{<name>}`` in Latex))
* System Group (e.g. group **bareos**, has been ``\group{<name>}`` in Latex))
* Volume Name (has been ``\volume{<name>}`` in Latex)
* Volume Status (UPPPERCASE) (has been ``\volumestatus{<name>}`` in Latex)
   
Unless a specific rule for this kind of item exists.

Specific rules exist for:

* Substitutions form :ref:`DocumentationStyleGuide/CommonNames:Common Names`.
* :ref:`DocumentationStyleGuide/BareosSpecificFormatting/BareosHostNames:Bareos Host Names`.
* :ref:`DocumentationStyleGuide/SpecificFormatting:Operating System`.
* :ref:`DocumentationStyleGuide/SpecificFormatting:Environment Variable`.
* :ref:`Configuration Resource Names <DocumentationStyleGuide/BareosSpecificFormatting/BareosConfiguration:Resource Name>`.



Volume Parameter
----------------

If you want to display a volume parameter, the following formatting should be used:

.. \newcommand{\volumeparameter}[2]{\ifthenelse{\isempty{#2}}{%
    \path|#1|%
   }{%
    \path|#1 = #2|%
   }}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\volumeparameter\{(.*?)\}\{(.*?)\}`#\1 = **\2**#g' ${DESTFILE}

Without value::

   **Volume Retention**

With value::

  **Recycle = yes**

The output should look like this:

**Volume Retention**

or

**Recycle = yes**



Operating System
----------------

If you want to display an operating system name, the following formatting should be used:

.. literalinclude:: /DocumentationStyleGuide/example/os.rst.inc
   :language: none
   
The output will look like this:

.. include:: /DocumentationStyleGuide/example/os.rst.inc

As an index is generated from the operation systems, the naming should be identical in all occurances.
The first space seperates the platform from the version.


Email Address
-------------

An email address can be written as plain text. Sphinx will format it automatically.

Example::

   Emails will be send to root@localhost\ .

Output:
   
Emails will be send to root@localhost\ .


Backus–Naur form
----------------

To display Backus–Naur form (BNF) definitions, use following formatting:

.. literalinclude:: /DocumentationStyleGuide/example/code-block-bnf.rst.inc

The output will look like this:

.. include:: /DocumentationStyleGuide/example/code-block-bnf.rst.inc


Diagrams
--------

There are several diagram formats available for you to use.

A block diagram
~~~~~~~~~~~~~~~

.. literalinclude:: /DocumentationStyleGuide/example/blockdiag.rst.inc
   :language: none

.. include:: /DocumentationStyleGuide/example/blockdiag.rst.inc

An activity diagram
~~~~~~~~~~~~~~~~~~~

.. literalinclude:: /DocumentationStyleGuide/example/actdiag.rst.inc
   :language: none

.. include:: /DocumentationStyleGuide/example/actdiag.rst.inc

A sequence diagram
~~~~~~~~~~~~~~~~~~

.. literalinclude:: /DocumentationStyleGuide/example/seqdiag.rst.inc
   :language: none

.. include:: /DocumentationStyleGuide/example/seqdiag.rst.inc

A network diagram
~~~~~~~~~~~~~~~~~

.. literalinclude:: /DocumentationStyleGuide/example/nwdiag.rst.inc
   :language: none

.. include:: /DocumentationStyleGuide/example/nwdiag.rst.inc

Any PlantUML diagram
~~~~~~~~~~~~~~~~~~~~

.. literalinclude:: /DocumentationStyleGuide/example/plantuml.rst.inc
   :language: none

.. include:: /DocumentationStyleGuide/example/plantuml.rst.inc
