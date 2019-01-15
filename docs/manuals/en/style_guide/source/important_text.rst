Specific Formatting
###################



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


Unix Command Session
--------------------

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


Command Output
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
