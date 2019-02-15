.. _ConnectionOverviewReference:

Connection Overview
===================
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


.. _TLSConfigurationReferenceChapter:

TLS Configuration Reference
===========================

To be able to communicate via TLS, TLS needs to be configured on both sides. In Bareos certain directives are used to set up TLS.

The following table explains the location of the relevant TLS configuration directives for all possible Bareos TCP connections. Each resource is referred to as <component>-<resource> to identify the exact configuration location. Refer to chapter :ref:`ConfigureChapter` for more details about configuration.

In Bareos Version 18.2 the relevant resources for some connections had to be changed. Affected directives are marked with the applicable version and the respective resource is written in bold letters.

*Remark: TLS-PSK is not available on Bareos components before Version 18.2.*

 .. csv-table:: TLS Configuration Reference
    :file: Bareos_connection_modes_overview_1.csv
    :widths: 20 35 10 35

.. rubric:: Footnotes
.. [#number] The connection number references this table: :ref:`LegendForFullConnectionOverviewReference`
.. [#identity] From Version 18.2 onwards this is identical to the TLS-PSK Identitiy
.. [#psk] From Version 18.2 onwards this is identical to the TLS-PSK Pre-Shared Key
.. [#user_agent] The name of the default console is predefined and cannot be changed
.. [#cert] Certificate directives are: TlsVerifyPeer, TlsCaCertificateFile, TlsCaCertificateDir, TlsCertificateRevocationList, TlsCertificate, TlsKey, TlsAllowedCn


.. _CompatibilityWithFileDaemonsBefore182Chapter:

Compatibility with |bareosFD|
=============================

|bareosFD| connection handshake probing
---------------------------------------

As from Bareos 18.2 all components by default establish a secure connection with encryption first, followed by the proprietary Bareos protocol. This is accomplished using TLS PSK. Older components of Bareos than version 18.2 start a connection with a cleartext handshake without encryption.

For downward compatibility Bareos Director Daemons and Bareos Storage Daemons are able to connect to Bareos File Daemons older than version 18.2. In this case Director and Storage switch to the old protocol.

There are two connection modes of a File Daemon, active and passive. In contrast to a connection from an active Bareos File Daemon, the protocol version of a passive File Daemon has to be probed by the Director Daemon initially when a job is initiated. This information is stored in the configuration and immediately submitted to the Storage Daemon when the job is started.

The following sequence is used to figure out the right protocol version and to submit this information to the attached Bareos Storage Daemon:

.. uml::
  :caption: Sequence diagram of a Bareos File Daemon connection

  hide footbox

  Actor user
  participant "ConfigurationParser\nclass" as Config << C,#EEEEEE >>
  participant "Some methods in\ndirectordaemon namespace" as Dir << N,#EEEEEE >>
  participant "Client methods in\n directordaemon namespace" as F << N,#EEEEEE >>
  participant "Client methods in\n filedaemon namespace" as FC << N,#EEEEEE >>

  == Config Initialisation ==

  user -> Config: reload config
  activate Config
  Config -> Config: ParseConfigFile()
  Config -> Dir: ConfigReadyCallback()
  activate Dir
  Dir -> Config: ResetAllClientConnectionHandshakeModes
  Dir <-- Config: All handshake modes reset to\nClientConnectionHandshakeMode::kUndefined
  Config <-- Dir: ConfigReadyCallback() done
  deactivate Dir
  user <-- Config: config reloaded

  ... try to connect to a client ...

  == Client Connection to old unknown client ==

  user -> Dir: run some client command
  activate Dir

  Dir -> F: ConnectToFileDaemon()
  activate F
  note right of F: Possible modes:\nkTlsFirst (try TLS immediately),\nkCleartextFirst (old cleartext handshake)
  F ->> FC: Try to connect to Filedaemon with immediate TLS\nconnection mode (kTlsFirst)
  F ->> FC: If immediate TLS fails try cleartext handshake mode\n(kCleartextFirst, this will happen with old clients before 18.2)
  F <- FC: Connection established
  Config <- F: Save successful mode into configuration of client
  Dir <-- F: ConnectToFileDaemon() done
  ... do something with client ...
  FC <--> F: close client connection
  Dir <-- F:
  user <-- Dir : finished some client command
  deactivate F
  deactivate Dir

  ... connect to the same filedaemon again ...

  == Client Connection to a known client ==

  user -> Dir: run some client command
  activate Dir
  Dir -> F: ConnectToFileDaemon()
  activate F
  Config -> F: Load successful mode from configuration of client
  F -> FC: Connect to Filedaemon with saved connection mode from config
  F <- FC: Connection established without waiting or probing
  Dir <-- F: ConnectToFileDaemon() done
  ... do something with client ...
  FC <--> F: close client connection
  Dir <-- F:
  user <-- Dir : finished some client command
  deactivate F
  deactivate Dir

  deactivate Config

|bareosFD| 18.2 with Bareos before 18.2
---------------------------------------

|bareosFD| 18.2 *onwards* can be used on a Bareos system *before* 18.2.

The older |bareosDir| and |bareosSD| connect to |bareosFD| using the cleartext Bareos handshake before they can switch to TLS. If you want transport encryption only TLS with certificates can be used, not PSK as it is possible with Bareos 18.2.

However, it is also possible to disable transport encryption and use cleartext transport using the following configuration changes:

|bareosDir| configuration
^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: ini
  :caption: :file:`/etc/bareos/bareos-dir.d/client/bareos-fd.conf`

  Client {
    ...
    TlsEnable = no
    TlsRequire = no
    ...
  }

.. code-block:: ini
  :caption: :file:`/etc/bareos/bareos-dir.d/storage/bareos-sd.conf`

  Storage {
    ...
    TlsEnable = no
    TlsRequire = no
    ...
  }

|bareosSD| configuration
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: ini
  :caption: :file:`/etc/bareos/bareos-sd.d/storage/bareos-sd.conf`

  Storage {
    ...
    TlsEnable = no
    TlsRequire = no
    ...
  }

|bareosFD| configuration
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: ini
  :caption: :file:`/etc/bareos/bareos-fd.d/client/bareos-fd.conf`

  Client {
    ...
    TlsEnable = no
    TlsRequire = no
    ...
  }

.. code-block:: ini
  :caption: :file:`/etc/bareos/bareos-fd.d/director/bareos-dir.conf`

  Director {
    ...
    TlsEnable = no
    TlsRequire = no
    ...
  }

|bareosFD| before 18.2 with Bareos 18.2
---------------------------------------

|bareosFD| *before* 18.2 can be used on a Bareos system 18.2 *onwards*.

The newer |bareosDir| and |bareosSD| connect to |bareosFD| using the cleartext Bareos handshake before they switch to TLS. If you want transport encryption only TLS with certificates can be used, not PSK as it is possible with Bareos 18.2.

However, it is also possible to disable transport encryption and use cleartext transport using the following configuration changes:

|bareosFD| configuration
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: ini
  :caption: :file:`/etc/bareos/bareos-fd.d/client/bareos-fd.conf`

  Client {
    ...
    TlsEnable = no
    TlsRequire = no
    ...
  }

.. code-block:: ini
  :caption: :file:`/etc/bareos/bareos-fd.d/director/bareos-dir.conf`

  Director {
    ...
    TlsEnable = no
    TlsRequire = no
    ...
  }

.. _PAMConfigurationChapter:

PAM-Configuration
=================

Introduction
------------

Before Bareos Version 18.2 authentication with a Bareos Director is done primarily by a named Console connection. Name and password are set in the regarding Bareos Console or |WebUI| configuration resource. Starting with Bareos Version 18.2 it is also possible to use Pluggable Authentication Modules (PAM) to authenticate a user indenpendently from the Console Resource.

As consequence a dedicated named Console or |WebUI| configuration must be used to establish a connection to a Bareos Director Daemon. This connection has name and password credentials, but only to establish an encrypted connection to the Director. To be able to authenticate users with PAM using this console, each user needs an additional User configuration that holds the regarding name and the Access Control List (ACL) or ACL profile. The ACL will be loaded as soon as the User is authenticated.

The credentials for user authentication comes from the PAM module which has been enabled for the Bareos Director Daemon.

For a simplified technical overview the following diagram shows the connection sequence of a Bareos Console to a Bareos Director using an interactive PAM authentication using the pam_unix.so PAM module.

More technical details can be found in the Bareos Developer Guide: :ref:`PAMDeveloperChapter`.

.. uml::
  :caption: Initiation of a Bareos Console connection using PAM authentication

  hide footbox

  actor user
  participant "B-Console" as console
  participant "Director" as director
  participant "PAM-Linux" as pam

  user -> console: start a named bconsole
  console <-> director: initiate TCP connection
  console <-> director: initiate a secure TLS connection (cert/psk)
  console <-> director: primary challenge/response authentication

  == PAM user authentication ==
  note left of pam: i.e. pam_unix.so\nconfigured in /etc/pam.d/bareos
  autonumber
  director -> pam: initialize pam module
  director <- pam: request username / password via callback
  console <- director: send "login:" / "password:" request encrypted via TLS
  user <- console: prompt "login:" / "Password:"
  user -> console: enter username / password (hidden)
  console -> director: send username / password encrypted via TLS
  director -> pam: give back username / password
  director <- pam: return success of authentication
  console <- director: send welcome message
  user <- console: show welcome message
  director -> pam: shutdown pam module

  autonumber stop
  == PAM user authentication end ==

  ... do something with console ...

  user -> console: quit session ('q'; Ctrl + D)
  console <-> director: Shutdown TLS
  console <-> director: Finish TCP connection

Configuration
-------------
To enable PAM authentication two systems have to be configured. The PAM module in the operating system and the Bareos Console.

PAM Module
^^^^^^^^^^
This is depending on the operating system and on the used pam module. For details read the manuals. The name of the service that has to be registered is "bareos".

Fedora 28 example: :

.. code-block:: ini
  :caption: :file:`/etc/pam.d/bareos`

  # check authorization
  auth       required     pam_unix.so

Bareos Console
^^^^^^^^^^^^^^
For PAM authentication a dedicated named console is used. Set the directive UsePamAuthentication=yes in the regarding Director-Console resource:

.. code-block:: ini
  :caption: :file:`bareos-dir.d/console/pam-console.conf`

  Console {
     Name = "PamConsole"
     Password = "Secretpassword"
     UsePamAuthentication = yes
  }

.. code-block:: ini
  :caption: :file:`bconsole/pam-console.conf`

  Console {
     Name = "PamConsole"
     Password = "Secretpassword"
  }

PAM User
^^^^^^^^
Example of a User resource (Bareos Director Configuration)

.. code-block:: ini
  :caption: :file:`bareos-dir.d/console/pam-user.conf`

  User {
     Name = "Bareos"
     Password = ""
     CommandACL = status, .status
     JobACL = *all*
  }
