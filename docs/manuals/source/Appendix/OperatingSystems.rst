.. _SupportedOSes:

Operating Systems
=================

.. index::
   pair: Operating Systems; Support

The Bareos project provides packages, published at https://download.bareos.org/ and https://download.bareos.com/.

Additionally, the following table gives an overview on the available package versions and the operating systems, respectively:

.. csv-table::
   :header: "Operating Systems", "Version", "Client", "Director", "Storage"

   :strong:`Linux`  :index:`\ <single: Platform; Linux>`
   :index:`Arch Linux <single: Platform; Arch Linux>`, , `X <https://aur.archlinux.org/pkgbase/bareos/>`__,      `X <https://aur.archlinux.org/pkgbase/bareos/>`__, `X <https://aur.archlinux.org/pkgbase/bareos/>`__
   CentOS, current, v12.4, v12.4, v12.4
   Debian, current, v12.4, v12.4, v12.4
   Fedora, current, v12.4, v12.4, v12.4
   :index:`Gentoo <single: Platform; Gentoo>`, , `X <https://packages.gentoo.org/packages/app-backup/bareos>`__,     `X <https://packages.gentoo.org/packages/app-backup/bareos>`__, `X <https://packages.gentoo.org/packages/app-backup/bareos>`__
   openSUSE, current, v12.4, v12.4, v12.4
   (RH)EL, current, v12.4, v12.4, v12.4
   SLES, current, v12.4, v12.4, v12.4
   Ubuntu, current, v12.4, v12.4, v12.4
   :ref:`Univention Corporate Linux <section-UniventionCorporateServer>`, current, v12.4, v12.4, v12.4
   :ref:`Universal Linux Client <section-UniversalLinuxClient>`, , v21.0, v21.0, v21.0

   :strong:`MS Windows`
   :ref:`MS Windows <section-windows>` 32bit, 10/8/7, v12.4, v15.2, v15.2
                                           , 2008/Vista/2003/XP, v12.4–v14.2
   :ref:`MS Windows <section-windows>` 64bit, >= 7, v12.4, v15.2, v15.2
                                            , 2008/Vista, v12.4–v14.2

   :strong:`Mac OS`
   :ref:`Mac OS X/Darwin <section-macosx>`, ,v14.2

   :strong:`BSD`
   :ref:`FreeBSD <section-FreeBSD>` :index:`\ <single: Platform; FreeBSD>`, >= 11, v19.2, v19.2, v19.2
   OpenBSD, , X
   NetBSD,  , X                                                                                                                                                            
   :strong:`Unix`
   :index:`AIX <single: Platform; AIX>`,         >= 4.3, com-13.2, \*, \*
   :index:`HP-UX <single: Platform; HP-UX>`,           , com-13.2
   :ref:`Solaris <section-Solaris>` (i386/Sparc) :index:`\ <single: Platform; Solaris>`, >= 11 , com-12.4, com-12.4, com-12.4


============ =============================================================================================================================
**vVV.V**    starting with Bareos version VV.V, this platform is official supported by the Bareos.org project
**com-VV.V** starting with Bareos version VV.V, this platform is supported. However, pre-build packages are only available from Bareos.com
**X**        known to work
**\***       has been reported to work by the community
============ =============================================================================================================================
