If the grpc plugin is loaded, then the specified grpc module is used by the client to load plugins that it itself did not load, but were requested by the director.
In that case the `Plugin=` directives from the fileset interpreted as if they were prefixed by `grpc:<grpc module value>:`.

If this value is unset or if the grpc plugin is not loaded, then this is not attempted.

.. note::
   The `grpc-python-module` that is distributed alongside the grpc plugin is designed to be used in this manner.

The backups created this way are treated as if they were created by the requested plugin.
This means that they may be restored on a client that did not use this feature but loaded the plugin directly, but they cannot be restored on a client that uses the grpc plugin without this feature enabled.
Similarly you may restore backups this way that were created by the plugin directly.
