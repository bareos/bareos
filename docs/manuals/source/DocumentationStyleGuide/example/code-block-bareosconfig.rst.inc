.. code-block:: bareosconfig
   :caption: bareos-sd.d/device/FileStorage.conf
    
   Device {
     Name = FileStorage
     ...
     Archive Device = /var/lib/bareos/storage
     ...
   }
