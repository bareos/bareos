Send the message to the system log (syslog).

Since :sinceVersion:`14.4.0: Syslog Level` the facility can be specified in the address field and the loglevel correspond to the Bareos :ref:`MessageTypes`. The defaults are ``DAEMON`` and ``LOG_ERR``.

Although the syslog destination is not used in the default Bareos config files, in certain cases where Bareos encounters errors in trying to deliver a message, as a last resort, it will send it to the system syslog to prevent loss of the message, so you might occassionally check the syslog for Bareos output.

