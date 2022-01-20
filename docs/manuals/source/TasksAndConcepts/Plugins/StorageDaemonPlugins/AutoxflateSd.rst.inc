.. _plugin-autoxflate-sd:

autoxflate-sd
~~~~~~~~~~~~~

.. index::
   single: Plugin; autoxflate-sd

This plugin is part of the **bareos-storage** package.

The autoxflate-sd plugin can inflate (decompress) and deflate (compress) the data being written to or read from a device. It can also do both.

.. image:: /include/images/autoxflate-functionblocks.*
   :width: 80.0%




Therefore the autoxflate plugin inserts a inflate and a deflate function block into the stream going to the device (called OUT) and coming from the device (called IN).

Each stream passes first the inflate function block, then the deflate function block.

The inflate blocks are controlled by the setting of the :config:option:`sd/device/AutoInflate`\  directive.

The deflate blocks are controlled by the setting of the :config:option:`sd/device/AutoDeflate`\ , :config:option:`sd/device/AutoDeflateAlgorithm`\  and :config:option:`sd/device/AutoDeflateLevel`\  directives.

The inflate blocks, if enabled, will uncompress data if it is compressed using the algorithm that was used during compression.

The deflate blocks, if enabled, will compress uncompressed data with the algorithm and level configured in the according directives.

The series connection of the inflate and deflate function blocks makes the plugin very flexible.

Scenarios where this plugin can be used are for example:

-  client computers with weak cpus can do backups without compression and let the sd do the compression when writing to disk

-  compressed backups can be recompressed to a different compression format (e.g. gzip |rarr| lzo) using migration jobs

-  client backups can be compressed with compression algorithms that the client itself does not support

Multi-core cpus will be utilized when using parallel jobs as the compression is done in each jobs’ thread.

When the autoxflate plugin is configured, it will write some status information into the joblog.

.. code-block:: bareosmessage
   :caption: used compression algorithm

   autodeflation: compressor on device FileStorage is FZ4H

.. code-block:: bareosmessage
   :caption: configured inflation and deflation blocks

   autoxflate-sd.c: FileStorage OUT:[SD->inflate=yes->deflate=yes->DEV] IN:[DEV->inflate=yes->deflate=yes->SD]

.. code-block:: bareosmessage
   :caption: overall deflation/inflation ratio

   autoxflate-sd.c: deflate ratio: 50.59%

Additional :config:option:`sd/storage/AutoXFlateOnReplication`\  can be configured at the Storage resource.
