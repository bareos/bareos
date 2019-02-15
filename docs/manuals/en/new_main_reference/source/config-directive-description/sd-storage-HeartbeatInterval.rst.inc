:index:`[TAG=Broken pipe] <single: Broken pipe>` This directive defines an interval of time in seconds. When the Storage daemon is waiting for the operator to mount a tape, each time interval, it will send a heartbeat signal to the File daemon. The default interval is zero which disables the heartbeat. This feature is particularly useful if you have a router that does not follow Internet standards and times out an valid connection after a short duration despite the fact that keepalive is set. This
usually results in a broken pipe error message.

