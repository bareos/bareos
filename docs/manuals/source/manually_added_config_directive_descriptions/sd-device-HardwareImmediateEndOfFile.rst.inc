Controls whether the tape device attempts immediate filemarks. Enabled by
default since Bareos :sinceVersion:`26: Immediate tape filemarks`. Immediate
filemarks do not wait for the drive's buffer to flush, unlike regular
filemarks. Set this to ``no`` if the drive must use regular filemarks.

If an immediate filemark fails, Bareos retries with a regular filemark.
When the drive reports that immediate filemarks are unsupported
(``ENOTTY`` or ``ENOSYS``), Bareos also disables immediate filemarks on
that device for subsequent writes until the device is recreated. A regular
filemark may flush the drive's buffer and reduce write speed.
