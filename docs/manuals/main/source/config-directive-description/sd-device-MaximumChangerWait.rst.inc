This directive specifies the maximum amount of time that Bareos will wait for the changer to respond to a command (e.g. load). If you have a slow autoloader you may want to set it longer.

If the autoloader program fails to respond in this time, Bareos will invalidate the volume slot number stored in the catalog and try again. If no additional changer volumes exist, Bareos will ask the operator to intervene.

