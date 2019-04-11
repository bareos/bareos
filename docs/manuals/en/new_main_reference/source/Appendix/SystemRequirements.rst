.. _SysReqs:

System Requirements
===================

:index:`\ <single: System Requirements>` :index:`\ <single: Requirements; System>`

-  The minimum versions for each of the databases supported by Bareos are:

   -  PostgreSQL 8.4 (since Bareos 13.2.3)

   -  MySQL 4.1 - 5.6 or compatible fork (e.g. MariaDB), see :ref:`section-MysqlSupport`

   -  SQLite 3

-  Windows release is cross-compiled on Linux

-  Bareos requires a good implementation of pthreads to work. This is not the case on some of the BSD systems.

-  The source code has been written with portability in mind and is mostly POSIX compatible. Thus porting to any POSIX compatible operating system should be relatively easy.

-  Jansson library: 

.. _jansson:

 :index:`\ <single: JSON>` :index:`\ <single: Jansson; \see{JSON}>` Bareos :sinceVersion:`15.2.0: requires: jansson` offers a JSON API mode, see `Bareos Developer Guide (api-mode-2-json) <http://doc.bareos.org/master/html/bareos-developer-guide.html#api-mode-2-json>`_. On some platform, the Jansson library is directory available. On others it can easly be added. For some older platforms, we compile Bareos without JSON API mode.




