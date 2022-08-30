.. _SupportedOSes:

Operating Systems
=================

.. index::
   pair: Operating Systems; Support

The Bareos project provides packages that have been released on http://download.bareos.org/bareos/release/.

Additionally, the following table gives an overview on the available package versions and the operating systems, respectively:

.. csv-table::
   :header: "Operating Systems", "Version", "Client Daemon", "Director Daemon" , "Storage Daemon"

   :strong:`Linux`  :index:`\ <single: Platform; Linux>`\
   Arch Linux :index:`\ <single: Platform; Arch Linux>`\ , `X <https://aur.archlinux.org/pkgbase/bareos/>`__,      `X <https://aur.archlinux.org/pkgbase/bareos/>`__, `X <https://aur.archlinux.org/pkgbase/bareos/>`__
   CentOS, current, v12.4, v12.4, v12.4
   Debian, current, v12.4, v12.4, v12.4
   Fedora, current, v12.4, v12.4, v12.4
   Gentoo :index:`\ <single: Platform; Gentoo>`\ , `X <https://packages.gentoo.org/packages/app-backup/bareos>`__,     `X <https://packages.gentoo.org/packages/app-backup/bareos>`__, `X <https://packages.gentoo.org/packages/app-backup/bareos>`__
   openSUSE, current, v12.4, v12.4, v12.4
   RHEL,     current, v12.4, v12.4, v12.4
   SLES,     current, v12.4, v12.4, v12.4
   Ubuntu,   current, v12.4, v12.4, v12.4
   :ref:`Univention Corporate Linux <section-UniventionCorporateServer>`, current, v12.4, v12.4, v12.4

   :strong:`MS Windows`
   :ref:`MS Windows <section-windows>` 32bit, 10/8/7, v12.4, v15.2, v15.2
                                           , 2008/Vista/2003/XP, v12.4–v14.2
   :ref:`MS Windows <section-windows>` 64bit, 10/8/2012/7, v12.4, v15.2, v15.2
                                            , 2008/Vista, v12.4–v14.2

   :strong:`Mac OS`
   :ref:`Mac OS X/Darwin <section-macosx>`, v14.2

   :strong:`BSD`
   :ref:`FreeBSD <section-FreeBSD>` :index:`\ <single: Platform; FreeBSD>`\ , >= 11, v19.2, v19.2, v19.2
   OpenBSD, , X
   NetBSD,  , X                                                                                                                                                            
   :strong:`Unix`
   AIX :index:`\ <single: Platform; AIX>`\ ,         >= 4.3, com-13.2, \*, \*
   HP-UX :index:`\ <single: Platform; HP-UX>`\ ,           , com-13.2
   :ref:`Solaris <section-Solaris>` (i386/Sparc) :index:`\ <single: Platform; Solaris>`\ , >= 11 , com-12.4, com-12.4, com-12.4


============ =============================================================================================================================
**vVV.V**    starting with Bareos version VV.V, this platform is official supported by the Bareos.org project
**com-VV.V** starting with Bareos version VV.V, this platform is supported. However, pre-build packages are only available from Bareos.com
**X**        known to work
**\***       has been reported to work by the community
============ =============================================================================================================================

Linux
-----

.. _section-UniversalLinuxClient:

Universal Linux Client
~~~~~~~~~~~~~~~~~~~~~~

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
at https://download.bareos.org/bareos/release/.
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


.. _section-UniventionCorporateServer:

Univention Corporate Server
~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. index::
   single: Platform; Univention Corporate Server

The `Univention Corporate Server (UCS) <https://www.univention.de/>`_ is an enterprise Linux distribution based on Debian.

Earlier releases (Bareos < 21, UCS < 5.0) offered extended integration into UCS and provided its software also via the Univention App Center.
With version 5.0 of the UCS App Center the method of integration changed requiring commercially not reasonable efforts for deep integration.

Bareos continues to support UCS with the same functionality as the other Linux distributions.

During the build process, Bareos Debian 10 packages are automatically tested on an UCS 5.0 system. Only packages that passes this acceptance test, will get released by the Bareos project.

.. note::

   While Bareos offers a software repository for UCS >= 5,
   this repository is identical with the corresponding Debian repository.
   The included APT sources file will also refer to the Debian repository.


.. _section-DebianOrg:

Debian.org / Ubuntu Universe
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Platform; Debian; Debian.org>`
:index:`\ <single: Platform; Debian; 8>`
:index:`\ <single: Platform; Ubuntu; Universe>`
:index:`\ <single: Platform; Ubuntu; Universe; 15.04>`

The distributions of Debian >= 8 include a version of Bareos. Ubuntu Universe >= 15.04 does also include these packages.

In the further text, these version will be named **Bareos (Debian.org)** (also for the Ubuntu Universe version, as this is based on the Debian version).


.. _section-DebianOrgLimitations:

Limitations of the Debian.org/Ubuntu Universe version of Bareos
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Debian.org does not include the libfastlz compression library and therefore the Bareos (Debian.org) packages do not offer the fileset options :strong:`compression=LZFAST`, :strong:`compression=LZ4` and :strong:`compression=LZ4HC`.

-  Debian.org does not include the **bareos-webui** package.


.. _section-macosx:

Mac OS X
--------

:index:`\ <single: Platform; Mac; OS X>`\

Bareos for MacOS X is available either

-  via the `Homebrew project <https://brew.sh/>`_ (https://formulae.brew.sh/formula/bareos-client) or

-  as pkg file from https://download.bareos.org/bareos/release/.

However, you have to choose upfront, which client you want to use. Otherwise conflicts do occur.

Both packages contain the |fd| and :command:`bconsole`.

Installing the Bareos Client as PKG
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`\ <single: Installation; MacOS>`\

The Bareos installer package for Mac OS X contains the |fd| for Mac OS X 10.5 or later.

On your local Mac, you must be an admin user. The main user is an admin user.

Download the :file:`bareos-*.pkg` installer package from https://download.bareos.org/bareos/release/.

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
