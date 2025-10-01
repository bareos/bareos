This specifies an external program to be called at the completion of each Job
after the device is released.
The purpose of this command is to check for TapeAlerts, which are present when
something is wrong with your tape drive (at least for most modern tape drives).
The same substitution characters that may be specified in the Changer Command
may also be used in this string. For more information, see the
:ref:`AutochangersChapter` chapter.

Note, it is not necessary to have an autochanger to use this command.
The example below uses the :command:`tapeinfo` program that comes with the
:command:`mtx` package, but it can be used on any tape drive.
However, you will need to specify a
:config:option:`sd/device/ChangerDevice`\  directive so that the generic SCSI
device name can be edited into the command (with the %c).

An example of the use of this command to print TapeAlerts in the Job report is:

.. literalinclude:: /include/config/SdDeviceAlertCommand1.conf
   :language: bareosconfig



and an example output when there is a problem could be:

.. literalinclude:: /include/config/SdDeviceAlertCommand2.conf
   :language: bareosconfig

.. warning::

   When the user running the command (i.e. the user running the |sd|) lacks
   proper privileges to retrieve TapeAlert flags, ``tapeinfo`` will silently
   skip TapeAlert retrieval. As a result **no alerts will be reported**.

.. note::

   This method of TapeAlert retrieval is inferior to the :ref:`scsitapealert-sd`
   plugin, because it only runs when a job finishes and is usually unable to
   warn you when retrieval fails.
