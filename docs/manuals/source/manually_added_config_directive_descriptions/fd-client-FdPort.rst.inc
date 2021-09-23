This specifies the port number on which the Client listens for Director connections. It must agree with the FDPort specified in the Client resource of the Director’s configuration file.

By default, the Client will listen to both IPv6 and IPv4 default addresses on the port you set. If you want to listen on either IPv4 or IPv6 only, you have to specify it with either :config:option:`fd/client/FdAddress`\ , or remove :config:option:`fd/client/FdPort`\ and just use :config:option:`fd/client/FdAddresses`\ instead.
