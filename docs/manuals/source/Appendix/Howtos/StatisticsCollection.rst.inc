.. _section-StatisticsCollection:

Statistics Collection
---------------------

.. index::
   single: Webui; Configure Statistics Collection

Statistics Collection can be controlled by a number of configuration directives. If Statistics Collection is enabled, statistics are collected by the |dir| and stored into the Catalog database. So enabling this feature will increase your database size.

The Statistics are used by the |webui| to show the status of a running job.

Director Configuration - Director Resource Directives
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- :config:option:`dir/director/StatisticsCollectInterval`
- :config:option:`dir/director/StatisticsRetention`

Director Configuration - Storage Resource Directives
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- :config:option:`dir/storage/CollectStatistics`

Storage Configuration - Storage Resource Directives
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- :config:option:`sd/storage/CollectDeviceStatistics`
- :config:option:`sd/storage/CollectJobStatistics`
- :config:option:`sd/storage/StatisticsCollectInterval`

Storage Configuration - Device Resource Directives
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- :config:option:`sd/device/CollectStatistics`

See chapter :ref:`section-JobStatistics` for additional information.
