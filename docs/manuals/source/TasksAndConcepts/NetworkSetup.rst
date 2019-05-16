Network setup
=============

.. _section-ClientInitiatedConnection:

Client Initiated Connection
---------------------------

The |dir| knows, when it is required to talk to a client (|fd|). Therefore, by defaults, the |dir| connects to the clients.

However, there are setups where this can cause problems, as this means that:

-  The client must be reachable by its configured :config:option:`dir/client/Address`\ . Address can be the DNS name or the IP address. (For completeness: there are potential workarounds by using the :ref:`setip <bcommandSetIP>` command.)

-  The |dir| must be able to connect to the |fd| over the network.

To circumvent these problems, since Bareos :sinceVersion:`16.2.2: Client Initiated Connection` it is possible to let the |fd| initiate the network connection to the |dir|.

Which address the client connects to reach the |dir| is configured in the :config:option:`fd/director/Address`\  directive.

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

Using Client Initiated Connections has disadvantages. Without Client Initiated Connections the |dir| only establishes a network connection when this is required. With Client Initiated Connections, the |fd| connects to the |dir| and the |dir| keeps these connections open. The command :bcommand:`status dir` will show all waiting connections:

.. code-block:: bconsole
   :caption: show waiting client connections

   *<input>status dir</input>
   ...
   Client Initiated Connections (waiting for jobs):
   Connect time        Protocol            Authenticated       Name
   ====================================================================================================
   19-Apr-16 21:50     54                  1                   client1.example.com
   ...
   ====

When both connection directions are allowed, the |dir| 

#. checks, if there is a waiting connection from this client.

#. tries to connect to the client (using the usual timeouts).

#. waits for a client connection to appear (using the same timeout as when trying to connect to a client).

If none of this worked, the job fails.

When a waiting connection is used for a job, the |fd| will detect this and creates an additional connection. This is required, to keep the client responsive for additional commands, like :bcommand:`cancel`.

To get feedback in case the |fd| fails to connect to the |dir|, consider configuring |fd| to log in a local file. This can be archived by adding the line

``Append = "/var/log/bareos/bareos-fd.log" = all, !skipped, !restored``

to the default message resource :config:option:`Fd/Messages = Standard`\ :

.. code-block:: bareosconfig
   :caption: bareos-fd.d/messages/Standard.conf

   Messages {
     Name = Standard
     Director = bareos-dir = all, !skipped, !restored
     Append = "/var/log/bareos/bareos-fd.log" = all, !skipped, !restored
   }

.. _PassiveClient:

Passive Clients
---------------

The normal way of initializing the data channel (the channel where the backup data itself is transported) is done by the |fd| (client) that connects to the |sd|.

In many setups, this can cause problems, as this means that:

-  The client must be able to resolve the name of the |sd| (often not true, you have to do tricks with the hosts file)

-  The client must be allowed to create a new connection.

-  The client must be able to connect to the |sd| over the network (often difficult over NAT or Firewall)

By using Passive Client, the initialization of the datachannel is reversed, so that the storage daemon connects to the |fd|. This solves almost every problem created by firewalls, NAT-gateways and resolving issues, as

-  The |sd| initiates the connection, and thus can pass through the same or similar firewall rules that the director already has to access the |fd|.

-  The client never initiates any connection, thus can be completely firewalled.

-  The client never needs any name resolution and is totally independent from any resolving issues.

.. image:: /include/images/passive-client-communication.*
   :width: 60.0%




Usage
~~~~~

To use this new feature, just configure :config:option:`dir/client/Passive`\ =yes in the client definition of the |dir|:

.. code-block:: bareosconfig
   :caption: Enable passive mode in bareos-dir.conf

   Client {
      Name = client1-fd
      Password = "secretpassword"
      <input>Passive = yes</input>
      [...]
   }

Also, prior to bareos version 15, you need to set :config:option:`fd/client/Compatible`\ =no in the :file:`bareos-fd.conf` configuration file. Since Bareos Version 15, the compatible option is set to no per default and does not need to be specified anymore.

.. code-block:: bareosconfig
   :caption: Disable compatible mode for the |fd| in bareos-fd.conf

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

:index:`\ <single: Lan Address>`\ 

Bareos supports network topologies where the |fd| and |sd| are situated inside of a LAN, but the |dir| is outside of that LAN in the Internet and accesses the |fd| and |sd| via SNAT / port forwarding.

