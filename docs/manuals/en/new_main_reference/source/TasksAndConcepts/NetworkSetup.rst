Network setup
=============

.. _section-ClientInitiatedConnection:

Client Initiated Connection
---------------------------

The |bareosDir| knows, when it is required to talk to a client (|bareosFd|). Therefore, by defaults, the |bareosDir| connects to the clients.

However, there are setups where this can cause problems, as this means that:

-  The client must be reachable by its configured :config:option:`dir/client/Address`\ . Address can be the DNS name or the IP address. (For completeness: there are potential workarounds by using the :ref:`setip <bcommandSetIP>` command.)

-  The |bareosDir| must be able to connect to the |bareosFd| over the network.

To circumvent these problems, since Bareos :index:`Version >= 16.2.2 <pair: bareos-16.2.2; Client Initiated Connection>` it is possible to let the |bareosFd| initiate the network connection to the |bareosDir|.

Which address the client connects to reach the |bareosDir| is configured in the :config:option:`fd/director/Address`\  directive.

To additional allow this connection direction use:

-  :config:option:`dir/client/ConnectionFromClientToDirector`\  = yes

-  :config:option:`dir/client/HeartbeatInterval`\  = 60 ``#`` to keep the network connection established

-  :config:option:`fd/director/ConnectionFromClientToDirector`\  = yes

To only allow Connection From the Client to the Director use:

-  :config:option:`dir/client/ConnectionFromDirectorToClient`\  = no

-  :config:option:`dir/client/ConnectionFromClientToDirector`\  = yes

-  :config:option:`dir/client/HeartbeatInterval`\  = 60 ``#`` to keep the network connection established

-  :config:option:`fd/director/ConnectionFromDirectorToClient`\  = no

-  :config:option:`fd/director/ConnectionFromClientToDirector`\  = yes

Using Client Initiated Connections has disadvantages. Without Client Initiated Connections the |bareosDir| only establishes a network connection when this is required. With Client Initiated Connections, the |bareosFd| connects to the |bareosDir| and the |bareosDir| keeps these connections open. The command :strong:`status dir` will show all waiting connections:

.. code-block:: sh
   :caption: show waiting client connections

   *<input>status dir</input>
   ...
   Client Initiated Connections (waiting for jobs):
   Connect time        Protocol            Authenticated       Name
   ====================================================================================================
   19-Apr-16 21:50     54                  1                   client1.example.com
   ...
   ====

When both connection directions are allowed, the |bareosDir| 

#. checks, if there is a waiting connection from this client.

#. tries to connect to the client (using the usual timeouts).

#. waits for a client connection to appear (using the same timeout as when trying to connect to a client).

If none of this worked, the job fails.

When a waiting connection is used for a job, the |bareosFd| will detect this and creates an additional connection. This is required, to keep the client responsive for additional commands, like :strong:`cancel`.

To get feedback in case the |bareosFd| fails to connect to the |bareosDir|, consider configuring |bareosFd| to log in a local file. This can be archived by adding the line

:strong:`Append = "/var/log/bareos/bareos-fd.log" = all, !skipped, !restored`

to the default message resource **Standard**:sup:`Fd`:sub:`Messages` :

.. code-block:: sh
   :caption: bareos-fd.d/messages/Standard.conf

   Messages {
     Name = Standard
     Director = bareos-dir = all, !skipped, !restored
     Append = "/var/log/bareos/bareos-fd.log" = all, !skipped, !restored
   }

.. _PassiveClient:

Passive Clients
---------------

The normal way of initializing the data channel (the channel where the backup data itself is transported) is done by the |bareosFd| (client) that connects to the |bareosSd|.

In many setups, this can cause problems, as this means that:

-  The client must be able to resolve the name of the |bareosSd| (often not true, you have to do tricks with the hosts file)

-  The client must be allowed to create a new connection.

-  The client must be able to connect to the |bareosSd| over the network (often difficult over NAT or Firewall)

By using Passive Client, the initialization of the datachannel is reversed, so that the storage daemon connects to the |bareosFd|. This solves almost every problem created by firewalls, NAT-gateways and resolving issues, as

-  The |bareosSd| initiates the connection, and thus can pass through the same or similar firewall rules that the director already has to access the |bareosFd|.

-  The client never initiates any connection, thus can be completely firewalled.

