This specifies an external program to be called at the completion of each Job after the device is released. The purpose of this command is to check for Tape Alerts, which are present when something is wrong with your tape drive (at least for most modern tape drives). The same substitution characters that may be specified in the Changer Command may also be used in this string. For more information, see the :ref:`AutochangersChapter` chapter.

Note, it is not necessary to have an autochanger to use this command. The example below uses the :command:`tapeinfo` program that comes with the :command:`mtx` package, but it can be used on any tape drive. However, you will need to specify a :config:option:`sd/device/ChangerDevice`\  directive so that the generic SCSI device name can be edited into the command (with the %c).

An example of the use of this command to print Tape Alerts in the Job report is: 

.. literalinclude:: /_static/config/SdDeviceAlertCommand1.conf



and an example output when there is a problem could be: 

.. literalinclude:: /_static/config/SdDeviceAlertCommand2.conf



