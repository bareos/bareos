This directive specifies the command that must be executed to unmount devices such as many USB devices. Before the command is executed, %a is replaced with the Archive Device, and %m with the Mount Point.

Most frequently, you will define it as follows:



   .. literalinclude:: /_static/config/SdDeviceUnmountCommand1.conf



See the :ref:`mountcodes` section below for more details of the editing codes that can be used in this directive.

If you need to specify multiple commands, create a shell script.

