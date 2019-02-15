The :strong:`Maximum Block Size` can be defined here to define different block sizes per volume or statically for all volumes at :config:option:`sd/device/MaximumBlockSize`\ . If not defined, its default is 63 KB. Increasing this value could improve the throughput of writing to tapes.



.. warning::
   However make sure to read the :ref:`Setting Block Sizes <setblocksizes>` chapter carefully before applying any changes.
