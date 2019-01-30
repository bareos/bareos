Common Names
######################

Specific strings are used again and again in the Bareos documentation.

Here we define how these name should be written (upper or lower case, in one word vs. multiple words, ...).

Text subsitutions can be defined in :file:`conf.py` file.

Bareos Names
============


+----------------------------------------+-------------------------------------------+
|      **Text to be Displayed**          |           **Text Formatting**             |
+----------------------------------------+-------------------------------------------+
| Bareos 	                         | Bareos                                    |
+----------------------------------------+-------------------------------------------+
| |bareosDir| 	                         | ``|bareosDir|``                           |
+----------------------------------------+-------------------------------------------+
| |bareosSd|		                 | ``|bareosSd|``                            |
+----------------------------------------+-------------------------------------------+
| |bareosFd|                             | ``|bareosSd|``       		     |
+----------------------------------------+-------------------------------------------+
| |bconsole|                             | ``|bconsole|``                            |
+----------------------------------------+-------------------------------------------+
| |bareosWebui|     			 | ``|bareosWebui|``                         |
+----------------------------------------+-------------------------------------------+
| |bareosTraymonitor|                    | ``|bareosTraymonitor|``                   |
+----------------------------------------+-------------------------------------------+

The name **Bareos** should always be written as capital B (except in technical terms like URLs or host names).


Bareos Paths and Filenames
--------------------------

+----------------------------------------+-------------------------------------------+-----------------------------------------+
|      **File/Directory**                |            **Text Formatting**            |               **Output**                |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Bareos Configuration Path              | ``:file:`/etc/bareos/```                  | :file:`/etc/bareos/`                    |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Bareos Storage Files                   | ``:file:`/var/lib/bareos/storage/```      | :file:`/var/lib/bareos/storage/`        |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Bareos Scripts                         | ``:file:`/usr/lib/bareos/scripts/```      | :file:`/usr/lib/bareos/scripts/`        |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Configuration File - Director          | ``:file:`/etc/bareos/bareos-dir.conf```   | :file:`/etc/bareos/bareos-dir.conf`     |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Configuration File - Sd                | ``:file:`/var/lib/bareos/storage/```      | :file:`/var/lib/bareos/storage/`        |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Configuration File - Fd                | ``:file:`/etc/bareos/bareos-sd.conf```    | :file:`/etc/bareos/bareos-sd.conf`      |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Configuration File - Bconsole          | ``:file:`/etc/bareos/bconsole.conf```     | :file:`/etc/bareos/bconsole.conf`       |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Configuration File - Tray Monitor      | ``:file:`/etc/bareos/tray-monitor.conf``` | :file:`/etc/bareos/tray-monitor.conf`   |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Configuration File - Bat               | ``:file:`/etc/bareos/bat.conf```          | :file:`/etc/bareos/bat.conf`            |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Configuration Directory - Director     | ``:file:`/etc/bareos/bareos-dir.d/```     | :file:`/etc/bareos/bareos-dir.d/`       |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Configuration Directory - Sd           | ``:file:`/etc/bareos/bareos-sd.d/```      | :file:`/etc/bareos/bareos-sd.d/`        |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Configuration Directory - Fd           | ``:file:`/etc/bareos/bareos-sd.d/```      | :file:`/etc/bareos/bareos-sd.d/`        |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Configuration Directory - Bconsole     | ``:file:`/etc/bareos/bconsole.d/```       | :file:`/etc/bareos/bconsole.d/`         |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Configuration Directory - Tray Monitor | ``:file:`/etc/bareos/tray-monitor.d/```   | :file:`/etc/bareos/tray-monitor.d/`     |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Bareos Log File                        | ``:file:`/var/log/bareos/bareos.log```    | :file:`/var/log/bareos/bareos.log`      |
+----------------------------------------+-------------------------------------------+-----------------------------------------+


NDMP Text
---------

+----------------------------------------+-------------------------------------------+
|      **Text to be Displayed**          |           **Text Formatting**             |
+----------------------------------------+-------------------------------------------+
| Data Management Agent	                 | ``|DataManagementAgent|``                 |
+----------------------------------------+-------------------------------------------+
| Data Agent		                 | ``|DataAgent|``                           |
+----------------------------------------+-------------------------------------------+
| Tape Agent                             | ``|Tape Agent|``      		     |
+----------------------------------------+-------------------------------------------+
| Robot Agent          			 | ``|Robot Agent|``                         |
+----------------------------------------+-------------------------------------------+
| NDMP_BAREOS                            | ``NDMP_BAREOS``                           |
+----------------------------------------+-------------------------------------------+
| NDMP_NATIVE                            | ``NDMP_NATIVE``                           |
+----------------------------------------+-------------------------------------------+

Unix Commmand Prompt
--------------------

Unix command prompts should look identical.
Normally, we assume a root console on Linux:

..  
    root{\at}linux:\~{}\# 

.. code-block:: sh

   ``root@linux:~#`` 

The output should look like this (mind a space after the ``#``):

|commandPrompt|

    
    
Mysql
-----

If you want to display a mysql related name, the following formatting should be followed:

.. \newcommand{\mysql}{MySQL/MariaDB\xspace}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\mysql(\s*)`# \|mysql\|\1#g'   ${DESTFILE}

.. code-block:: sh

   |mysql|

The output should look like this:

MySQL/MariaDB


Postgresql
----------

If you want to display a postgresql related name, the following formatting should be followed:

.. \newcommand{\postgresql}{PostgreSQL\xspace}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\postgresql(\s*)`# \|postgresql\|\1#g'   ${DESTFILE}

.. code-block:: sh

   |postgresql|

The output should look like this:

PostgreSQL


Sqlite Database
---------------

If you want to display a sqlite related name, the following formatting should be followed:

.. \newcommand{\sqlite}{Sqlite\xspace}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\sqlite(\s*)`#\|sqlite\|\1#g'   ${DESTFILE}

.. code-block:: sh

   |sqlite|

The output should look like this:

Sqlite


VMware
------

If you want to display a vmware related name, the following formatting should be followed:

..   \newcommand{\vmware}{VMware\registered\xspace}

     Post Conversion Changes
     ${PERL} 's#:raw-latex:`\\vmware(\s*)`#\|vmware\|\1#g'   ${DESTFILE}

.. code-block:: sh

   |vmware|

The output should look like this:

VMware


VMware vSphere
--------------

If you want to display a vSphere related name, the following formatting should be followed:

..   \newcommand{\vSphere}{VMware vSphere\registered\xspace}

     Post Conversion Changes
     ${PERL} 's#:raw-latex:`\\vSphere(\s*)`#\|vsphere\|\1#g'   ${DESTFILE}

.. code-block:: sh

   |vSphere|

The output should look like this:

VMware vSphere


yes|no
------

If you want to display yes|no text, the following formatting should be used:

.. code-block:: sh

   yes|no

The output should look like this:

yes|no
