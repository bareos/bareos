Reservation
===========

Introduction
------------
After the director set up a job on the SD it will then issue a "use" command to tell the SD what storage device it wants to use.
The SD then checks the list of devices the director sent and tries to reserve a suitable device.
In the end the SD either tells the director which device to use or that there is no device available for the job.

There is a so-called reservation context that is passed to every function that has a swimlane in this diagram.
This context is basically a bunch of flags that are set and cleared to change the behaviour of the rest of the code.

For example CanReserveDevice() from the second diagram sets the flag low_use_drive which is then evaluated in UseDeviceCmd().

Diagram Conventions
~~~~~~~~~~~~~~~~~~~
Each swimlane denotes a separate function.

.. uml:: reservation/legend.puml

UseDeviceCmd
~~~~~~~~~~~~
This function reads the list of storages and devices the director is willing to use for this job.
Afterwards several different methods of finding a device to reseve are used.
If no device could be reserved the function waits for up to a minute or until ReleaseDeviceCond() is called and then tries again.

.. uml:: reservation/UseDeviceCmd.puml

ReserveDevice
~~~~~~~~~~~~~
Here we see wether the media type matches and actually try to open the device.
The actual reservation is delegated to ReserveDeviceForRead() or ReserveDeviceForAppend().
While the first one is more or less trivial, the latter one is really complicated.

.. uml:: reservation/ReserveDevice.puml
