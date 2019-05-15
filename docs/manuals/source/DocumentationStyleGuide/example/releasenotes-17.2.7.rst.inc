Bareos-17.2
-----------


.. index:: 
   :pair: bareos-17.2.7; Release Notes

.. _bareos-17.2.7:

bareos-17.2.7
~~~~~~~~~~~~~

================ ===============================================
Code Release     2018-07-13
Database Version 2171 (unchanged)
Release Ticket   :ticket:`966`
Url              http://download.bareos.com/bareos/release/17.2/
================ ===============================================

This release contains several bugfixes and enhancements. Excerpt:

-  :ticket:`892` **bareos-storage-droplet**: improve error handling on unavailable backend.
-  :ticket:`902` **bareos-storage-droplet**: improve job status handling (terminate job after all data is written).
-  :ticket:`967` :strong:`Windows`: overwrite symbolic links on restore.
-  :ticket:`983` |bareosSd|: prevent sporadic crash when :config:option:`sd/storage/CollectJobStatistics = yes`\ .
-  :strong:`SLES 12sp2` and :strong:`SLES 12sp3`: provide **bareos-storage-ceph** and **bareos-filedaemon-ceph-plugin** packages.
