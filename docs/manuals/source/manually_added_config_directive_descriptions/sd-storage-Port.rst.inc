Specifies port number on which the Storage daemon listens for Director connections.

By default, the Storage Daemon will listen to both IPv6 and IPv4 default addresses on the port you set. If you want to listen on either IPv4 or IPv6 only, you have to specify it with either :config:option:`sd/storage/Address`\ , or remove :config:option:`sd/storage/Port`\ and just use :config:option:`sd/storage/Addresses`\ instead.
