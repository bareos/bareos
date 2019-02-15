Specify the ports and addresses on which the File daemon listens for Director connections. Probably the simplest way to explain is to show an example:



   .. literalinclude:: /_static/config/FdClientFDAddresses1.conf



where ip, ip4, ip6, addr, and port are all keywords. Note, that the address can be specified as either a dotted quadruple, or IPv6 colon notation, or as a symbolic name (only in the ip specification). Also, the port can be specified as a number or as the mnemonic value from the /etc/services file. If a port is not specified, the default will be used. If an ip section is specified, the resolution can be made either by IPv4 or IPv6. If ip4 is specified, then only IPv4 resolutions will be
permitted, and likewise with ip6.

