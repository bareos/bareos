Text Replacing Formats
######################

The replacement text has been defined in conf.py under General configurations.

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


Vmware
------

If you want to display a vmware related name, the following formatting should be followed:

..   \newcommand{\vmware}{VMware\registered\xspace}

     Post Conversion Changes
     ${PERL} 's#:raw-latex:`\\vmware(\s*)`#\|vmware\|\1#g'   ${DESTFILE}

.. code-block:: sh

   |vmware|

The output should look like this:

VMware


vSphere
-------

If you want to display a vSphere related name, the following formatting should be followed:

..   \newcommand{\vSphere}{VMware vSphere\registered\xspace}

     Post Conversion Changes
     ${PERL} 's#:raw-latex:`\\vSphere(\s*)`#\|vsphere\|\1#g'   ${DESTFILE}

.. code-block:: sh

   |vSphere|

The output should look like this:

VMware vSphere


Bareos File Daemon
------------------

If you want to display the name, "Bareos File Daemon", the following formatting should be followed:

..  \newcommand{\bareosFd}{Bareos File Daemon\xspace}

    Post Conversion Changes
    ${PERL} 's#:raw-latex:`\\bareosFd(\s*)`#\|bareosFd\|\1#g'   ${DESTFILE}

.. code-block:: sh

   |bareosFd|

The output should look like this:

Bareos File Daemon


Bareos Storage Daemon
---------------------

If you want to display the name, "Bareos Storage Daemon", the following formatting should be followed:

..  \newcommand{\bareosSd}{Bareos Storage Daemon\xspace}

    Post Conversion Changes
    ${PERL} 's#:raw-latex:`\\bareosSd(\s*)`#\|bareosSd\|\1#g'   ${DESTFILE}

.. code-block:: sh

   |bareosSd|

The output should look like this:

Bareos Storage Daemon


Bareos Director
---------------

If you want to display the name, "Bareos Director", the following formatting should be followed:

.. \newcommand{\bareosDir}{Bareos Director\xspace}

    Post Conversion Changes
    ${PERL} 's#:raw-latex:`\\bareosDir(\s*)`#\|bareosDir\|\1#g'   ${DESTFILE}

.. code-block:: sh

   |bareosDir|

The output should look like this:

Bareos Director


Bareos Tray Monitor
-------------------

If you want to display the name, "Bareos Tray Monitor", the following formatting should be followed:

.. \newcommand{\bareosTrayMonitor}{Bareos Tray Monitor\xspace}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\bareosTrayMonitor(\s*)`#\|bareosTrayMonitor\|\1#g'   ${DESTFILE}

.. code-block:: sh

   |bareosTraymonitor|

The output should look like this:

Bareos Tray Monitor


Bareos Webui
------------

If you want to display the name, "Bareos Webui", the following formatting should be followed:

.. \newcommand{\bareosWebui}{Bareos Webui\xspace}

   Post Conversion Changes
   ${PERL} 's#:raw-latex:`\\bareosWebui(\s*)`#\|bareosWebui\|\1#g'   ${DESTFILE}

.. code-block:: sh

   |bareosWebui|

The output should look like this:

Bareos Webui


Yes|No
------

If you want to display yes|no text, the following formatting should be used:

.. code-block:: sh

   yes|no

The output should look like this:

yes|no

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
| NDMP_BAREOS                            | ``NDMP\_BAREOS``                          |
+----------------------------------------+-------------------------------------------+
| NDMP_NATIVE                            | ``NDMP\_NATIVE``                          |
+----------------------------------------+-------------------------------------------+