Consider the following scheme:

.. code-block:: shell-session

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

The |dir| can access the :strong:`FD_LAN` via the IP 8.8.8.10, which is forwarded to the IP 10.0.0.10 inside of the LAN.

The |dir| can access the :strong:`SD_LAN` via the IP 8.8.8.20 which is forwarded to the IP 10.0.0.20 inside of the LAN.

There is also a |fd| and a |sd| outside of the LAN, which have the IPs 8.8.8.30 and 8.8.8.40

All resources are configured so that the :strong:`Address`\  directive gets the address where the |dir| can reach the daemons.

Additionally, devices being in the LAN get the LAN address configured in the :strong:`Lan Address`\  directive. The configuration looks as follows:

.. code-block:: bareosconfig
   :caption: bareos-dir.d/client/FD\_LAN.conf

   Client {
      Name = FD_LAN
      Address = 8.8.8.10
      LanAddress = 10.0.0.10
      ...
   }

.. code-block:: bareosconfig
   :caption: bareos-dir.d/client/SD\_LAN.conf

   Storage {
      Name = SD_LAN
      Address = 8.8.8.20
      LanAddress = 10.0.0.20
      ...
   }

.. code-block:: bareosconfig
   :caption: bareos-dir.d/client/FD\_WAN.conf

   Client {
      Name = FD_WAN
      Address = 8.8.8.30
      ...
   }

.. code-block:: bareosconfig
   :caption: bareos-dir.d/client/SD\_WAN.conf

   Storage {
      Name = SD_WAN
      Address = 8.8.8.40
      ...
   }

This way, backups and restores from each |fd| using each |sd| are possible as long as the firewall allows the needed network connections.

The |dir| simply checks if both the involved |fd| and |sd| both have a :strong:`Lan Address`\  (:config:option:`dir/client/LanAddress`\  and :config:option:`dir/storage/LanAddress`\ ) configured.

In that case, the initiating daemon is ordered to connect to the :strong:`Lan Address`\  instead of the :strong:`Address`\ . In active client mode, the |fd| connects to the |sd|, in passive client mode (see :ref:`PassiveClient`) the |sd| connects to the |fd|.

If only one or none of the involved |fd| and |sd| have a :strong:`Lan Address`\  configured, the :strong:`Address`\  is used as connection target for the initiating daemon.

.. _ConnectionOverviewReference:

Network Connections Overview
============================
The following diagrams show Bareos components with any possible
network connections between them. Arrows point always from the TCP
Client to the respective TCP Server, thus the direction of the connection
initiation. This is not neccessarily the direction of the data flow. 

Full connection overview
------------------------
This diagram contains all possible connections between Bareos components
that are virtually usable. See the chapters :ref:`below for specific diagrams <ConnecionOverviewNamedConsoleAndDefaultConsole>` of the Bareos operating modes.

