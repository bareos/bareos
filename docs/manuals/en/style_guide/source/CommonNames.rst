Common Names
============

Specific strings are used again and again in the Bareos documentation.

Here we define how these name should be written (upper or lower case, in one word vs. multiple words, ...).

Text subsitutions can be defined in :file:`conf.py` file.

Bareos Names
------------

+----------------------------------------+-------------------------------------------+
|      **Text to be Displayed**          |           **Text Formatting**             |
+----------------------------------------+-------------------------------------------+
| Bareos 	                         | Bareos                                    |
+----------------------------------------+-------------------------------------------+
| |dir| 	                         | ``|dir|``                                 |
+----------------------------------------+-------------------------------------------+
| |sd|	         	                 | ``|sd|``                                  |
+----------------------------------------+-------------------------------------------+
| |fd|                                   | ``|fd|``       		             |
+----------------------------------------+-------------------------------------------+
| |bconsole|                             | ``|bconsole|``                            |
+----------------------------------------+-------------------------------------------+
| |webui|         			 | ``|webui|``                               |
+----------------------------------------+-------------------------------------------+
| |traymonitor|                          | ``|traymonitor|``                         |
+----------------------------------------+-------------------------------------------+

The name **Bareos** should always be written with capital B (except in technical terms like URLs, releases (bareos-18.2.5) or host names).


Bareos Paths and Filenames
--------------------------

+----------------------------------------+-------------------------------------------+-----------------------------------------+
|      **File/Directory**                |            **Text Formatting**            |               **Output**                |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Bareos Configuration Path              | ``:file:`/etc/bareos/```                  | :file:`/etc/bareos/`                    |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Bareos Disk Storage Path               | ``:file:`/var/lib/bareos/storage/```      | :file:`/var/lib/bareos/storage/`        |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Bareos Scripts                         | ``:file:`/usr/lib/bareos/scripts/```      | :file:`/usr/lib/bareos/scripts/`        |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Configuration Directory - Director     | ``:file:`/etc/bareos/bareos-dir.d/```     | :file:`/etc/bareos/bareos-dir.d/`       |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Configuration Directory - Sd           | ``:file:`/etc/bareos/bareos-sd.d/```      | :file:`/etc/bareos/bareos-sd.d/`        |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Configuration Directory - Fd           | ``:file:`/etc/bareos/bareos-sd.d/```      | :file:`/etc/bareos/bareos-sd.d/`        |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Configuration Directory - Tray Monitor | ``:file:`/etc/bareos/tray-monitor.d/```   | :file:`/etc/bareos/tray-monitor.d/`     |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Configuration File - Bconsole          | ``:file:`/etc/bareos/bconsole.conf```     | :file:`/etc/bareos/bconsole.conf`       |
+----------------------------------------+-------------------------------------------+-----------------------------------------+
| Bareos Log File                        | ``:file:`/var/log/bareos/bareos.log```    | :file:`/var/log/bareos/bareos.log`      |
+----------------------------------------+-------------------------------------------+-----------------------------------------+


NDMP
----

.. csv-table:: NDMP Names
   :header: "Text to be Displayed", "Text Formatting"

   "Data Management Agent", "Data Management Agent"
   "Data Agent",            "Data Agent"
   "Tape Agent",            "Tape Agent"
   "Robot Agent",           "Robot Agent"
   |ndmpbareos|,            ``|ndmpbareos|``
   |ndmpnative|,            ``|ndmpnative|``

.. _section-NdmpBareos:

NDMP_BAREOS
~~~~~~~~~~~

This is only a fake section to demonstrate ``|ndmpbareos|``. Ignore it.

.. _section-NdmpNative:

NDMP_NATIVE
~~~~~~~~~~~

This is only a fake section to demonstrate ``|ndmpnative|``. Ignore it.


Products
--------

.. csv-table:: Product Names
   :header: "Text to be Displayed", "Text Formatting"
   
   |mysql|,      ``|mysql|``
   |postgresql|, ``|postgresql|``
   reST,         reST
   |sqlite|,     ``|sqlite|``
   |vmware|,     ``|vmware|``
   |vSphere|,    ``|vSphere|``
