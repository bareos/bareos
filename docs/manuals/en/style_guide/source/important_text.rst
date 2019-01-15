Important Text Formats
######################

Bconsole Command
----------------

The bconsole command can be used to write a complete bconsole session. This command should be formatted as:

.. \newcommand{\bcommand}[2]{\path|#1| \path|#2|}

   Post Conversion Changes:
   ${PERL} 's#:raw-latex:`\\bcommand\{(\w+)\}\{([\w/ ]+)}`#:strong:`\1 \2`#g' ${DESTFILE}
   ${PERL} 's#:raw-latex:`\\bcommand\{(.*?)\}\{}`#:strong:`\1`#g'  ${DESTFILE}
   ${PERL} 's#:raw-latex:`\\bcommand\{(.*?)\}`#:strong:`\1`#g'  ${DESTFILE}
   ${PERL} 's#\\bcommand\{(.*?)\}\{}#:strong:`\1`#g'  ${DESTFILE}

.. code-block:: sh

   :strong:`help configure`

The output should look like this:

:strong:`help configure`


Output of a Bconsole Command
----------------------------

If you write only the output of a bconsole command and not a complete bconsole session, then format it like this:

.. \newcommand{\bconsoleOutput}[1]{\path|#1|}

   Pre Conversion Changes:
   perl -pe 's#\\bquote\{\\bconsoleOutput\{Building directory tree ...\}\}#\\bquote{Building directory tree ...}#g' \

   Post Conversion Changes:
   ${PERL} 's#:raw-latex:`\\bquote\{(.*?)\}`#:emphasis:`\1`#g' ${DESTFILE}

.. code-block:: sh

   ``Building directory tree ...``

The output should look like this:

 ``Building directory tree ...``



Bquote
------

If you want to quote a certain text, the following formatting should be used:

.. \newcommand{\bquote}[1]{``#1''}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\bquote\{(.*?)\}`#:emphasis:`\1`#g' ${DESTFILE}

.. code-block:: sh

   ''Application Managed''

The output should look like this:

''Application Managed''


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


Bareos Config Input
-------------------

If you want to display the contents of a config file and it is present in some other folder, the following formatting should be used:

.. \newcommand{\bconfigInput}[1]{
    {
        \footnotesize
   %     \lstinputlisting[backgroundcolor=\color{colorConfig},frame=single,framerule=0pt,basicstyle=\ttfamily,keepspaces=true,columns=fullflexible,moredelim=[is][\it]{<input>}{</input>},moredelim=[is][\underline]{<strong>}{</strong>}]{#1}
        \lstinputlisting[
                backgroundcolor=\color{colorConfig},
                frame=single,framerule=0pt,
                basicstyle=\ttfamily,
                columns=fullflexible,
                keepspaces=true,
                literate={-}{-}1
            ]{#1}
    }
    ${PERL} 's#\\bconfigInput\{(.*?)\}#.. literalinclude:: ../../main/\1#g' ${DESTFILE}

.. code-block:: sh

   .. literalinclude:: ../../main/config/SdDeviceDeviceOptionsRados1.conf

The output should look like this:

.. literalinclude:: ../../main/config/SdDeviceDeviceOptionsRados1.conf


Bconsole session
----------------

If you want to display a complete bareos console session, then it needs to be put in a code block.

.. ${PERL} 's#\{bconsole\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}

.. code-block:: sh

   .. code-block:: sh
    :caption: list volumes
    *list volumes

The output should look like this:

.. code-block:: sh
   :caption: list volumes

   *list volumes


Bareos Configuration
--------------------

If you want to display bareos configuration that is part of configuration file, then it needs to be put in a code block.

.. ${PERL} 's#\{bconfig\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}

.. code-block:: sh

   .. code-block:: sh
    :caption: Pool Ressource: setting Maximum Block Size

    Pool {
       Name = LTO-4-1M
       Pool Type = Backup
       Recycle = yes                       # Bareos can automatically recycle Volumes
       AutoPrune = yes                     # Prune expired volumes
       Volume Retention = 1 Month          # How long should the Full Backups be kept? (#06)
       Maximum Block Size = 1048576
       Recycle Pool = Scratch
    }

The output should look like this:

.. code-block:: sh
    :caption: Pool Resource: setting Maximum Block Size

    Pool {
       Name = LTO-4-1M
       Pool Type = Backup
       Recycle = yes                       # Bareos can automatically recycle Volumes
       AutoPrune = yes                     # Prune expired volumes
       Volume Retention = 1 Month          # How long should the Full Backups be kept? (#06)
       Maximum Block Size = 1048576
       Recycle Pool = Scratch
    }


Bareos Configuration Resource
-----------------------------

If you want to display a baeros specific resource configuration file, then it needs to be put in a code block.

.. ${PERL} 's#\{bareosConfigResource\}\{(.*?)\}\{(.*?)\}\{(.*?)\}#\n.. code-block:: sh\n    :caption: \1 \2 \3\n#g'   ${DESTFILE}
   perl -i -pe 's#\{bareosConfigResource\}\{(.*)\}\{(.*)\}\{(.*)\}#\n.. code-block:: sh\n    :caption: daemon:\1 resource:\2 name:\3\n#g' ${DESTFILE}

.. code-block:: sh

   .. code-block:: sh
    :caption: bareos-dir job example

    Job {
        ...
        Accurate = yes
        Always Incremental = yes
        Always Incremental Job Retention = <timespec>
        Always Incremental Keep Number = <number>
        ...
    }

The output should look like this:

   .. code-block:: sh
    :caption: bareos-dir job example

    Job {
        ...
        Accurate = yes
        Always Incremental = yes
        Always Incremental Job Retention = <timespec>
        Always Incremental Keep Number = <number>
        ...
    }


Configuration File
------------------

If you want to display a general configuration file(not related to bareos), then it needs to be put in a code block.

.. ${PERL} 's#\{config\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}

.. code-block:: sh

   .. code-block:: sh
    :caption: /etc/my.cnf.d/server.cnf

    ...
    [mysqld]
    innodb_lock_wait_timeout = 300
    ...

The output should look like this:

.. code-block:: sh
    :caption: /etc/my.cnf.d/server.cnf

    ...
    [mysqld]
    innodb_lock_wait_timeout = 300
    ...


Unix Commands
-------------

If you want to display a unix command, then it needs to be put in a code block.

.. ${PERL} 's#\{commands\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}

.. code-block:: sh

   .. code-block:: sh
    :caption: Access the local PostgreSQL database

    su - postgres
    psql

The output should look like this:

.. code-block:: sh
    :caption: Access the local PostgreSQL database

    su - postgres
    psql


Unix Command Prompt
-------------------

If you want to display a unix command prompt session, then it needs to be put in a code block.

.. ${PERL} 's#\{commandOut\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}

.. code-block:: sh

   .. code-block:: sh
    :caption: bconsole command line options

    Usage: bconsole [-s] [-c config_file] [-d debug_level]
           -D <dir>    select a Director
           -l          list Directors defined
           -c <file>   set configuration file to file
           -d <nn>     set debug level to <nn>
           -dt         print timestamp in debug output
           -n          no conio
           -s          no signals
           -u <nn>     set command execution timeout to <nn> seconds
           -t          test - read configuration and exit
           -?          print this message.

The output should look like this:

.. code-block:: sh
    :caption: bconsole command line options

    Usage: bconsole [-s] [-c config_file] [-d debug_level]
           -D <dir>    select a Director
           -l          list Directors defined
           -c <file>   set configuration file to file
           -d <nn>     set debug level to <nn>
           -dt         print timestamp in debug output
           -n          no conio
           -s          no signals
           -u <nn>     set command execution timeout to <nn> seconds
           -t          test - read configuration and exit
           -?          print this message.

Logging
-------

If you want to display bareos specific logs, then they need to be put in a code block.

.. ${PERL} 's#\{logging\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}

.. code-block:: sh

   .. code-block:: sh
    :caption: bareos.log

    08-Sep 12:58 win-fd JobId 10: secure_erase: executing C:/cygwin64/bin/shred.exe "C:/temp/bareos-restores/C/Program Files/Bareos/Plugins/bareos_fd_consts.py"
    08-Sep 12:58 win-fd JobId 10: secure_erase: executing C:/cygwin64/bin/shred.exe "C:/temp/bareos-restores/C/Program Files/Bareos/Plugins/bareos_sd_consts.py"
    08-Sep 12:58 win-fd JobId 10: secure_erase: executing C:/cygwin64/bin/shred.exe "C:/temp/bareos-restores/C/Program Files/Bareos/Plugins/bpipe-fd.dll"

The output should look like this:

.. code-block:: sh
    :caption: bareos.log

    08-Sep 12:58 win-fd JobId 10: secure_erase: executing C:/cygwin64/bin/shred.exe "C:/temp/bareos-restores/C/Program Files/Bareos/Plugins/bareos_fd_consts.py"
    08-Sep 12:58 win-fd JobId 10: secure_erase: executing C:/cygwin64/bin/shred.exe "C:/temp/bareos-restores/C/Program Files/Bareos/Plugins/bareos_sd_consts.py"
    08-Sep 12:58 win-fd JobId 10: secure_erase: executing C:/cygwin64/bin/shred.exe "C:/temp/bareos-restores/C/Program Files/Bareos/Plugins/bpipe-fd.dll"


System Command
---------------

The system command can be used to display the linux system commands. This command should be formatted as:

..  \newcommand{\command}[1]{\path|#1|}

    Post Conversion Changes:
    ${PERL} 's#:raw-latex:`\\command\{(.*?)\}`#:program:`\1`#g'  ${DESTFILE}

.. code-block:: sh

   :program:`cd ..`

The output should look like this:

:program:`cd ..`


Directory
---------

If you want to display a directory path, the following formatting should be followed:

.. \newcommand{\directory}[1]{\path|#1|}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\directory\{(.*?)\}`#:file:`\1`#g'   ${DESTFILE}

.. code-block:: sh

   :file:`/var/lib/pgsql/data`

The output should look like this:

:file:`/var/lib/pgsql/data`


Database Table
--------------

If you want to display the database table name, the following formatting should be followed:

.. \newcommand{\dbtable}[1]{\path|#1|}

   Post Conversion Changes
   ${PERL} 's#\\dbtable\{(.*)\}#**\1**#g'   ${DESTFILE}

.. code-block:: sh

   **Media**

The output should look like this:

**Media**


File
----

If you want to display a configuration file name, the following formatting should be followed:

.. \newcommand{\file}[1]{\path|#1|}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\file\{(.*?)\}`#:file:`\1`#g'   ${DESTFILE}
   ${PERL} 's#\\file\{(.*?)\}#:file:`\1`#g'   ${DESTFILE}

.. code-block:: sh

   :file:`/etc/devfs.conf`

The output should look like this:

:file:`/etc/devfs.conf`


File Set
--------

If you want to display the name of a File Set(i.e. group of files), the following formatting should be followed:

.. \newcommand{\fileset}[1]{\resourcename{Dir}{FileSet}{#1}}

   Post Conversion Changes
   ${PERL} 's|:raw-latex:`\\fileset\{(.*)\}`|**\1**|g'   ${DESTFILE}

.. code-block:: sh

   **LinuxAll**

The output should look like this:

**LinuxAll**


Host
----

If you want to display a hostname, the following formatting should be followed:

.. \newcommand{\host}[1]{\path|#1|}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\host\{(.*?)\}`#:strong:`\1`#g' ${DESTFILE}

.. code-block:: sh

   :strong:`localhost`

The output should look like this:

:strong:`localhost`


Job Name
--------

If you want to display a linux job name, the following formatting should be used:

.. \newcommand{\job}[1]{\resourcename{Dir}{Job}{#1}}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\job\{(.*?)\}`#**\1**:sup:`Dir`:sub:`job`\ #g'   ${DESTFILE}

.. code-block:: sh

   **BackupCatalog**:sup:`Dir`:sub:`job`

* Superscript: The daemon to which the job belongs to. There can be 3 types of daemons: 'Dir', 'Fd', 'Sd'
* Subscript: The resource type of the job.

The output should look like this:

**BackupCatalog**:sup:`Dir`:sub:`job`


Name
----

This formatting has been used to provide names of several types. It is an ambigious formatting and hence, should not be used.

.. \newcommand{\name}[1]{\textbf{#1}}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\name\{(.*?)\}`#**\1**#g' ${DESTFILE}

.. code-block:: sh

   **admin**

The output should look like this:

**admin**


User
----

If you want to display a user name, the following formatting should be followed:

.. \newcommand{\user}[1]{\path|#1|}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\user\{(.*)\}`#**\1**#g'   ${DESTFILE}
   ${PERL} 's#\\user\{(.*)\}#**\1**#g'   ${DESTFILE}

.. code-block:: sh

   **root**

The output should look like this:

**root**


Package
-------

If you want to display a linux package name, the following formatting should be used:

.. \newcommand{\package}[1]{\index[general]{Package!#1}\path|#1|\xspace}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\package\{(.*?)\}`#**\1**#g'   ${DESTFILE}
   ${PERL} 's#:raw-latex:`\\package\{(.*?)\}`#**\1**#g'   ${DESTFILE}

.. code-block:: sh

   **database-mysql**

The output should look like this:

**database-mysql**


Parameter
---------

If you want to display a parameter name, the following formatting should be used:

.. 
    \newcommand{\parameter}[1]{\path|#1|}

.. code-block:: sh

   ``time``

The output should look like this:

``time``


Plugin Event
------------

If you want to display name of a bareos plugin event, the following formatting should be used:

.. \newcommand{\pluginevent}[1]{\path|#1|}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\pluginevent\{(.*?)\}`#:strong:`\1`#g' ${DESTFILE}

.. code-block:: sh

   :strong:`unload`

The output should look like this:

:strong:`unload`


Resource Name
-------------

If you want to display a resource name, the following formatting should be used:


.. \newcommand{\resourcename}{[...]}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\resourcename\{(.*?)\}\{(.*?)\}\{(.*?)\}`#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}
   ${PERL} 's#:raw-latex:`\\resourcename\*\{(.*?)\}\{(.*?)\}\{(.*?)\}`#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}

.. code-block:: sh

   **Standard**:sup:`Fd`:sub:`Messages`

* Superscript: The daemon to which the resource belongs to. There can be 3 types of daemons: 'Dir', 'Fd', 'Sd'
* Subscript: The resource type of the resource.

The output should look like this:

**Standard**:sup:`Fd`:sub:`Messages`


Argument
--------

If you want to display an argument name/value, the following formatting should be used:

.. \newcommand{\argument}[1]{\textit{#1}}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\argument\{(.*?)\}`#:strong:`\1`#g' ${DESTFILE}

.. code-block:: sh

   :strong:`passphrase`

The output should look like this:

:strong:`passphrase`


Resource Type
-------------

If you want to display a resource type, the following formatting should be used:

.. \newcommand{\resourcetype}[2]{\path|#2|$^{\mbox{\tiny #1}}$}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\resourcetype\{(.*?)\}\{(.*?)\}`#:sup:`\1`\ :strong:`\2`#g'   ${DESTFILE}

.. code-block:: sh

   :sup:`Sd` :strong:`Storage`

* Superscript: The resource type of the resource name mentioned. There can be 3 types of resources: 'Dir', 'Fd', 'Sd'

The output should look like this:

:sup:`Sd` :strong:`Storage`


Pool
----

If you want to display a Pool name, the following formatting should be used:

.. \newcommand{\pool}[1]{\resourcename{Dir}{Pool}{#1}}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\pool\{(.*?)\}`#**\1**:sup:`Dir`:sub:`pool`\ #g'   ${DESTFILE}

.. code-block:: sh

   **VirtualFull**:sup:`Dir`:sub:`pool`

The output should look like this:

**VirtualFull**:sup:`Dir`:sub:`pool`


Registry Key
------------

If you want to display a path where registry key is defined, this formatting should be used.

.. \newcommand{\registrykey}[1]{\path|#1|}

   Pre Conversion Changes
   perl -pe 's#registrykey#textbf#g'

.. code-block:: sh

   ``HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\BackupRestore\FilesNotToBackup``

The output should look like this:

``HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Control\BackupRestore\FilesNotToBackup``


Variable
--------

If you want to display an environment variable name, the following formating should be followed:


..   \newcommand{\variable}[1]{\path|#1|}
  perl -pe 's#\\variable\{\$#\\textbf\{\\\$#g'

.. code-block:: sh

   **$HOSTNAME**

The output should look like this:

**$HOSTNAME**


Volume Status
-------------

If you want to display the status of a volume, the following formatting should be used:

..   \newcommand{\volumestatus}[1]{\path|#1|}

      Post Conversion Changes
      ${PERL} 's#:raw-latex:`\\volumestatus\{(.*?)\}`#**\1**#g'   ${DESTFILE}

.. code-block:: sh

   **APPEND**

The output should look like this:

**APPEND**


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

.. code-block:: sh

   Recycle = **yes**

The output should look like this:

Recycle = **yes**


Configuration Line
------------------

If you want to display a line from the configuration, the following formatting should be used:

.. \newcommand{\configline}[1]{\path|#1|}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\configline\{(.*?)\}`#:strong:`\1`#g'  ${DESTFILE}

.. code-block:: sh

   :strong:`Append = "/var/log/bareos/bareos-fd.log" = all, !skipped, !restored`

The output should look like this:

:strong:`Append = "/var/log/bareos/bareos-fd.log" = all, !skipped, !restored`


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


Since Version
-------------

If you want to display since which version a particular a feature is present or deleted, the following formatting should be used:

.. \newcommand{\sinceVersion}[3]{%
   % 1: daemon (dir|sd|fd),
   % 2: item,
   % 3: version
   \edef\pv{#3}%
   \ifcsvoid{pv}{}{%
   Version $>=$ #3%
   %\index[#1]{#2}%
   % expand variables
   \edef\temp{\noexpand\index[general]{bareos-#3!#2}}%
   \temp%
   }%

   Post Conversion Changes
   ${PERL} 's|:raw-latex:`\\sinceVersion\{(.*)\}\{(.*)\}\{(.*)\}`|\3|g'   ${DESTFILE}

.. code-block:: sh

   Version >= 16.2.4

The output should look like this:

Version >= 16.2.4


Resource Directive
------------------

If you want to display a resource directive name, the following formatting should be used:

.. \makeatletter
   \newcommand{\linkResourceDirective}{%
   \@ifstar{%
   \linkResourceDirectiveStar%
   }{%
   \linkResourceDirectiveNostar%
   }%
   }
   \makeatother
    \newcommand{\linkResourceDirectiveStar}[3]{%
   \linkResourceDirectiveOpt{#1}{#2}{#3}{#3}%
   }
   \newcommand{\linkResourceDirectiveNostar}[3]{%
   \linkResourceDirectiveOpt{#1}{#2}{#3}{#3 $^{\mbox{\tiny #1}}_{\mbox{\tiny #2}}$}%
   }

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\linkResourceDirective\{([ \w]+)\}\{([ \w]+)\}\{([ \w]+)\}`#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}
   ${PERL} 's#:raw-latex:`\\linkResourceDirective\*\{([ \w]+)\}\{([ \w]+)\}\{([ \w]+)\}`#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}
   ${PERL} 's#\\linkResourceDirective\{([ \w]+)\}\{([ \w]+)\}\{([ \w]+)\}#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}

.. code-block:: sh

   **Always Incremental Max Full Age**:sup:`Dir`:sub:`Job`


* Superscript: The daemon to which the resource directive belongs to. There can be 3 types of daemons: 'Dir', 'Fd', 'Sd'
* Subscript: The resource type of the directive.


**Always Incremental Max Full Age**:sup:`Dir`:sub:`Job`


Resource Directive Value
------------------------

If you want to display a resource directive along with its value, the following formatting should be used:

.. \newcommand{\linkResourceDirectiveValue}[4]{%
   \linkResourceDirective{#1}{#2}{#3} \path|= #4|%
   }

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\linkResourceDirectiveValue\{(.*?)\}\{(.*?)\}\{(.*?)\}\{(.*?)\}`#**\3**:sup:`\1`:sub:`\2`\ = **\4**#g' ${DESTFILE}

.. code-block:: sh

   **Device Type**:sup:`Sd`:sub:`Device` = **File**

The output should look like this:

**Device Type**:sup:`Sd`:sub:`Device` = **File**
