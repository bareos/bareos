.. _appendix-chapter-faq:

FAQ
===

.. _bareos-1825-updatefaq:

Bareos 18.2.5 FAQ
--------------------

What is the important feature introduced in Bareos 18.2?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

#. A new network protocol was introduced where TLS is immediately used.

  * When no certificates are configured, the network connection will still be
    encrypted using TLS-PSK.
  * When certificates are configured, Bareos will configure both TLS-PSK and
    TLS with certificates at the same time, so that the TLS protocol will
    choose which one to use.

How to update from Bareos 17.2?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To update from Bareos 17.2, as always all core components need to be updated as
they need to be of the same Bareos version (|bconsole|, |bareosDir|, |bareosSd|).

How can I see what encryption is being used?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Whenever a connection is established, the used cipher is logged and will be
shown in the job log and messages output:


.. code-block:: sh
   :caption: console output

   Connecting to Director localhost:9101
    Encryption: ECDHE-PSK-CHACHA20-POLY1305



.. code-block:: sh
   :caption: job log

   [...] JobId 1: Connected Storage daemon at bareos:9103, encryption: ECDHE-PSK-CHACHA20-POLY1305

What should I do when I get "TLS negotiation failed"?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Bareos components use TLS-PSK as default. When the TLS negotiation fails then most likely identity
or password do not match. Doublecheck the component name and password in the respective configuration
to match each other.

How does the compatibility with old clients work?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The Bareos Director always connects to clients using the new immediate TLS
protocol.  If that fails, it will fall back to the old protocol and try to
connect again.

When the connection is successful, the director will store which protocol needs
to be used with the client and use this protocol the next time this client will
be connected.  Whenever the configuration is reloaded, the protocol information
will be cleared and the probing will be done again when the next connection to
this client is done.

.. code-block:: sh
   :caption: probing the client protocol

   [...] JobId 1: Probing... (result will be saved until config reload)
   [...] JobId 1: Connected Client: bareos-fd at localhost:9102, encryption: ECDHE-PSK-CHACHA20-POLY1305
   [...] JobId 1:    Handshake: Immediate TLS



Does Bareos support TLS 1.3?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^
Yes. If Bareos is compiled with OpenSSL 1.1.1, it will automatically use TLS
1.3 where possible.


Are old Bareos clients still working?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Bareos clients < 18.2 will still work, and the old protocol will be used.
This was mostly tested with Bareos 17.2 clients.



Can I use a new Bareos 18.2 client with my Bareos 17.2 system?
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Yes, it is possible to use a Bareos 18.2 client, but some changes need to be done
in the configuration.

It is possible to use the Bareos 18.2 client with a Bareos 17.2 Server. However,
the new immediate TLS Protocol and TLS-PSK are not usable, as the server
components do not support it. This also means that it is **not** possible to
use TLS with certificates in this setup. The communication will be unencrypted
using the old protocol.

As in Bareos 18.2, the default value of **TLS Enable** was changed to **yes** to
automatically use TLS-PSK, and the meaning of **TLS Require** also was altered
so that it enforces the new protocol, these settings need to be changed.

In order to make Bareos 18.2 clients work with a Bareos 17.2 server, the following
changes need to be done:

* **On all Bareos 18.2 clients**, the directive **TLS Enable** in the file
  :file:`/etc/bareos/bareos-fd.d/director/bareos-dir.conf` needs to be set to **no**.
  If the directive **TLS Require** is set, it also needs
  to be set to **no** in the same file.
  This is enough for standard clients which do not have any special setup for the
  connections, and also for clients that are configured to use **client initiated
  connections**.

* For **clients that use the passive mode**, also the clients' setting in the
  Bareos 17.2 director in file :file:`/etc/bareos/bareos-dir.d/client/passive-fd.conf` needs
  to to be altered so that both directives **TLS Enable**
  and **TLS Require** are set to **no**.
