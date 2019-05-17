The normal way of initializing the data channel (the channel where the backup data itself is transported) is done by the file daemon (client) that connects to the storage daemon.

By using the client passive mode, the initialization of the datachannel is reversed, so that the storage daemon connects to the filedaemon.

See chapter :ref:`Passive Client <PassiveClient>`.

