.. _bareos-1824rc2-releasenotes:

Bareos 18.2.4rc2 Release Notes
============================

General Release Information
---------------------------

.. list-table:: Bareos 18.2.4rc2 Release Information
   :header-rows: 0
   :widths: auto

   * - **Release Date**
     - December 2018
   * - **Database Version**
     -  2171
   * - **URL**
     - http://download.bareos.org/bareos/release/18.2/
..
   * - **Release Ticket**
     -
..
   * - **LOC**
     - 123456+ 12345-

.. csv-table:: binary package availablility
   :header: "Distribution", "Architecture"
   :widths: auto

   CentOS_6, "x86_64"
   CentOS_7, "x86_64"
   Debian_8.0, "i586,x86_64"
   Debian_9.0, "i586,x86_64"
   Fedora_28, "x86_64"
   Fedora_29, "x86_64"
   openSUSE_Leap_15.0, "x86_64"
   RHEL_6, "x86_64"
   RHEL_7, "x86_64"
   SLE_12_SP3, "x86_64"
   SLE_12_SP4, "x86_64"
   SLE_15, "x86_64"
   Univention_4.3, "x86_64"
   win_cross, "x86_64"
   xUbuntu_14.04, "i586,x86_64"
   xUbuntu_16.04, "i586,x86_64"
   xUbuntu_18.04, "x86_64"

New Features
------------

* New network Protocol using immediately TLS

  * TLS is immediately used on all network connections
  * Support for TLS-PSK in all daemons
  * Automatic encryption of all network traffic with TLS-PSK
  * Full Compatibility with old |bareosFd|

    * Old |bareosFd| speaking the old protocol are automatically detected
      and daemons switch to the old protocol

  * Easy update without configuration changes

    * Existing Bareos installations can be upgraded without configuration changes

* PAM Support

  * Detailed information follows
  * Introduction of new "User" Resource
  * The |bareosDir| supports PAM for user authentication
  * The Bareos WebUI supports PAM user authentication against the |bareosDir|

Changed Features
----------------
* Bandwidth limiting now also works in TLS encrypted connections. Before, bandwidth limiting
  was ignored when the connections were TLS encrypted

* droplet (S3): multiple enhancements

* |bconsole|: added "whoami" command to show currently associated user

* xattr and acl support now are enabled by default

  * both features were disabled by default and needed to be enabled in the fileset options
  * now both are enabled by default and can be disabled in the fileset options
  * new |bareosFd| logs the current status of both options in job log

Backward compatibility
---------------------
* |bareosDir| >= 18.2 can work with all |bareosFd| versions. However, all other components need to be updated to Bareos version >= 18.2
* To maintain |bareosWebui| access to the |bareosDir|, it depends on the current configuration. 1. TLS certificates: Nothing to do. 2. No TLS configured: Set TlsEnable=false in the respective console config of the |bareosWebui| in the |bareosDir|
..  * |bconsole| < 18.2 can be used with minor drawbacks (no PAM authentication, no TLS-PSK)

Deprecated and Removed Features
-------------------------------
* removed Bareos conio option, as the standard library readline is used instead
* GnutTLS is not supported anymore, OpenSSL is now required


Bugs Fixed
----------
* #805 (bugs.bareos.org): can't restore vmware-plugin assisted backups via |bareosWebui|
* Windows Installer: Fixed infinite install dialog for VC 2012 checks on x86 windows
* Fixed memory leaks in the |bareosDir| when using bconsole or |bareosWebui|
* Fixed a bug in the debug message handler on the |bareosDir| when debuglevel is >= 900
* Improved shutdown of |bareosDir|


Internal Project Changes
------------------------
* reorganized the whole git repository and merged sub repositories into main repository
* changed the build system from autoconf/automake to cmake
* switched from cmocka to google test framework for unit tests
* introduced namespaces to avoid name clashes when parts of different daemons are tested in one test
* switched to use c++11 standard, start to refactor using standard library instead of legacy features
* use google c++ style guide

  * refactored variable names

* refactored configuration parser
* TLS implementation has now a base class interface instead of compile time switched behaviour
* library cleanup and reorganization

  * library does not use main program variables anymore
  * removed libbareoscfg
  * enhanced windows cross building

* renamed c++ files to use .cc file extension
* cleanup of header files

  * removed "protos.h"
  * introduced individual header file for each c++ file
  * each header file has own google c++ standard header guard
  * explicitly declare functions override where applicable


* |bareosTraymonitor|: Allows compiling using Qt4 or Qt5
* switch the documentation from LaTeX to Sphinx (work in progress)
* |bareosWebui|: Enhances Selenium tests to be used on https://saucelabs.com/u/Bareossaucelabs
* clang: massively reduced number of warnings
* FreeBSD: added start scripts, fixed buggy cmake detection of ACL support
* regression tests

  * automatically build |bareosTraymonitor|
  * preconfigure |bareosWebui| to run in php's own webserver for easy testing




.. _bareos-1824rc2-updatefaq:

Bareos 18.2.4rc2 FAQ
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
  Bareos 17.2 director in file :file:`/etc/Bareos/Bareos-dir.d/client/passive-fd.conf` needs
  to to be altered so that both directives **TLS Enable**
  and **TLS Require** are set to **no**.
