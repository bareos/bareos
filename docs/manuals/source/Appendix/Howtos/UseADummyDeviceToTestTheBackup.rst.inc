.. _dummydevice:

Use a dummy device to test the backup
-------------------------------------



.. _TestUsingFifoDevice:

 If your are testing your configuration, but don’t want to store the backup data, it is possible to use a dummy FIFO device to test your configuration, see :ref:`Stored configuration <SetupFifo>`.

Obviously, it can not be used to do a restore.

.. code-block:: bareosconfig
   :caption: FIFO Storage Device Configuration

   Device {
     Name = NULL
     Media Type = NULL
     Device Type = Fifo
     Archive Device = /dev/null
     LabelMedia = yes
     Random Access = no
     AutomaticMount = no
     RemovableMedia = no
     MaximumOpenWait = 60
     AlwaysOpen = no
   }