.. uml::
  :caption: Sequence diagram of a Bareos File Daemon connection

  left to right direction
  skinparam shadowing false

  (Console\nPython\nWebUI) as Con
  (Tray Monitor) as Tray

  [Filedaemon] as FD
  [Directordaemon] as Dir
  [Storagedaemon] as SD
  [Storagedaemon2] as SD2

  database Catalog

  !define arrow_hidden(from,direction,to,comment) from -[#white]direction->to : <color white>comment</color>

  !define arrow(from,direction,to,comment) from -direction->to : comment

  arrow(Con, right, Dir, 1)
  arrow(Con, right, Dir, 2)

  arrow(Dir, up, FD, 3)
  arrow(FD, down, Dir, 4)

  arrow(Dir, right, SD, 5a)

  arrow(FD, down, SD, 6)
  arrow(SD, down, FD, 7)

  arrow(SD, down, SD2, 8)
  arrow(Dir, down, SD2, 5b)

  arrow(Tray, down, Dir, 9)
  arrow(Tray, down, FD, 10)
  arrow(Tray, down, SD, 11)

  arrow(Dir, down, Catalog, 12)

.. _LegendForFullConnectionOverviewReference:

.. csv-table:: Legend for full connection overview
   :header: "Connection Number", "Description", "Type"
   :widths: auto

    1, "Named Console", "control channel"
    2, "Default Console", "control channel"
    3, "**Director to File Daemon (default)**", "control channel"
    4, ":ref:`Client initiated <section-ClientInitiatedConnection>` File Daemon to Director", "control channel"
   5a, "**Director to Storage (default)**", "control channel"
   5b, "Director to 2nd Storage doing SD-SD copy or migrate", "control channel"
    6, "**File Daemon to Storage Daemon (default)**", "data channel"
    7, ":ref:`Passive Client <PassiveClient>` Storage Daemon to File Daemon", "data channel"
    8, "Storage Daemon to Storage Daemon", "data channel"
    9, "Tray Monitor to Director Daemon", "monitor channel"
   10, "Tray Monitor to File Daemon", "monitor channel"
   11, "Tray Monitor to Storage Daemon", "monitor channel"
   12, "Director to catalog database", "database channel"

.. _ConnecionOverviewNamedConsoleAndDefaultConsole:

Named Console and Default Console
---------------------------------

.. _ConnectionDiagramNamedAndDefaultConsole:

.. uml::
  :caption: Diagram of Console to Director connection

  left to right direction
  skinparam shadowing false

  (Console\nPython\nWebUI) as Con
  (Tray Monitor) as Tray

  [Filedaemon] as FD
  [Directordaemon] as Dir
  [Storagedaemon] as SD
  [Storagedaemon2] as SD2

  !define arrow_hidden(from,direction,to,comment) from -[#white]direction->to : <color white>comment</color>

  !define arrow(from,direction,to,comment) from -direction->to : comment

  arrow(Con, right, Dir, 1)
  arrow(Con, right, Dir, 2)

  arrow_hidden(Dir, up, FD, 3)
  arrow_hidden(FD, down, Dir, 4)

  arrow_hidden(Dir, right, SD, 5a)

  arrow_hidden(FD, down, SD, 6)
  arrow_hidden(SD, down, FD, 7)

  arrow_hidden(SD, down, SD2, 8)
  arrow_hidden(Dir, down, SD2, 5b)

  arrow_hidden(Tray, down, Dir, 9)
  arrow_hidden(Tray, down, FD, 10)
  arrow_hidden(Tray, down, SD, 11)

Default Backup/Restore
----------------------

.. _ConnectionDiagramDefaultBackupOrRestoreOperation:

.. uml::
  :caption: Diagram of a default Backup or Restore operation

  left to right direction
  skinparam shadowing false

  (Console\nPython\nWebUI) as Con
  (Tray Monitor) as Tray

  [Filedaemon] as FD
  [Directordaemon] as Dir
  [Storagedaemon] as SD
  [Storagedaemon2] as SD2

  !define arrow_hidden(from,direction,to,comment) from -[#white]direction->to : <color white>comment</color>

  !define arrow(from,direction,to,comment) from -direction->to : comment

  arrow_hidden(Con, right, Dir, 1)
  arrow_hidden(Con, right, Dir, 2)

  arrow(Dir, up, FD, 3)
  arrow_hidden(FD, down, Dir, 4)

  arrow(Dir, right, SD, 5a)

  arrow(FD, down, SD, 6)
  arrow_hidden(SD, down, FD, 7)

  arrow_hidden(SD, down, SD2, 8)
  arrow_hidden(Dir, down, SD2, 5b)

  arrow_hidden(Tray, down, Dir, 9)
  arrow_hidden(Tray, down, FD, 10)
  arrow_hidden(Tray, down, SD, 11)

Client Initiated Backup/Restore
-------------------------------

.. _ConnectionDiagramClientInitiatedBackupOrRestoreOperation:

.. uml::
  :caption: Diagram of a **client initiated** Backup or Restore operation

  left to right direction
  skinparam shadowing false

  (Console\nPython\nWebUI) as Con
  (Tray Monitor) as Tray

  [Filedaemon] as FD
  [Directordaemon] as Dir
  [Storagedaemon] as SD
  [Storagedaemon2] as SD2

  !define arrow_hidden(from,direction,to,comment) from -[#white]direction->to : <color white>comment</color>

  !define arrow(from,direction,to,comment) from -direction->to : comment

  arrow_hidden(Con, right, Dir, 1)
  arrow_hidden(Con, right, Dir, 2)

  arrow_hidden(Dir, up, FD, 3)
  arrow(FD, down, Dir, 4)

  arrow(Dir, right, SD, 5a)

  arrow(FD, down, SD, 6)
  arrow_hidden(SD, down, FD, 7)

  arrow_hidden(SD, down, SD2, 8)
  arrow_hidden(Dir, down, SD2, 5b)

  arrow_hidden(Tray, down, Dir, 9)
  arrow_hidden(Tray, down, FD, 10)
  arrow_hidden(Tray, down, SD, 11)

Passive Client Backup/Restore
-----------------------------

.. _ConnectionDiagramPassiveClientBackupOrRestoreOperation:

.. uml::
  :caption: Diagram of a **passive client** Backup or Restore operation

  left to right direction
  skinparam shadowing false

  (Console\nPython\nWebUI) as Con
  (Tray Monitor) as Tray

  [Filedaemon] as FD
  [Directordaemon] as Dir
  [Storagedaemon] as SD
  [Storagedaemon2] as SD2

  !define arrow_hidden(from,direction,to,comment) from -[#white]direction->to : <color white>comment</color>

  !define arrow(from,direction,to,comment) from -direction->to : comment

  arrow_hidden(Con, right, Dir, 1)
  arrow_hidden(Con, right, Dir, 2)

  arrow(Dir, up, FD, 3)
  arrow_hidden(FD, down, Dir, 4)

  arrow(Dir, right, SD, 5a)

  arrow_hidden(FD, down, SD, 6)
  arrow(SD, down, FD, 7)

  arrow_hidden(SD, down, SD2, 8)
  arrow_hidden(Dir, down, SD2, 5b)

  arrow_hidden(Tray, down, Dir, 9)
  arrow_hidden(Tray, down, FD, 10)
  arrow_hidden(Tray, down, SD, 11)

Storage-Storage Migration
-------------------------

.. _ConnectionDiagramStorageToStorageCopyOrMigrateOperation:

.. uml::
  :caption: Diagram of a Storage to Storage copy or migrate operation

  left to right direction
  skinparam shadowing false

  (Console\nPython\nWebUI) as Con
  (Tray Monitor) as Tray

  [Filedaemon] as FD
  [Directordaemon] as Dir
  [Storagedaemon] as SD
  [Storagedaemon2] as SD2

  !define arrow_hidden(from,direction,to,comment) from -[#white]direction->to : <color white>comment</color>

  !define arrow(from,direction,to,comment) from -direction->to : comment

  arrow_hidden(Con, right, Dir, 1)
  arrow_hidden(Con, right, Dir, 2)

  arrow_hidden(Dir, up, FD, 3)
  arrow_hidden(FD, down, Dir, 4)

  arrow(Dir, right, SD, 5a)

  arrow_hidden(FD, down, SD, 6)
  arrow_hidden(SD, down, FD, 7)

  arrow(SD, down, SD2, 8)
  arrow(Dir, down, SD2, 5b)

  arrow_hidden(Tray, down, Dir, 9)
  arrow_hidden(Tray, down, FD, 10)
  arrow_hidden(Tray, down, SD, 11)

Tray-Monitor
------------

.. _ConnectionDiagramAllTrayMonitorConnections:

.. uml::
  :caption: Diagram of all Tray Monitor Connections

  left to right direction
  skinparam shadowing false

  (Console\nPython\nWebUI) as Con
  (Tray Monitor) as Tray

  [Filedaemon] as FD
  [Directordaemon] as Dir
  [Storagedaemon] as SD
  [Storagedaemon2] as SD2

  !define arrow_hidden(from,direction,to,comment) from -[#white]direction->to : <color white>comment</color>

  !define arrow(from,direction,to,comment) from -direction->to : comment

  arrow_hidden(Con, right, Dir, 1)
  arrow_hidden(Con, right, Dir, 2)

  arrow_hidden(Dir, up, FD, 3)
  arrow_hidden(FD, down, Dir, 4)

  arrow_hidden(Dir, right, SD, 5a)

  arrow_hidden(FD, down, SD, 6)
  arrow_hidden(SD, down, FD, 7)

  arrow_hidden(SD, down, SD2, 8)
  arrow_hidden(Dir, down, SD2, 5b)

  arrow(Tray, down, Dir, 9)
  arrow(Tray, down, FD, 10)
  arrow(Tray, down, SD, 11)


