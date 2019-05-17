.. _MediaType:

This directive specifies the Media Type to be used to store the data. This is an arbitrary string of characters up to 127 maximum that you define. It can be anything you want. However, it is best to make it descriptive of the storage media (e.g. File, DAT, "HP DLT8000", 8mm, ...). In addition, it is essential that you make the Media Type specification unique for each storage media type. If you have two DDS-4 drives that have incompatible formats, or if you have a DDS-4 drive and a DDS-4
autochanger, you almost certainly should specify different Media Types. During a restore, assuming a DDS-4 Media Type is associated with the Job, Bareos can decide to use any Storage daemon that supports Media Type DDS-4 and on any drive that supports it.

If you are writing to disk Volumes, you must make doubly sure that each Device resource defined in the Storage daemon (and hence in the Director’s conf file) has a unique media type. Otherwise Bareos may assume, these Volumes can be mounted and read by any Storage daemon File device.

Currently Bareos permits only a single Media Type per Storage Device definition. Consequently, if you have a drive that supports more than one Media Type, you can give a unique string to Volumes with different intrinsic Media Type (Media Type = DDS-3-4 for DDS-3 and DDS-4 types), but then those volumes will only be mounted on drives indicated with the dual type (DDS-3-4).

If you want to tie Bareos to using a single Storage daemon or drive, you must specify a unique Media Type for that drive. This is an important point that should be carefully understood. Note, this applies equally to Disk Volumes. If you define more than one disk Device resource in your Storage daemon’s conf file, the Volumes on those two devices are in fact incompatible because one can not be mounted on the other device since they are found in different directories. For this reason, you probably
should use two different Media Types for your two disk Devices (even though you might think of them as both being File types). You can find more on this subject in the :ref:`Basic Volume Management <DiskChapter>` chapter of this manual.

The MediaType specified in the Director’s Storage resource, must correspond to the Media Type specified in the Device resource of the Storage daemon configuration file. This directive is required, and it is used by the Director and the Storage daemon to ensure that a Volume automatically selected from the Pool corresponds to the physical device. If a Storage daemon handles multiple devices (e.g. will write to various file Volumes on different partitions), this directive allows you to specify
exactly which device.

As mentioned above, the value specified in the Director’s Storage resource must agree with the value specified in the Device resource in the Storage daemon’s configuration file. It is also an additional check so that you don’t try to write data for a DLT onto an 8mm device. 
