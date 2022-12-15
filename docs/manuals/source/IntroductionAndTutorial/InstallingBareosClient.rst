.. _section-InstallBareosClient:

Installing a Bareos Client
==========================

When installing a Bareos client,
you should choose the same release as on the Bareos server.

The package **bareos-client** is a meta-package.
Installing it will also install
the **bareos-filedaemon**, **bareos-bconsole** and
suggests the installation of the **bareos-traymonitor**.

If you prefer to install just the backup client,
it is sufficient to only install the package **bareos-filedaemon**.

After installing the client,
please read the chapter :ref:`section-AddAClient`
about how to configurre the client.

Installing a Bareos Client on Linux Distributions
-------------------------------------------------

The installation of a Bareos client on a Linux (and FreeBSD) system
is identical as described for Bareos server installations.

Just install the package **bareos-filedaemon** or **bareos-client** (**bareos-filedaemon**, **bareos-bconsole** and **bareos-traymonitor**) instead of the meta-package **bareos**.

If there is no specific Bareos repository for your Linux distribution,
consider using the :ref:`section-UniversalLinuxClient` instead.

* :ref:`section-InstallBareosPackagesRedhat`
* :ref:`section-InstallBareosPackagesSuse`
* :ref:`section-InstallBareosPackagesDebian`
* :ref:`section-UniversalLinuxClient`


.. _section-UniversalLinuxClient:

Installing the Bareos Universal Linux Client
--------------------------------------------

The Bareos project provides packages
for the current releases of all major Linux distributions.
In order to support even more platforms
Bareos :sinceVersion:`21.0.0: Universal Linux Client (ULC)`
provides the so called **Universal Linux Client (ULC)**.

The Universal Linux Client is a |fd|,
built in a way to have minimal dependencies to other libraries.

.. note::

   The **Universal Linux Client** depends on the **OpenSSL** library
   of the host in order to utilize security updates for this library.

It incorporates all functionality for normal backup and restore operations,
however it has only limited plugin support.

Currently it is provided as a Debian package.
However, it is planed to provide it also in other formats.

The ULC have extra repositories, their names starting with **ULC_**
(e.g. **ULC_deb_OpenSSL_1.1**)
at https://download.bareos.com/bareos/release/ and https://download.bareos.org/current/.
There will be different repositories depending on packaging standard
and remaining dependencies.
These repositories contain the **bareos-universal-client** package
and sometimes their corresponding debug package.
You can either add the repository to your system
or only download and install the package file.

One of ULC's goals is to support new platforms
for which native packages are not yet available.
As soon as native packages are available,
their repository can be added
and on an update the ULC package
will be seamlessly replaced by the normal |fd| package.
No change to the Bareos configuration is required.

.. warning::

   While ULC packages are designed to run on as many Linux platforms as possible,
   they should only be used
   if this platform is not directly supported by the Bareos project.
   When available, native packages should be preferred.

Feature overview:

  * Single package installation
  * Repository based installation
  * Minimal dependencies to system libraries (except OpenSSL)
  * Uses host OpenSSL library
  * Replaceable by the normal |fd|. No configuration change required.


Installing a Bareos Client on FreeBSD
-------------------------------------

Installing the Bareos client is very similar to :ref:`section-InstallBareosPackagesFreebsd`.

Get the :file:`add_bareos_repositories.sh`
matching the requested Bareos release
and the distribution of the target system
from https://download.bareos.org/ or https://download.bareos.com/
and execute it on the target system:

.. code-block:: shell-session
   :caption: Shell example script for Bareos installation on FreeBSD

   root@host:~# sh ./add_bareos_repositories.sh
   root@host:~# pkg install --yes bareos.com-filedaemon

   ## enable services
   root@host:~# sysrc bareosfd_enable=YES

   ## start services
   root@host:~# service bareos-fd start


.. _section-Solaris:

Installing a Bareos Client on Oracle Solaris
--------------------------------------------

.. index::
   single: Platform; Solaris

The |fd| is available as **IPS** (*Image Packaging System*) packages for **Oracle Solaris 11.4**.

First, download the Solaris package to the local disk and add the package as publisher
**bareos**:

.. code-block:: shell-session
   :caption: Add bareos publisher

   root@solaris114:~# pkg set-publisher -p bareos-fd-<version>.p5p  bareos
   pkg set-publisher:
     Added publisher(s): bareos


Then, install the filedaemon with **pkg install**:


.. code-block:: shell-session
   :caption: Install solaris package

   root@solaris114:~# pkg install bareos-fd
             Packages to install:  1
              Services to change:  1
         Create boot environment: No
   Create backup boot environment: No

   DOWNLOAD                                PKGS         FILES    XFER (MB)   SPEED
   Completed                                1/1         44/44      1.0/1.0  4.8M/s

   PHASE                                          ITEMS
   Installing new actions                         94/94
   Updating package state database                 Done
   Updating package cache                           0/0
   Updating image state                            Done
   Creating fast lookup database                working |


