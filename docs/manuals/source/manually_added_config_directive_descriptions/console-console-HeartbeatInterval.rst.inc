This directive is optional and if specified will cause the Console to set a keepalive interval (heartbeat) in seconds on each of the sockets to communicate with the Director. It is implemented only on systems (Linux, ...) that provide the setsockopt TCP_KEEPIDLE function. If the value is set to 0 (zero), no change is made to the socket.

