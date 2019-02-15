If ``yes``, permits the daemon to examine the device to determine if it contains a Bareos labeled volume. This is done initially when the daemon is started, and then at the beginning of each job. This directive is particularly important if you have set :config:option:`sd/device/AlwaysOpen`\ = **no** because it permits Bareos to attempt to read the device before asking the system operator to mount a tape. However, please note that the tape must be
mounted before the job begins.

