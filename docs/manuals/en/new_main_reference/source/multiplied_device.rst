.. This chapter is in preparation to be a subchapter of the _StorageResourceDevice chapter.

.. _StorageResourceMultipliedDevice:

Multiplied Device
~~~~~~~~~~~~~~~~~

The Multiplied Device feature can be used when multiple identical devices are needed.
In this case the :config:option:`sd/device/Count` can be added to the regarding Device resource.

When the configuration is loaded the |bareosSD| will then automatically multiply this device :config:option:`sd/device/Count` times. The number of multiplied devices includes the original Device.

A number "0001" will be appended to name of the initial Device. All other multiplied Devices have increasing numbers "0002", "0003", accordingly. In the example below the name of the multiplied devices will be "MultiFileStorage0001", "MultiFileStorage0002", and so on.

.. code-block:: bareosconfig
   :caption: bareos-sd.d/device/multiplied_device.conf

   Device {
     #Multiply this device Count times
     Count = 3

     Name = MultiFileStorage
     Media Type = File
     Archive Device = /home/testuser/multiplied-device-test/storage
     LabelMedia = yes                   # lets Bareos label unlabeled media
     Random Access = yes
     AutomaticMount = yes               # when device opened, read it
     RemovableMedia = no
     AlwaysOpen = no
     Description = "File device. Will be multiplied 3 times"
   }

In the |bareosDir| any of the Multiplied Devices can be referred to using their numbered names.

However, in the autochanger resource of the |bareosSD| the original name of the initial Multiplied Device Resource can be used.

.. code-block:: bareosconfig
   :caption: bareos-sd.d/autochanger/autochanger.conf

   Autochanger {
     Name = "virtual-autochanger"

     # list here only the name of the initial multiplied device resource
     Device = MultiFileStorage

     Changer Device = /dev/null
     Changer Command = ""
   }

When the configuration is exported, again only the name of the initial Multiplied Device Resource will be printed.

