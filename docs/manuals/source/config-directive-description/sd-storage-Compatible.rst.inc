This directive enables the compatible mode of the storage daemon. In this mode the storage daemon will try to write the storage data in a compatible way with Bacula of which Bareos is a fork. This only works for the data streams both share and not for any new datastreams which are Bareos specific. Which may be read when used by a Bareos storage daemon but might not be understood by any of the Bacula components (dir/sd/fd).

The default setting of this directive was changed to no since Bareos :sinceVersion:`15.2.0: Compatible = no`.

