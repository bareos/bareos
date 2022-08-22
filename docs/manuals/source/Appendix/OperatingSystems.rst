.. _SupportedOSes:

Operating Systems
=================

.. index::
   pair: Operating Systems; Support

The Bareos project provides packages that have been released on http://download.bareos.org/bareos/release/.

Additionally, the following table gives an overview on the available package versions and the operating systems, respectively:

.. csv-table::
   :header: "Operating Systems", "Version", "Client", "Director" , "Storage"

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
   Universal Linux Client,  openSSL 1.1, v21.0, v21.0, v21.0


   :ref:`Univention Corporate Linux <section-UniventionCorporateServer>`, App Center, v12.4, v12.4, v12.4

   :strong:`MS Windows`
   :ref:`MS Windows <section-windows>` 32bit, 10/8/7, v12.4, v15.2, v15.2
                                           , 2008/8/2003/XP, v12.4–v14.2
   :ref:`MS Windows <section-windows>` 64bit, 11/2022/2019/2016/10, v12.4, v15.2, v15.2
                                            , 8/2012/7/2008/Vista, v12.4–v14.2

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