-  The client never needs any name resolution and is totally independent from any resolving issues.

.. image:: /include/images/passive-client-communication.*
   :width: 60.0%




Usage
~~~~~

To use this new feature, just configure :config:option:`dir/client/Passive`\ =yes in the client definition of the |bareosDir|:

.. code-block:: sh
   :caption: Enable passive mode in bareos-dir.conf

   Client {
      Name = client1-fd
      Password = "secretpassword"
      <input>Passive = yes</input>
      [...]
   }

Also, prior to bareos version 15, you need to set :config:option:`fd/client/Compatible`\ =no in the :file:`bareos-fd.conf` configuration file. Since Bareos Version 15, the compatible option is set to no per default and does not need to be specified anymore.

.. code-block:: sh
   :caption: Disable compatible mode for the |bareosFd| in bareos-fd.conf

   Director {
     Name = bareos-dir
     Password = "secretpassword"
   }

   Client {
      Name = client1-fd
      [...]
      <input>Compatible = no</input>
   }

.. _LanAddress:

Using different IP Adresses for SD – FD Communication
-----------------------------------------------------

:index:`[TAG=Lan Address] <single: Lan Address>`

Bareos supports network topologies where the |bareosFd| and |bareosSd| are situated inside of a LAN, but the |bareosDir| is outside of that LAN in the Internet and accesses the |bareosFd| and |bareosSd| via SNAT / port forwarding.

Consider the following scheme:

.. code-block:: sh

      /-------------------\
      |                   |    LAN 10.0.0.1/24
      |                   |
      |  FD_LAN   SD_LAN  |
      |  .10         .20  |
      |                   |
      \___________________/
                |
            NAT Firewall
            FD: 8.8.8.10 -> 10.0.0.10
            SD: 8.8.8.20 -> 10.0.0.20
                |
      /-------------------\
      |                   |
      |                   |     WAN / Internet
      |        DIR        |
      |     8.8.8.100     |
      |                   |
      | FD_WAN   SD_WAN   |
      | .30         .40   |
      \___________________/

The |bareosDir| can access the :strong:`FD_LAN` via the IP 8.8.8.10, which is forwarded to the IP 10.0.0.10 inside of the LAN.

The |bareosDir| can access the :strong:`SD_LAN` via the IP 8.8.8.20 which is forwarded to the IP 10.0.0.20 inside of the LAN.

There is also a |bareosFd| and a |bareosSd| outside of the LAN, which have the IPs 8.8.8.30 and 8.8.8.40

All resources are configured so that the :strong:`Address` directive gets the address where the |bareosDir| can reach the daemons.

Additionally, devices being in the LAN get the LAN address configured in the :strong:`Lan Address` directive. The configuration looks as follows:

.. code-block:: sh
   :caption: bareos-dir.d/client/FD\_LAN.conf

   Client {
      Name = FD_LAN
      Address = 8.8.8.10
      LanAddress = 10.0.0.10
      ...
   }

.. code-block:: sh
   :caption: bareos-dir.d/client/SD\_LAN.conf

   Storage {
      Name = SD_LAN
      Address = 8.8.8.20
      LanAddress = 10.0.0.20
      ...
   }

.. code-block:: sh
   :caption: bareos-dir.d/client/FD\_WAN.conf

   Client {
      Name = FD_WAN
      Address = 8.8.8.30
      ...
   }

.. code-block:: sh
   :caption: bareos-dir.d/client/SD\_WAN.conf

   Storage {
      Name = SD_WAN
      Address = 8.8.8.40
      ...
   }

This way, backups and restores from each |bareosFd| using each |bareosSd| are possible as long as the firewall allows the needed network connections.

The |bareosDir| simply checks if both the involved |bareosFd| and |bareosSd| both have a :strong:`Lan Address` (:config:option:`dir/client/LanAddress`\  and :config:option:`dir/storage/LanAddress`\ ) configured.

In that case, the initiating daemon is ordered to connect to the :strong:`Lan Address` instead of the :strong:`Address`. In active client mode, the |bareosFd| connects to the |bareosSd|, in passive client mode (see :ref:`PassiveClient`) the |bareosSd| connects to the |bareosFd|.

If only one or none of the involved |bareosFd| and |bareosSd| have a :strong:`Lan Address` configured, the :strong:`Address` is used as connection target for the initiating daemon.