After installation, check the bareos-fd service status with **svcs bareos-fd**:

.. code-block:: shell-session
   :caption: Check solaris service

   root@solaris114:~# svcs bareos-fd
   STATE          STIME      FMRI
   online         16:16:14   svc:/bareos-fd:default


Finish the installation by adapting the configuration in :file:`/usr/local/etc/bareos` and restart the
service with **svcadm restart bareos-fd**:

.. code-block:: shell-session
   :caption: Restart solaris service

   root@solaris114:~# svcadm restart bareos-fd

The |fd| service on solaris is now ready for use.


.. _section-macosx:

Installing a Bareos Client on Mac OS X
--------------------------------------

.. index::
   single: Platform; Mac; OS X

Bareos for MacOS X is available either

-  via the `Homebrew project <https://brew.sh/>`_ (https://formulae.brew.sh/formula/bareos-client) or

-  as pkg file from https://download.bareos.org/ or https://download.bareos.com/.

However, you have to choose upfront, which client you want to use. Otherwise conflicts do occur.

Both packages contain the |fd| and :command:`bconsole`.

Installing the Bareos Client as PKG
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. index::
   single: Installation; MacOS

The Bareos installer package for Mac OS X contains the |fd| for Mac OS X 10.5 or later.

On your local Mac, you must be an admin user. The main user is an admin user.

Download the :file:`bareos-*.pkg` installer package from https://download.bareos.org/ or  or https://download.bareos.com/.

Find the .pkg you just downloaded. Install the .pkg by holding the CTRL key, left-clicking the installer and choosing "open".

Follow the directions given to you and finish the installation.

Configuration
~~~~~~~~~~~~~

To make use of your |fd| on your system, it is required to configure the |dir| and the local |fd|.

Configure the server-side by follow the instructions at :ref:`section-AddAClient`.

After configuring the server-side you can either transfer the necessary configuration file using following command or configure the client locally.

Option 1: Copy the director resource from the Bareos Director to the Client
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Assuming your client has the DNS entry :strong:`client2.example.com` and has been added to |dir| as :config:option:`dir/client = client2-fd`\ :

.. code-block:: shell-session

   scp /etc/bareos/bareos-dir-export/client/client2-fd/bareos-fd.d/director/bareos-dir.conf root@client2.example.com:/usr/local/etc/bareos/bareos-fd.d/director/

This differs in so far, as on Linux the configuration files are located under :file:`/etc/bareos/`, while on MacOS they are located at :file:`/usr/local/etc/bareos/`.

Option 2: Edit the director resource on the Client
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Alternatively, you can edit the file :file:`/usr/local/etc/bareos/bareos-fd.d/director/bareos-dir.conf`.

This can be done by right-clicking the finder icon in your task bar, select "Go to folder ..." and paste :file:`/usr/local/etc/bareos/bareos-fd.d/director/`.

Select the :file:`bareos-dir.conf` file and open it.

Alternatively you can also call following command on the command console:

.. code-block:: shell-session

   open -t /usr/local/etc/bareos/bareos-fd.d/director/bareos-dir.conf

The file should look similar to this:

.. code-block:: bareosconfig
   :caption: bareos-fd.d/director/bareos-dir.conf

   Director {
     Name = bareos-dir
     Password = "SOME_RANDOM_PASSWORD"
     Description = "Allow the configured Director to access this file daemon."
   }

Set this client-side password to the same value as given on the server-side.



.. warning::

   The configuration file contains passwords and therefore must not be accessible for any users except admin users.

Restart bareos-fd after changing the configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The bareos-fd must be restarted to reread its configuration:

.. code-block:: shell-session
   :caption: Restart the |fd|

   sudo launchctl stop  org.bareos.bareos-fd
   sudo launchctl start org.bareos.bareos-fd

Verify that the Bareos File Daemon is working
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Open the :command:`bconsole` on your |dir| and check the status of the client with

.. code-block:: bareosconfig

   *<input>status client=client2-fd</input>

In case, the client does not react, following command are useful the check the status:

.. code-block:: shell-session
   :caption: Verify the status of |fd|

   # check if bareos-fd is started by system:
   sudo launchctl list org.bareos.bareos-fd

   # get process id (PID) of bareos-fd
   pgrep bareos-fd

   # show files opened by bareos-fd
   sudo lsof -p `pgrep bareos-fd`

   # check what process is listening on the |fd| port
   sudo lsof -n -iTCP:9102 | grep LISTEN

You can also manually start bareos-fd in debug mode by:

.. code-block:: shell-session
   :caption: Start |fd| in debug mode

   sudo /usr/local/sbin/bareos-fd -f -d 100


Installing a Bareos Client on Windows
-------------------------------------

See :ref:`Windows:Installation`.
