.. _SupportedOSes:

Operating Systems
=================

:index:`[TAG=Systems->Supported Operating Systems] <pair: Systems; Supported Operating Systems>` :index:`[TAG=Support->Operating Systems] <pair: Support; Operating Systems>`

The Bareos project provides and supports packages that have been released at http://download.bareos.org/bareos/release/

However, the following tabular gives an overview, what components are expected on which platforms to run:

# Tabular in LaTex format (original)

::

   \begin{tabular}[h]{|l|c|c|c|c|}
     \hline
     :strong:`Operating Systems` & :strong:`Version` & :strong:`Client Daemon` & :strong:`Director Daemon` & :strong:`Storage Daemon` \\
     \hline
     \hline
     :strong:`:strong:`Linux``  :index:`[TAG=Platform->Linux] <pair: Platform; Linux>` \\
     \hline
     Arch Linux
     :index:`[TAG=Platform->Arch Linux] <pair: Platform; Arch Linux>`
                     & ~ & `X <https://aur.archlinux.org/pkgbase/bareos/>`_ & `X <https://aur.archlinux.org/pkgbase/bareos/>`_ & `X <https://aur.archlinux.org/pkgbase/bareos/>`_ \\
     \hline
     CentOS          & current & v12.4 & v12.4 & v12.4 \\
     \hline
     Debian          & current & v12.4 & v12.4 & v12.4 \\
     \hline
     Fedora          & current & v12.4 & v12.4 & v12.4 \\
     \hline
     Gentoo
     :index:`[TAG=Platform->Gentoo] <pair: Platform; Gentoo>`
                       & ~ & `X <https://packages.gentoo.org/package/app-backup/bareos>`_ & `X <https://packages.gentoo.org/package/app-backup/bareos>`_ & `X <https://packages.gentoo.org/package/app-backup/bareos>`_ \\
     \hline
     openSUSE        & current & v12.4 & v12.4 & v12.4 \\
     \hline
     RHEL            & current & v12.4 & v12.4 & v12.4 \\
     \hline
     SLES            & current & v12.4 & v12.4 & v12.4 \\
     \hline
     Ubuntu          & current & v12.4 & v12.4 & v12.4 \\
     \hline
     :ref:`Univention Corporate Linux <section-UniventionCorporateServer>` & App Center & v12.4 & v12.4 & v12.4 \\
     \hline
     :strong:`:strong:`MS Windows``  \\
     \hline
     :ref:`MS Windows <section-windows>` 32bit & 10/8/7       & v12.4 & v15.2 & v15.2 \\
     ~                & 2008/Vista   &  &  &  \\
     ~                & 2003/XP      & v12.4--v14.2 &         &  \\
     \hline
     :ref:`MS Windows <section-windows>` 64bit & 10/8/2012/7  & v12.4 & v15.2 & v15.2 \\
     ~                & 2008/Vista   &  &  &  \\
     \hline
     :strong:`:strong:`Mac OS``  \\
     \hline
     :ref:`Mac OS X/Darwin <section-macosx>`   & ~ & v14.2 &  &  \\
     \hline
     :strong:`:strong:`BSD``  \\
     \hline
     FreeBSD
     :index:`[TAG=Platform->FreeBSD] <pair: Platform; FreeBSD>`
                       & >= 5.0 & `X <http://www.freshports.org/sysutils/bareos-client/>`_ & `X <http://www.freshports.org/sysutils/bareos-server/>`_ & `X <http://www.freshports.org/sysutils/bareos-server/>`_  \\
     \hline
     OpenBSD          & ~ & X &  & ~ \\
     \hline
     NetBSD           & ~ & X &  & ~ \\
     \hline
     :strong:`:strong:`Unix``  \\
     %\hline
     %BSDI             & ~ & * & ~ & ~ \\
     \hline
     AIX
     :index:`[TAG=Platform->AIX] <pair: Platform; AIX>`
                      & >= 4.3 & com-13.2 & * & * \\
     \hline
     HP-UX
     :index:`[TAG=Platform->HP-UX] <pair: Platform; HP-UX>`
                      & ~ & com-13.2 & ~ & ~ \\
     \hline
     Irix             & ~ & * & ~ & ~ \\
     %\hline
     %OpenSolaris      & ~ & X & X & X \\
     \hline
     Solaris
     :index:`[TAG=Platform->Solaris] <pair: Platform; Solaris>`
                      & >= 8 & com-12.4 & com-12.4 & com-12.4 \\
     \hline
     True64           & ~ & * & ~ & ~ \\
     \hline
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

============================================================================================ =========== ============================================================================= ============================================================================= =============================================================================
**Operating Systems**                                                                        **Version** **Client Daemon**                                                             **Director Daemon**                                                           **Storage Daemon**
============================================================================================ =========== ============================================================================= ============================================================================= =============================================================================
:strong:`:strong:`Linux``  :index:`[TAG=Platform->Linux] <pair: Platform; Linux>`                                                                                                                                                                        
Arch Linux :index:`[TAG=Platform->Arch Linux] <pair: Platform; Arch Linux>`                                             `X <https://aur.archlinux.org/pkgbase/bareos/>`_             `X <https://aur.archlinux.org/pkgbase/bareos/>`_             `X <https://aur.archlinux.org/pkgbase/bareos/>`_
CentOS                                                                                       current     v12.4                                                                         v12.4                                                                         v12.4
Debian                                                                                       current     v12.4                                                                         v12.4                                                                         v12.4
Fedora                                                                                       current     v12.4                                                                         v12.4                                                                         v12.4
Gentoo :index:`[TAG=Platform->Gentoo] <pair: Platform; Gentoo>`                                                     `X <https://packages.gentoo.org/package/app-backup/bareos>`_ `X <https://packages.gentoo.org/package/app-backup/bareos>`_ `X <https://packages.gentoo.org/package/app-backup/bareos>`_
openSUSE                                                                                     current     v12.4                                                                         v12.4                                                                         v12.4
RHEL                                                                                         current     v12.4                                                                         v12.4                                                                         v12.4
SLES                                                                                         current     v12.4                                                                         v12.4                                                                         v12.4
Ubuntu                                                                                       current     v12.4                                                                         v12.4                                                                         v12.4
:ref:`Univention Corporate Linux <section-UniventionCorporateServer>`           App Center  v12.4                                                                         v12.4                                                                         v12.4
:strong:`:strong:`MS Windows``                                                                                                                                                                                                                 
:ref:`MS Windows <section-windows>` 32bit                                       10/8/7      v12.4                                                                         v15.2                                                                         v15.2
                                                                                             2008/Vista                                                                                                                                                             
                                                                                             2003/XP     v12.4–v14.2                                                                                                                                                
:ref:`MS Windows <section-windows>` 64bit                                       10/8/2012/7 v12.4                                                                         v15.2                                                                         v15.2
                                                                                             2008/Vista                                                                                                                                                             
:strong:`:strong:`Mac OS``                                                                                                                                                                                                                     
:ref:`Mac OS X/Darwin <section-macosx>`                                                     v14.2                                                                                                                                                      
:strong:`:strong:`BSD``                                                                                                                                                                                                                        
FreeBSD :index:`[TAG=Platform->FreeBSD] <pair: Platform; FreeBSD>`                                       >= 5.0      `X <http://www.freshports.org/sysutils/bareos-client/>`_     `X <http://www.freshports.org/sysutils/bareos-server/>`_     `X <http://www.freshports.org/sysutils/bareos-server/>`_
OpenBSD                                                                                                  X                                                                                                                                                            
NetBSD                                                                                                   X                                                                                                                                                            
:strong:`:strong:`Unix``                                                                                                                                                                                                                       
AIX :index:`[TAG=Platform->AIX] <pair: Platform; AIX>`                                               >= 4.3      com-13.2                                                                      \*                                                                            \*
HP-UX :index:`[TAG=Platform->HP-UX] <pair: Platform; HP-UX>`                                                       com-13.2                                                                                                                                                     
Irix                                                                                                     \*                                                                                                                                                           
Solaris :index:`[TAG=Platform->Solaris] <pair: Platform; Solaris>`                                       >= 8        com-12.4                                                                      com-12.4                                                                      com-12.4
True64                                                                                                   \*                                                                                                                                                           
============================================================================================ =========== ============================================================================= ============================================================================= =============================================================================

# Tabular in LaTex format (original)

::

   \begin{tabular}[h]{p{0,15\textwidth} p{0,8\textwidth}}
   :strong:`vVV.V`   & starting with Bareos version VV.V, this platform is official supported by the Bareos.org project \\
   :strong:`com-VV.V`   & starting with Bareos version VV.V, this platform is supported. However, pre-build packages are only available from  Bareos.com\\
   :strong:`nightly` & provided by Bareos nightly build. Bug reports are welcome, however it is not official supported \\
   :strong:`X`       & known to work \\
   :strong:`*` & has been reported to work by the community\\
   \end{tabular}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

============ =============================================================================================================================
**vVV.V**    starting with Bareos version VV.V, this platform is official supported by the Bareos.org project
**com-VV.V** starting with Bareos version VV.V, this platform is supported. However, pre-build packages are only available from Bareos.com
**nightly**  provided by Bareos nightly build. Bug reports are welcome, however it is not official supported
**X**        known to work
**\***       has been reported to work by the community
============ =============================================================================================================================

Linux
-----

.. _section-packages:

Packages for the different Linux platforms
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following tables summarize what packages are available for the different Linux platforms.

This information is generated based on http://download.bareos.com/bareos/release/. In most cases this is identical to the packages provided by http://download.bareos.org/bareos/release/. Only if a package have been added later in a maintenance release, these information may differ.

Distributions that are no longer relevant are left out. However, you might still find the packages on our download servers.

Bareos tries to provide all packages for all current platforms. For extra packages, it depends if the distribution contains the required dependencies.

For general information about the packages, see :ref:`section-BareosPackages`.

Packages names not containing the word **bareos** are required packages where we decided to include them ourselves.



:index:`[TAG=Platform->CentOS->5] <triple: Platform; CentOS; 5>`:index:`[TAG=Platform->CentOS->6] <triple: Platform; CentOS; 6>`:index:`[TAG=Platform->CentOS->7] <triple: Platform; CentOS; 7>`:index:`[TAG=Platform->RHEL->4] <triple: Platform; RHEL; 4>`:index:`[TAG=Platform->RHEL->5] <triple: Platform; RHEL; 5>`:index:`[TAG=Platform->RHEL->6] <triple: Platform; RHEL; 6>`:index:`[TAG=Platform->RHEL->7] <triple: Platform; RHEL; 7>`

# Tabular in LaTex format (original)

::

   \begin{longtable}{ l | c | c | c | c | c | c | c }
   \hline 
    & 
   :strong:`CentOS`  &
   :strong:`RHEL` 
   \\ 
    & 5 & 6 & 7 & 4 & 5 & 6 & 7\\ 
   \hline 
   \hline 
   **bareos** & 12.4-16.2 & 12.4-17.2 & 12.4-17.2 & 13.2-14.2 & 12.4-17.2 & 12.4-17.2 & 12.4-17.2 \\ 
   **bareos-bat** &   & 12.4-16.2 & 12.4-16.2 &   &   & 12.4-16.2 & 12.4-16.2 \\ 
   **bareos-bconsole** & 12.4-16.2 & 12.4-17.2 & 12.4-17.2 & 13.2-14.2 & 12.4-17.2 & 12.4-17.2 & 12.4-17.2 \\ 
   **bareos-client** & 12.4-16.2 & 12.4-17.2 & 12.4-17.2 & 13.2-14.2 & 12.4-17.2 & 12.4-17.2 & 12.4-17.2 \\ 
   **bareos-common** & 12.4-16.2 & 12.4-17.2 & 12.4-17.2 & 13.2-14.2 & 12.4-17.2 & 12.4-17.2 & 12.4-17.2 \\ 
   **bareos-database-common** & 12.4-16.2 & 12.4-17.2 & 12.4-17.2 &   & 12.4-17.2 & 12.4-17.2 & 12.4-17.2 \\ 
   **bareos-database-mysql** & 12.4-16.2 & 12.4-17.2 & 12.4-17.2 &   & 12.4-17.2 & 12.4-17.2 & 12.4-17.2 \\ 
   **bareos-database-postgresql** & 12.4-16.2 & 12.4-17.2 & 12.4-17.2 &   & 12.4-17.2 & 12.4-17.2 & 12.4-17.2 \\ 
   **bareos-database-sqlite3** & 12.4-16.2 & 12.4-17.2 & 12.4-17.2 &   & 12.4-17.2 & 12.4-17.2 & 12.4-17.2 \\ 
   **bareos-database-tools** & 12.4-16.2 & 12.4-17.2 & 12.4-17.2 &   & 12.4-17.2 & 12.4-17.2 & 12.4-17.2 \\ 
   **bareos-director** & 12.4-16.2 & 12.4-17.2 & 12.4-17.2 &   & 12.4-17.2 & 12.4-17.2 & 12.4-17.2 \\ 
   **bareos-director-python-plugin** &   & 14.2-17.2 & 14.2-17.2 &   &   & 14.2-17.2 & 14.2-17.2 \\ 
   **bareos-filedaemon** & 12.4-16.2 & 12.4-17.2 & 12.4-17.2 & 13.2-14.2 & 12.4-17.2 & 12.4-17.2 & 12.4-17.2 \\ 
   **bareos-filedaemon-ceph-plugin** &   &   &   &   &   &   & 15.2-17.2 \\ 
   **bareos-filedaemon-glusterfs-plugin** &   &   & 15.2-17.2 &   &   &   & 15.2-17.2 \\ 
   **bareos-filedaemon-ldap-python-plugin** &   & 15.2-17.2 & 15.2-17.2 &   &   & 15.2-17.2 & 15.2-17.2 \\ 
   **bareos-filedaemon-python-plugin** &   & 14.2-17.2 & 14.2-17.2 &   &   & 14.2-17.2 & 14.2-17.2 \\ 
   **bareos-regress-config** &   & 17.2 & 17.2 &   & 17.2 & 17.2 & 17.2 \\ 
   **bareos-storage** & 12.4-16.2 & 12.4-17.2 & 12.4-17.2 &   & 12.4-17.2 & 12.4-17.2 & 12.4-17.2 \\ 
   **bareos-storage-ceph** &   &   &   &   &   &   & 14.2-17.2 \\ 
   **bareos-storage-droplet** &   &   & 17.2 &   &   &   & 17.2 \\ 
   **bareos-storage-fifo** & 14.2-16.2 & 14.2-17.2 & 14.2-17.2 &   & 14.2-17.2 & 14.2-17.2 & 14.2-17.2 \\ 
   **bareos-storage-glusterfs** &   &   & 14.2-17.2 &   &   &   & 14.2-17.2 \\ 
   **bareos-storage-python-plugin** &   & 14.2-17.2 & 14.2-17.2 &   &   & 14.2-17.2 & 14.2-17.2 \\ 
   **bareos-storage-tape** & 12.4-16.2 & 12.4-17.2 & 12.4-17.2 &   & 12.4-17.2 & 12.4-17.2 & 12.4-17.2 \\ 
   **bareos-tools** & 12.4-16.2 & 12.4-17.2 & 12.4-17.2 &   & 12.4-17.2 & 12.4-17.2 & 12.4-17.2 \\ 
   **bareos-traymonitor** &   & 12.4-17.2 & 12.4-17.2 &   &   & 12.4-17.2 & 12.4-17.2 \\ 
   **bareos-vadp-dumper** &   &   & 15.2-17.2 &   &   &   & 15.2-17.2 \\ 
   **bareos-vmware-plugin** &   &   & 15.2-17.2 &   &   &   & 15.2-17.2 \\ 
   **bareos-vmware-vix-disklib** &   &   & 15.2-17.2 &   &   &   & 15.2-17.2 \\ 
   **bareos-webui** &   & 15.2 & 15.2-17.2 &   &   & 15.2 & 15.2-17.2 \\ 
   **libdroplet** &   & 17.2 & 17.2 &   &   & 17.2 & 17.2 \\ 
   **libfastlz** & 12.4-16.2 & 12.4-17.2 & 12.4-17.2 & 13.2-14.2 & 12.4-17.2 & 12.4-17.2 & 12.4-17.2 \\ 
   **lzo** & 12.4-16.2 &   &   & 13.2-14.2 & 12.4-17.2 &   &   \\ 
   **python-bareos** &   & 17.2 & 17.2 &   & 17.2 & 17.2 & 17.2 \\ 
   \hline 
   \end{longtable}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

=========================================================== ========================================= ============================= ========= ========= ========= ========= =========
\                                                           :strong:`CentOS`  .. raw:: latex                                                       
                                                                                                                                                                           
                                                                                                         :strong:`RHEL`                                         
\                                                           5                                         6                             7         4         5         6         7
**bareos**                               12.4-16.2                                 12.4-17.2                     12.4-17.2 13.2-14.2 12.4-17.2 12.4-17.2 12.4-17.2
**bareos-bat**                                                                     12.4-16.2                     12.4-16.2                     12.4-16.2 12.4-16.2
**bareos-bconsole**                      12.4-16.2                                 12.4-17.2                     12.4-17.2 13.2-14.2 12.4-17.2 12.4-17.2 12.4-17.2
**bareos-client**                        12.4-16.2                                 12.4-17.2                     12.4-17.2 13.2-14.2 12.4-17.2 12.4-17.2 12.4-17.2
**bareos-common**                        12.4-16.2                                 12.4-17.2                     12.4-17.2 13.2-14.2 12.4-17.2 12.4-17.2 12.4-17.2
**bareos-database-common**               12.4-16.2                                 12.4-17.2                     12.4-17.2           12.4-17.2 12.4-17.2 12.4-17.2
**bareos-database-mysql**                12.4-16.2                                 12.4-17.2                     12.4-17.2           12.4-17.2 12.4-17.2 12.4-17.2
**bareos-database-postgresql**           12.4-16.2                                 12.4-17.2                     12.4-17.2           12.4-17.2 12.4-17.2 12.4-17.2
**bareos-database-sqlite3**              12.4-16.2                                 12.4-17.2                     12.4-17.2           12.4-17.2 12.4-17.2 12.4-17.2
**bareos-database-tools**                12.4-16.2                                 12.4-17.2                     12.4-17.2           12.4-17.2 12.4-17.2 12.4-17.2
**bareos-director**                      12.4-16.2                                 12.4-17.2                     12.4-17.2           12.4-17.2 12.4-17.2 12.4-17.2
**bareos-director-python-plugin**                                                  14.2-17.2                     14.2-17.2                     14.2-17.2 14.2-17.2
**bareos-filedaemon**                    12.4-16.2                                 12.4-17.2                     12.4-17.2 13.2-14.2 12.4-17.2 12.4-17.2 12.4-17.2
**bareos-filedaemon-ceph-plugin**                                                                                                                        15.2-17.2
**bareos-filedaemon-glusterfs-plugin**                                                                           15.2-17.2                               15.2-17.2
**bareos-filedaemon-ldap-python-plugin**                                           15.2-17.2                     15.2-17.2                     15.2-17.2 15.2-17.2
**bareos-filedaemon-python-plugin**                                                14.2-17.2                     14.2-17.2                     14.2-17.2 14.2-17.2
**bareos-regress-config**                                                          17.2                          17.2                17.2      17.2      17.2
**bareos-storage**                       12.4-16.2                                 12.4-17.2                     12.4-17.2           12.4-17.2 12.4-17.2 12.4-17.2
**bareos-storage-ceph**                                                                                                                                  14.2-17.2
**bareos-storage-droplet**                                                                                       17.2                                    17.2
**bareos-storage-fifo**                  14.2-16.2                                 14.2-17.2                     14.2-17.2           14.2-17.2 14.2-17.2 14.2-17.2
**bareos-storage-glusterfs**                                                                                     14.2-17.2                               14.2-17.2
**bareos-storage-python-plugin**                                                   14.2-17.2                     14.2-17.2                     14.2-17.2 14.2-17.2
**bareos-storage-tape**                  12.4-16.2                                 12.4-17.2                     12.4-17.2           12.4-17.2 12.4-17.2 12.4-17.2
**bareos-tools**                         12.4-16.2                                 12.4-17.2                     12.4-17.2           12.4-17.2 12.4-17.2 12.4-17.2
**bareos-traymonitor**                                                             12.4-17.2                     12.4-17.2                     12.4-17.2 12.4-17.2
**bareos-vadp-dumper**                                                                                           15.2-17.2                               15.2-17.2
**bareos-vmware-plugin**                                                                                         15.2-17.2                               15.2-17.2
**bareos-vmware-vix-disklib**                                                                                    15.2-17.2                               15.2-17.2
**bareos-webui**                                                                   15.2                          15.2-17.2                     15.2      15.2-17.2
**libdroplet**                                                                     17.2                          17.2                          17.2      17.2
**libfastlz**                            12.4-16.2                                 12.4-17.2                     12.4-17.2 13.2-14.2 12.4-17.2 12.4-17.2 12.4-17.2
**lzo**                                  12.4-16.2                                                                         13.2-14.2 12.4-17.2          
**python-bareos**                                                                  17.2                          17.2                17.2      17.2      17.2
=========================================================== ========================================= ============================= ========= ========= ========= ========= =========

:index:`[TAG=Platform->Fedora->20] <triple: Platform; Fedora; 20>`:index:`[TAG=Platform->Fedora->21] <triple: Platform; Fedora; 21>`:index:`[TAG=Platform->Fedora->22] <triple: Platform; Fedora; 22>`:index:`[TAG=Platform->Fedora->23] <triple: Platform; Fedora; 23>`:index:`[TAG=Platform->Fedora->24] <triple: Platform; Fedora; 24>`:index:`[TAG=Platform->Fedora->25] <triple: Platform; Fedora; 25>`:index:`[TAG=Platform->Fedora->26] <triple: Platform; Fedora; 26>`:index:`[TAG=Platform->Fedora->27] <triple: Platform; Fedora; 27>`

# Tabular in LaTex format (original)

::

   \begin{longtable}{ l | c | c | c | c | c | c | c | c }
   \hline 
    & 
   :strong:`Fedora` 
   \\ 
    & 20 & 21 & 22 & 23 & 24 & 25 & 26 & 27\\ 
   \hline 
   \hline 
   **bareos** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-bat** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 &   &   &   \\ 
   **bareos-bconsole** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-client** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-common** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-database-common** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-database-mysql** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-database-postgresql** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-database-sqlite3** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-database-tools** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-director** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-director-python-plugin** & 14.2-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-filedaemon** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-filedaemon-glusterfs-plugin** & 15.2 & 15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-filedaemon-ldap-python-plugin** & 15.2 & 15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-filedaemon-python-plugin** & 14.2-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-regress-config** &   &   &   &   &   & 17.2 & 17.2 &   \\ 
   **bareos-storage** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-storage-fifo** & 14.2-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-storage-glusterfs** & 14.2-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-storage-python-plugin** & 14.2-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-storage-tape** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-tools** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-traymonitor** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **bareos-webui** & 15.2 & 15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 &   \\ 
   **libfastlz** & 12.4-15.2 & 14.2-15.2 & 15.2 & 15.2-16.2 & 16.2 & 17.2 & 17.2 & 17.2 \\ 
   **python-bareos** &   &   &   &   &   & 17.2 & 17.2 & 17.2 \\ 
   \hline 
   \end{longtable}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

=========================================================== =============================== ========= ==== ========= ==== ==== ==== ====
\                                                           .. raw:: latex                                                         
                                                                                                                                   
                                                               :strong:`Fedora`                                         
\                                                           20                              21        22   23        24   25   26   27
**bareos**                               12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-bat**                           12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2          
**bareos-bconsole**                      12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-client**                        12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-common**                        12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-database-common**               12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-database-mysql**                12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-database-postgresql**           12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-database-sqlite3**              12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-database-tools**                12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-director**                      12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-director-python-plugin**        14.2-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-filedaemon**                    12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-filedaemon-glusterfs-plugin**   15.2                            15.2      15.2 15.2-16.2 16.2 17.2 17.2
**bareos-filedaemon-ldap-python-plugin** 15.2                            15.2      15.2 15.2-16.2 16.2 17.2 17.2
**bareos-filedaemon-python-plugin**      14.2-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-regress-config**                                                                              17.2 17.2
**bareos-storage**                       12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-storage-fifo**                  14.2-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-storage-glusterfs**             14.2-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-storage-python-plugin**         14.2-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-storage-tape**                  12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-tools**                         12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-traymonitor**                   12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2
**bareos-webui**                         15.2                            15.2      15.2 15.2-16.2 16.2 17.2 17.2
**libfastlz**                            12.4-15.2                       14.2-15.2 15.2 15.2-16.2 16.2 17.2 17.2 17.2
**python-bareos**                                                                                      17.2 17.2 17.2
=========================================================== =============================== ========= ==== ========= ==== ==== ==== ====

:index:`[TAG=Platform->SLES->10sp4] <triple: Platform; SLES; 10sp4>`:index:`[TAG=Platform->SLES->11sp4] <triple: Platform; SLES; 11sp4>`:index:`[TAG=Platform->SLES->12sp1] <triple: Platform; SLES; 12sp1>`:index:`[TAG=Platform->SLES->12sp2] <triple: Platform; SLES; 12sp2>`:index:`[TAG=Platform->SLES->12sp3] <triple: Platform; SLES; 12sp3>`

# Tabular in LaTex format (original)

::

   \begin{longtable}{ l | c | c | c | c | c }
   \hline 
    & 
   :strong:`SLES` 
   \\ 
    & 10sp4 & 11sp4 & 12sp1 & 12sp2 & 12sp3\\ 
   \hline 
   \hline 
   **bareos** & 14.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-bat** &   & 14.2-16.2 & 14.2-16.2 &   &   \\ 
   **bareos-bconsole** & 14.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-client** & 14.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-common** & 14.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-database-common** & 14.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-database-mysql** & 14.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-database-postgresql** & 14.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-database-sqlite3** &   & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-database-tools** & 14.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-director** & 14.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-director-python-plugin** &   & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-filedaemon** & 14.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-filedaemon-ceph-plugin** &   &   & 15.2-17.2 &   &   \\ 
   **bareos-filedaemon-ldap-python-plugin** &   & 15.2-17.2 & 15.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-filedaemon-python-plugin** &   & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-regress-config** &   & 17.2 & 17.2 & 17.2 & 17.2 \\ 
   **bareos-storage** & 14.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-storage-ceph** &   &   & 15.2-17.2 &   &   \\ 
   **bareos-storage-droplet** &   &   & 17.2 & 17.2 & 17.2 \\ 
   **bareos-storage-fifo** & 14.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-storage-python-plugin** &   & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-storage-tape** & 14.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-tools** & 14.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-traymonitor** &   & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-vadp-dumper** &   & 15.2-16.2 & 16.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-vmware-plugin** &   & 15.2-16.2 & 16.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-vmware-vix-disklib** &   & 15.2-16.2 & 16.2-17.2 & 17.2 & 17.2 \\ 
   **bareos-webui** &   & 15.2-17.2 & 15.2-17.2 & 17.2 & 17.2 \\ 
   **libdroplet** &   &   & 17.2 & 17.2 & 17.2 \\ 
   **libfastlz** & 14.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 17.2 \\ 
   **libjansson4** &   & 15.2-17.2 & 15.2-17.2 & 17.2 & 17.2 \\ 
   **libjansson4-32bit** &   & 15.2-17.2 &   &   &   \\ 
   **libjansson4-x86** &   & 15.2-17.2 &   &   &   \\ 
   **python-bareos** &   & 17.2 & 17.2 & 17.2 & 17.2 \\ 
   **python-py** &   & 15.2-16.2 &   &   &   \\ 
   **python-pyvmomi** &   & 15.2-17.2 & 16.2-17.2 & 17.2 & 17.2 \\ 
   **python-requests** &   & 15.2-16.2 &   &   &   \\ 
   **python-six** &   & 15.2-16.2 &   &   &   \\ 
   \hline 
   \end{longtable}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

=========================================================== ============================= ========= ========= ===== =====
\                                                           .. raw:: latex                                         
                                                                                                                   
                                                               :strong:`SLES`                           
\                                                           10sp4                         11sp4     12sp1     12sp2 12sp3
**bareos**                               14.2                          14.2-17.2 14.2-17.2 17.2  17.2
**bareos-bat**                                                         14.2-16.2 14.2-16.2      
**bareos-bconsole**                      14.2                          14.2-17.2 14.2-17.2 17.2  17.2
**bareos-client**                        14.2                          14.2-17.2 14.2-17.2 17.2  17.2
**bareos-common**                        14.2                          14.2-17.2 14.2-17.2 17.2  17.2
**bareos-database-common**               14.2                          14.2-17.2 14.2-17.2 17.2  17.2
**bareos-database-mysql**                14.2                          14.2-17.2 14.2-17.2 17.2  17.2
**bareos-database-postgresql**           14.2                          14.2-17.2 14.2-17.2 17.2  17.2
**bareos-database-sqlite3**                                            14.2-17.2 14.2-17.2 17.2  17.2
**bareos-database-tools**                14.2                          14.2-17.2 14.2-17.2 17.2  17.2
**bareos-director**                      14.2                          14.2-17.2 14.2-17.2 17.2  17.2
**bareos-director-python-plugin**                                      14.2-17.2 14.2-17.2 17.2  17.2
**bareos-filedaemon**                    14.2                          14.2-17.2 14.2-17.2 17.2  17.2
**bareos-filedaemon-ceph-plugin**                                                15.2-17.2      
**bareos-filedaemon-ldap-python-plugin**                               15.2-17.2 15.2-17.2 17.2  17.2
**bareos-filedaemon-python-plugin**                                    14.2-17.2 14.2-17.2 17.2  17.2
**bareos-regress-config**                                              17.2      17.2      17.2  17.2
**bareos-storage**                       14.2                          14.2-17.2 14.2-17.2 17.2  17.2
**bareos-storage-ceph**                                                          15.2-17.2      
**bareos-storage-droplet**                                                       17.2      17.2  17.2
**bareos-storage-fifo**                  14.2                          14.2-17.2 14.2-17.2 17.2  17.2
**bareos-storage-python-plugin**                                       14.2-17.2 14.2-17.2 17.2  17.2
**bareos-storage-tape**                  14.2                          14.2-17.2 14.2-17.2 17.2  17.2
**bareos-tools**                         14.2                          14.2-17.2 14.2-17.2 17.2  17.2
**bareos-traymonitor**                                                 14.2-17.2 14.2-17.2 17.2  17.2
**bareos-vadp-dumper**                                                 15.2-16.2 16.2-17.2 17.2  17.2
**bareos-vmware-plugin**                                               15.2-16.2 16.2-17.2 17.2  17.2
**bareos-vmware-vix-disklib**                                          15.2-16.2 16.2-17.2 17.2  17.2
**bareos-webui**                                                       15.2-17.2 15.2-17.2 17.2  17.2
**libdroplet**                                                                   17.2      17.2  17.2
**libfastlz**                            14.2                          14.2-17.2 14.2-17.2 17.2  17.2
**libjansson4**                                                        15.2-17.2 15.2-17.2 17.2  17.2
**libjansson4-32bit**                                                  15.2-17.2                
**libjansson4-x86**                                                    15.2-17.2                
**python-bareos**                                                      17.2      17.2      17.2  17.2
**python-py**                                                          15.2-16.2                
**python-pyvmomi**                                                     15.2-17.2 16.2-17.2 17.2  17.2
**python-requests**                                                    15.2-16.2                
**python-six**                                                         15.2-16.2                
=========================================================== ============================= ========= ========= ===== =====

:index:`[TAG=Platform->openSUSE->13.1] <triple: Platform; openSUSE; 13.1>`:index:`[TAG=Platform->openSUSE->13.2] <triple: Platform; openSUSE; 13.2>`:index:`[TAG=Platform->openSUSE->42.1] <triple: Platform; openSUSE; 42.1>`:index:`[TAG=Platform->openSUSE->42.2] <triple: Platform; openSUSE; 42.2>`:index:`[TAG=Platform->openSUSE->42.3] <triple: Platform; openSUSE; 42.3>`

# Tabular in LaTex format (original)

::

   \begin{longtable}{ l | c | c | c | c | c }
   \hline 
    & 
   :strong:`openSUSE` 
   \\ 
    & 13.1 & 13.2 & 42.1 & 42.2 & 42.3\\ 
   \hline 
   \hline 
   **bareos** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-bat** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 &   &   \\ 
   **bareos-bconsole** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-client** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-common** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-database-common** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-database-mysql** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-database-postgresql** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-database-sqlite3** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-database-tools** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-director** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-director-python-plugin** & 14.2-15.2 & 14.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-filedaemon** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-filedaemon-ldap-python-plugin** & 15.2 & 15.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-filedaemon-python-plugin** & 14.2-15.2 & 14.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-regress-config** &   &   &   & 17.2 & 17.2 \\ 
   **bareos-storage** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-storage-droplet** &   &   &   & 17.2 & 17.2 \\ 
   **bareos-storage-fifo** & 14.2-15.2 & 14.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-storage-python-plugin** & 14.2-15.2 & 14.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-storage-tape** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-tools** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-traymonitor** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **bareos-webui** & 15.2 & 15.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **libdroplet** &   &   &   & 17.2 & 17.2 \\ 
   **libfastlz** & 12.4-15.2 & 13.2-16.2 & 15.2-16.2 & 17.2 & 17.2 \\ 
   **python-bareos** &   &   &   & 17.2 & 17.2 \\ 
   \hline 
   \end{longtable}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

=========================================================== ================================= ========= ========= ==== ====
\                                                           .. raw:: latex                                            
                                                                                                                      
                                                               :strong:`openSUSE`                          
\                                                           13.1                              13.2      42.1      42.2 42.3
**bareos**                               12.4-15.2                         13.2-16.2 15.2-16.2 17.2 17.2
**bareos-bat**                           12.4-15.2                         13.2-16.2 15.2-16.2     
**bareos-bconsole**                      12.4-15.2                         13.2-16.2 15.2-16.2 17.2 17.2
**bareos-client**                        12.4-15.2                         13.2-16.2 15.2-16.2 17.2 17.2
**bareos-common**                        12.4-15.2                         13.2-16.2 15.2-16.2 17.2 17.2
**bareos-database-common**               12.4-15.2                         13.2-16.2 15.2-16.2 17.2 17.2
**bareos-database-mysql**                12.4-15.2                         13.2-16.2 15.2-16.2 17.2 17.2
**bareos-database-postgresql**           12.4-15.2                         13.2-16.2 15.2-16.2 17.2 17.2
**bareos-database-sqlite3**              12.4-15.2                         13.2-16.2 15.2-16.2 17.2 17.2
**bareos-database-tools**                12.4-15.2                         13.2-16.2 15.2-16.2 17.2 17.2
**bareos-director**                      12.4-15.2                         13.2-16.2 15.2-16.2 17.2 17.2
**bareos-director-python-plugin**        14.2-15.2                         14.2-16.2 15.2-16.2 17.2 17.2
**bareos-filedaemon**                    12.4-15.2                         13.2-16.2 15.2-16.2 17.2 17.2
**bareos-filedaemon-ldap-python-plugin** 15.2                              15.2-16.2 15.2-16.2 17.2 17.2
**bareos-filedaemon-python-plugin**      14.2-15.2                         14.2-16.2 15.2-16.2 17.2 17.2
**bareos-regress-config**                                                                      17.2 17.2
**bareos-storage**                       12.4-15.2                         13.2-16.2 15.2-16.2 17.2 17.2
**bareos-storage-droplet**                                                                     17.2 17.2
**bareos-storage-fifo**                  14.2-15.2                         14.2-16.2 15.2-16.2 17.2 17.2
**bareos-storage-python-plugin**         14.2-15.2                         14.2-16.2 15.2-16.2 17.2 17.2
**bareos-storage-tape**                  12.4-15.2                         13.2-16.2 15.2-16.2 17.2 17.2
**bareos-tools**                         12.4-15.2                         13.2-16.2 15.2-16.2 17.2 17.2
**bareos-traymonitor**                   12.4-15.2                         13.2-16.2 15.2-16.2 17.2 17.2
**bareos-webui**                         15.2                              15.2-16.2 15.2-16.2 17.2 17.2
**libdroplet**                                                                                 17.2 17.2
**libfastlz**                            12.4-15.2                         13.2-16.2 15.2-16.2 17.2 17.2
**python-bareos**                                                                              17.2 17.2
=========================================================== ================================= ========= ========= ==== ====

:index:`[TAG=Platform->Debian->6] <triple: Platform; Debian; 6>`:index:`[TAG=Platform->Debian->7] <triple: Platform; Debian; 7>`:index:`[TAG=Platform->Debian->8] <triple: Platform; Debian; 8>`:index:`[TAG=Platform->Debian->9] <triple: Platform; Debian; 9>`:index:`[TAG=Platform->Univention->4.0] <triple: Platform; Univention; 4.0>`:index:`[TAG=Platform->Univention->4.2] <triple: Platform; Univention; 4.2>`

# Tabular in LaTex format (original)

::

   \begin{longtable}{ l | c | c | c | c | c | c }
   \hline 
    & 
   :strong:`Debian`  &
   :strong:`Univention` 
   \\ 
    & 6 & 7 & 8 & 9 & 4.0 & 4.2\\ 
   \hline 
   \hline 
   **bareos** & 12.4-15.2 & 12.4-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-bat** & 12.4-15.2 & 12.4-16.2 & 14.2-16.2 &   & 15.2-16.2 &   \\ 
   **bareos-bconsole** & 12.4-15.2 & 12.4-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-client** & 12.4-15.2 & 12.4-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-common** & 12.4-15.2 & 12.4-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-database-common** & 12.4-15.2 & 12.4-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-database-mysql** & 12.4-15.2 & 12.4-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-database-postgresql** & 12.4-15.2 & 12.4-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-database-sqlite3** & 12.4-15.2 & 12.4-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-database-tools** & 12.4-15.2 & 12.4-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-director** & 12.4-15.2 & 12.4-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-director-python-plugin** & 14.2-15.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-filedaemon** & 12.4-15.2 & 12.4-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-filedaemon-ceph-plugin** &   &   & 15.2-16.2 & 17.2 &   &   \\ 
   **bareos-filedaemon-glusterfs-plugin** &   &   & 15.2-17.2 & 17.2 &   & 17.2 \\ 
   **bareos-filedaemon-ldap-python-plugin** & 15.2 & 15.2-17.2 & 15.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-filedaemon-python-plugin** & 14.2-15.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-regress-config** &   & 17.2 & 17.2 & 17.2 &   & 17.2 \\ 
   **bareos-storage** & 12.4-15.2 & 12.4-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-storage-ceph** &   &   & 15.2-16.2 & 17.2 &   &   \\ 
   **bareos-storage-fifo** & 14.2-15.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-storage-glusterfs** &   &   & 15.2-17.2 & 17.2 &   & 17.2 \\ 
   **bareos-storage-python-plugin** & 14.2-15.2 & 14.2-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-storage-tape** & 12.4-15.2 & 12.4-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-tools** & 12.4-15.2 & 12.4-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-traymonitor** & 12.4-15.2 & 12.4-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **bareos-vadp-dumper** &   &   & 15.2-17.2 &   &   &   \\ 
   **bareos-vmware-plugin** &   &   & 15.2-17.2 &   &   &   \\ 
   **bareos-vmware-vix-disklib** &   &   & 17.2 &   &   & 17.2 \\ 
   **bareos-vmware-vix-disklib5** &   &   & 15.2-16.2 &   &   &   \\ 
   **bareos-webui** &   & 15.2-17.2 & 15.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **libfastlz** & 12.4-15.2 & 12.4-17.2 & 14.2-17.2 & 17.2 & 15.2-16.2 & 17.2 \\ 
   **libjansson4** & 15.2 &   &   &   &   &   \\ 
   **python-bareos** &   &   & 17.2 & 17.2 &   & 17.2 \\ 
   **univention-bareos** &   &   &   &   & 15.2-16.2 & 17.2 \\ 
   \hline 
   \end{longtable}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

=========================================================== ========================================= =================================== ========= ==== ========= ====
\                                                           :strong:`Debian`  .. raw:: latex                                              
                                                                                                                                                                  
                                                                                                         :strong:`Univention`                          
\                                                           6                                         7                                   8         9    4.0       4.2
**bareos**                               12.4-15.2                                 12.4-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-bat**                           12.4-15.2                                 12.4-16.2                           14.2-16.2      15.2-16.2
**bareos-bconsole**                      12.4-15.2                                 12.4-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-client**                        12.4-15.2                                 12.4-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-common**                        12.4-15.2                                 12.4-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-database-common**               12.4-15.2                                 12.4-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-database-mysql**                12.4-15.2                                 12.4-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-database-postgresql**           12.4-15.2                                 12.4-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-database-sqlite3**              12.4-15.2                                 12.4-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-database-tools**                12.4-15.2                                 12.4-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-director**                      12.4-15.2                                 12.4-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-director-python-plugin**        14.2-15.2                                 14.2-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-filedaemon**                    12.4-15.2                                 12.4-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-filedaemon-ceph-plugin**                                                                                      15.2-16.2 17.2          
**bareos-filedaemon-glusterfs-plugin**                                                                                 15.2-17.2 17.2           17.2
**bareos-filedaemon-ldap-python-plugin** 15.2                                      15.2-17.2                           15.2-17.2 17.2 15.2-16.2 17.2
**bareos-filedaemon-python-plugin**      14.2-15.2                                 14.2-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-regress-config**                                                          17.2                                17.2      17.2           17.2
**bareos-storage**                       12.4-15.2                                 12.4-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-storage-ceph**                                                                                                15.2-16.2 17.2          
**bareos-storage-fifo**                  14.2-15.2                                 14.2-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-storage-glusterfs**                                                                                           15.2-17.2 17.2           17.2
**bareos-storage-python-plugin**         14.2-15.2                                 14.2-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-storage-tape**                  12.4-15.2                                 12.4-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-tools**                         12.4-15.2                                 12.4-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-traymonitor**                   12.4-15.2                                 12.4-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**bareos-vadp-dumper**                                                                                                 15.2-17.2               
**bareos-vmware-plugin**                                                                                               15.2-17.2               
**bareos-vmware-vix-disklib**                                                                                          17.2                     17.2
**bareos-vmware-vix-disklib5**                                                                                         15.2-16.2               
**bareos-webui**                                                                   15.2-17.2                           15.2-17.2 17.2 15.2-16.2 17.2
**libfastlz**                            12.4-15.2                                 12.4-17.2                           14.2-17.2 17.2 15.2-16.2 17.2
**libjansson4**                          15.2                                                                                                  
**python-bareos**                                                                                                      17.2      17.2           17.2
**univention-bareos**                                                                                                                 15.2-16.2 17.2
=========================================================== ========================================= =================================== ========= ==== ========= ====

:index:`[TAG=Platform->Ubuntu->10.04] <triple: Platform; Ubuntu; 10.04>`:index:`[TAG=Platform->Ubuntu->12.04] <triple: Platform; Ubuntu; 12.04>`:index:`[TAG=Platform->Ubuntu->14.04] <triple: Platform; Ubuntu; 14.04>`:index:`[TAG=Platform->Ubuntu->16.04] <triple: Platform; Ubuntu; 16.04>`:index:`[TAG=Platform->Ubuntu->8.04] <triple: Platform; Ubuntu; 8.04>`

# Tabular in LaTex format (original)

::

   \begin{longtable}{ l | c | c | c | c | c }
   \hline 
    & 
   :strong:`Ubuntu` 
   \\ 
    & 10.04 & 12.04 & 14.04 & 16.04 & 8.04\\ 
   \hline 
   \hline 
   **bareos** & 12.4-15.2 & 12.4-17.2 & 13.2-17.2 & 15.2-17.2 & 13.2-14.2 \\ 
   **bareos-bat** & 12.4-15.2 & 12.4-16.2 & 13.2-16.2 & 15.2-16.2 &   \\ 
   **bareos-bconsole** & 12.4-15.2 & 12.4-17.2 & 13.2-17.2 & 15.2-17.2 & 13.2-14.2 \\ 
   **bareos-client** & 12.4-15.2 & 12.4-17.2 & 13.2-17.2 & 15.2-17.2 & 13.2-14.2 \\ 
   **bareos-common** & 12.4-15.2 & 12.4-17.2 & 13.2-17.2 & 15.2-17.2 & 13.2-14.2 \\ 
   **bareos-database-common** & 12.4-15.2 & 12.4-17.2 & 13.2-17.2 & 15.2-17.2 & 13.2-14.2 \\ 
   **bareos-database-mysql** & 12.4-15.2 & 12.4-17.2 & 13.2-17.2 & 15.2-17.2 & 13.2-14.2 \\ 
   **bareos-database-postgresql** & 12.4-15.2 & 12.4-17.2 & 13.2-17.2 & 15.2-17.2 & 13.2-14.2 \\ 
   **bareos-database-sqlite3** & 12.4-15.2 & 12.4-17.2 & 13.2-17.2 & 15.2-17.2 & 13.2-14.2 \\ 
   **bareos-database-tools** & 12.4-15.2 & 12.4-17.2 & 13.2-17.2 & 15.2-17.2 & 13.2-14.2 \\ 
   **bareos-director** & 12.4-15.2 & 12.4-17.2 & 13.2-17.2 & 15.2-17.2 & 13.2-14.2 \\ 
   **bareos-director-python-plugin** & 14.2-15.2 & 14.2-17.2 & 14.2-17.2 & 15.2-17.2 &   \\ 
   **bareos-filedaemon** & 12.4-15.2 & 12.4-17.2 & 13.2-17.2 & 15.2-17.2 & 13.2-14.2 \\ 
   **bareos-filedaemon-ceph-plugin** &   &   & 15.2-16.2 & 15.2-17.2 &   \\ 
   **bareos-filedaemon-glusterfs-plugin** &   &   &   & 15.2-17.2 &   \\ 
   **bareos-filedaemon-ldap-python-plugin** & 15.2 & 15.2-17.2 & 15.2-17.2 & 15.2-17.2 &   \\ 
   **bareos-filedaemon-python-plugin** & 14.2-15.2 & 14.2-17.2 & 14.2-17.2 & 15.2-17.2 &   \\ 
   **bareos-regress-config** &   & 17.2 & 17.2 & 17.2 &   \\ 
   **bareos-storage** & 12.4-15.2 & 12.4-17.2 & 13.2-17.2 & 15.2-17.2 & 13.2-14.2 \\ 
   **bareos-storage-ceph** &   &   & 15.2-16.2 & 15.2-17.2 &   \\ 
   **bareos-storage-fifo** & 14.2-15.2 & 14.2-17.2 & 14.2-17.2 & 15.2-17.2 & 14.2 \\ 
   **bareos-storage-glusterfs** &   &   &   & 15.2-17.2 &   \\ 
   **bareos-storage-python-plugin** & 14.2-15.2 & 14.2-17.2 & 14.2-17.2 & 15.2-17.2 &   \\ 
   **bareos-storage-tape** & 12.4-15.2 & 12.4-17.2 & 13.2-17.2 & 15.2-17.2 & 13.2-14.2 \\ 
   **bareos-tools** & 12.4-15.2 & 12.4-17.2 & 13.2-17.2 & 15.2-17.2 & 13.2-14.2 \\ 
   **bareos-traymonitor** & 12.4-15.2 & 12.4-17.2 & 13.2-17.2 & 15.2-17.2 &   \\ 
   **bareos-vadp-dumper** &   &   &   & 17.2 &   \\ 
   **bareos-vmware-plugin** &   &   &   & 17.2 &   \\ 
   **bareos-vmware-vix-disklib** &   &   &   & 17.2 &   \\ 
   **bareos-webui** & 15.2 & 15.2-17.2 & 15.2-17.2 & 15.2-17.2 &   \\ 
   **libfastlz** & 12.4-15.2 & 12.4-17.2 & 12.4-17.2 & 15.2-17.2 & 13.2-14.2 \\ 
   **libjansson4** & 15.2 &   &   &   &   \\ 
   **python-bareos** &   &   & 17.2 & 17.2 &   \\ 
   \hline 
   \end{longtable}

# Tabular converted from LaTeX to RST (or empty, in case of problems):

=========================================================== =============================== ========= ========= ========= =========
\                                                           .. raw:: latex                                               
                                                                                                                         
                                                               :strong:`Ubuntu`                               
\                                                           10.04                           12.04     14.04     16.04     8.04
**bareos**                               12.4-15.2                       12.4-17.2 13.2-17.2 15.2-17.2 13.2-14.2
**bareos-bat**                           12.4-15.2                       12.4-16.2 13.2-16.2 15.2-16.2
**bareos-bconsole**                      12.4-15.2                       12.4-17.2 13.2-17.2 15.2-17.2 13.2-14.2
**bareos-client**                        12.4-15.2                       12.4-17.2 13.2-17.2 15.2-17.2 13.2-14.2
**bareos-common**                        12.4-15.2                       12.4-17.2 13.2-17.2 15.2-17.2 13.2-14.2
**bareos-database-common**               12.4-15.2                       12.4-17.2 13.2-17.2 15.2-17.2 13.2-14.2
**bareos-database-mysql**                12.4-15.2                       12.4-17.2 13.2-17.2 15.2-17.2 13.2-14.2
**bareos-database-postgresql**           12.4-15.2                       12.4-17.2 13.2-17.2 15.2-17.2 13.2-14.2
**bareos-database-sqlite3**              12.4-15.2                       12.4-17.2 13.2-17.2 15.2-17.2 13.2-14.2
**bareos-database-tools**                12.4-15.2                       12.4-17.2 13.2-17.2 15.2-17.2 13.2-14.2
**bareos-director**                      12.4-15.2                       12.4-17.2 13.2-17.2 15.2-17.2 13.2-14.2
**bareos-director-python-plugin**        14.2-15.2                       14.2-17.2 14.2-17.2 15.2-17.2
**bareos-filedaemon**                    12.4-15.2                       12.4-17.2 13.2-17.2 15.2-17.2 13.2-14.2
**bareos-filedaemon-ceph-plugin**                                                  15.2-16.2 15.2-17.2
**bareos-filedaemon-glusterfs-plugin**                                                       15.2-17.2
**bareos-filedaemon-ldap-python-plugin** 15.2                            15.2-17.2 15.2-17.2 15.2-17.2
**bareos-filedaemon-python-plugin**      14.2-15.2                       14.2-17.2 14.2-17.2 15.2-17.2
**bareos-regress-config**                                                17.2      17.2      17.2     
**bareos-storage**                       12.4-15.2                       12.4-17.2 13.2-17.2 15.2-17.2 13.2-14.2
**bareos-storage-ceph**                                                            15.2-16.2 15.2-17.2
**bareos-storage-fifo**                  14.2-15.2                       14.2-17.2 14.2-17.2 15.2-17.2 14.2
**bareos-storage-glusterfs**                                                                 15.2-17.2
**bareos-storage-python-plugin**         14.2-15.2                       14.2-17.2 14.2-17.2 15.2-17.2
**bareos-storage-tape**                  12.4-15.2                       12.4-17.2 13.2-17.2 15.2-17.2 13.2-14.2
**bareos-tools**                         12.4-15.2                       12.4-17.2 13.2-17.2 15.2-17.2 13.2-14.2
**bareos-traymonitor**                   12.4-15.2                       12.4-17.2 13.2-17.2 15.2-17.2
**bareos-vadp-dumper**                                                                       17.2     
**bareos-vmware-plugin**                                                                     17.2     
**bareos-vmware-vix-disklib**                                                                17.2     
**bareos-webui**                         15.2                            15.2-17.2 15.2-17.2 15.2-17.2
**libfastlz**                            12.4-15.2                       12.4-17.2 12.4-17.2 15.2-17.2 13.2-14.2
**libjansson4**                          15.2                                                         
**python-bareos**                                                                  17.2      17.2     
=========================================================== =============================== ========= ========= ========= =========

.. _section-UniventionCorporateServer:

Univention Corporate Server
~~~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`[TAG=Platform->Univention Corporate Server|see {Platform, Univention}] <pair: Platform; Univention Corporate Server|see {Platform, Univention}>` :os:`Univention` The Bareos version for the Univention App Center integraties into the Univention Enterprise Linux environment, making it easy to backup all the systems managed by the central Univention Corporate Server.

Preamble
^^^^^^^^

The `Univention Corporate Server <http://www.univention.de/>`_ is an enterprise Linux distribution based on Debian. It consists of an integrated management system for the centralised administration of servers, computer workplaces, users and their rights as well as a wide range of server applications. It also includes an Unvention App Center for the easy installation and management of extensions and appliances.

Bareos is part of the `App Center <https://www.univention.de/produkte/univention-app-center/app-katalog/bareos/>`_ and therefore an Univention environment can easily be extended to provide backup functionality for the Univention servers as well as for the connected client systems. Using the Univention Management Console (UMC), you can also create backup jobs for client computers (Windows or Linux systems), without the need of editing configuration files.

The Bareos Univention App is shipped with a default configuration for the director daemon and the storage daemon.



.. warning::
   You need to review some Univention configuration registry (UCR) variables. Most likely, you will want to set the location where the backups are stored. Otherwise, you may quickly run out of disk space on your backup server!

You will find further information under :ref:`section-UniventionBackupStorage`.

Quick Start
^^^^^^^^^^^

-  Determine the space requirements and where to store your backup data

-  Set the ``bareos/*`` UCR variables according to your needs, see :ref:`section-UCR`

-  Restart :command:`bareos-dir`, :command:`bareos-sd` and :command:`bareos-fd` (or simply reboot the server)

-  Install the Bareos file daemon on clients and copy Director configuration resource file from

   -  

      :file:`/etc/bareos/bareos-dir-export/client/<clientname>-fd/bareos-fd.d/director/*.conf`

   -  (or :file:`/etc/bareos/autogenerated/client-configs/<hostname>.conf`, if Bareos < 16.2.0)

   For details, see :ref:`section-UniventionAddClient`.

-  Enable backup jobs for clients in the Univention Management Console

.. _section-UCR:

UCR variables
^^^^^^^^^^^^^

``bareos/filestorage``
   : /var/lib/bareos/storage (default)

   -  Location where to store the backup files. Make sure, it offers enough disk space for a configured backup volumes.

``bareos/max_full_volume_bytes``
   : 20 (default)

   -  Maximum size (in GB) of a volume for the :config:option:`dir/pool = Full`\  backup pool

``bareos/max_full_volumes``
   : 1 (default)

   -  Maximum number of volumes for the :config:option:`dir/pool = Full`\  backup pool

``bareos/max_diff_volume_bytes``
   : 10 (default)

   -  Maximum size (in GB) of a volume for the :config:option:`dir/pool = Differential`\  backup pool

``bareos/max_diff_volumes``
   : 1 (default)

   -  Maximum number of volumes for the :config:option:`dir/pool = Differential`\  backup pool

``bareos/max_incr_volume_bytes``
   : 1 (default)

   -  Maximum size (in GB) of a volume for the :config:option:`dir/pool = Incremental`\  backup pool

``bareos/max_incr_volumes``
   : 1 (default)

   -  Maximum number of volumes for the :config:option:`dir/pool = Incremental`\  backup pool

``bareos/backup_myself``
   : no (default)

   no
      don’t backup the server itself

   yes
      backup the server itself

``bareos/webui/console/user1/username``
   : Administrator (default)

   -  User name to login at the bareos-webui

``bareos/webui/console/user1/password``
   : (no default value)

   -  Password to login at the bareos-webui

UCR variables can be set via the Univention Configuration Registry Web interface

.. image:: /include/images/univention-configuration-registry-settings.*
   :width: 100.0%



or using the :command:`ucr` command line tool:

.. code-block:: sh
   :caption: Enable backup of the server itself

   root@ucs:~# <input>ucr set bareos/backup_myself=yes</input>
   Setting bareos/backup_myself
   File: /etc/bareos/bareos-dir.conf
   [ ok ] Reloading Bareos Director: bareos-dir.



.. warning::
   univention-bareos < 15.2 did require a manual reload/restart of the bareos-dir service:

.. code-block:: sh
   :caption: let bareos-dir reload its configuration

   root@ucs:~# <input>service bareos-dir reload</input>
   [ ok ] Reloading Bareos Director: bareos-dir.

Setup
^^^^^

After installation of the Bareos app, Bareos is ready for operation. A default configuration is created automatically.

Bareos consists of three daemons called :command:`director` (or :command:`bareos-dir`), :command:`storage-daemon` (or :command:`bareos-sd`) and :command:`filedaemon` (or :command:`bareos-fd`). All three daemons are started right after the installation by the Univention App Center.

If you want to enable automatic backups of the server, you need to set the Univention configuration registry (UCR) variable ``bareos/backup_myself`` to :strong:`yes` and reload the director daemon.

Administration
^^^^^^^^^^^^^^

For general tasks the :ref:`bareos-webui <section-webui>` can be used. Additional, there is the :command:`bconsole` command line tool:

.. code-block:: sh
   :caption: Starting the bconsole

   root@ucs:~# <input>bconsole</input>
   Connecting to Director ucs:9101
   1000 OK: ucs-dir Version: 15.2.2 (15 November 2015)
   Enter a period to cancel a command.
   *

For general information, see the :ref:`Bconsole Tuturial <section-TuturialBconsole>`.

Backup Schedule
^^^^^^^^^^^^^^^

As a result of the default configuration located at the :command:`bareos-dir`, the backup schedule will look as follows:

Full Backups
   -  are written into the :config:option:`dir/pool = Full`\  pool

   -  on the first saturday at 21:00 o’clock

   -  and kept for 365 days

Differential Backups
   -  are written into the :config:option:`dir/pool = Differential`\  pool

   -  on every 2nd to 5th saturday at 21:00 o’clock

   -  and kept for 90 days

Incremental Backups
   -  are written into the :config:option:`dir/pool = Incremental`\  pool

   -  on every day from monday to friday at 21:00 o’clock

   -  and kept for 30 days

That means full backups will be written every first saturday at 21:00 o’clock, differential backups every 2nd to 5th saturday at 21:00 o’clock and incremental backups from monday to friday at 21:00 o’clock. So you have got one full backup every month, four weekly differential and 20 daily incremental backups per month.

This schedule is active for the Univention server backup of itself and all other clients, which are backed up through the :command:`bareos-dir` on the Univention server.

There is also a special backup task, which is the Bareos backups itself for a possible disaster recovery. This backup has got its own backup cycle which starts after the main backups. The backup consists of a database backup for the metadata of the Bareos backup server and a backup of the Bareos configuration files under :file:`/etc/bareos/`.

Backup data management
^^^^^^^^^^^^^^^^^^^^^^

Data from the backup jobs is written to volumes, which are organized in pools (see chapter :ref:`DirectorResourcePool`).

The default configuration uses three different pools, called :config:option:`dir/pool = Full`\ , :config:option:`dir/pool = Differential`\  and :config:option:`dir/pool = Incremental`\ , which are used for full backups, differential and incremental backups, respectively.

If you change the UCR variables, the configuration files will be rewritten automatically. After each change you will need to reload the director daemon.

.. code-block:: sh
   :caption: Example for changing the Full pool size to $10 \ast 20$ GB

   root@ucs:~# <input>ucr set bareos/max_full_volumes=10</input>
   Setting bareos/max_full_volumes
   File: /etc/bareos/bareos-dir.conf
   [ ok ] Reloading Bareos Director: bareos-dir.
   root@ucs:~# <input>ucr set bareos/max_full_volume_bytes=20</input>
   Setting bareos/max_full_volume_bytes
   File: /etc/bareos/bareos-dir.conf
   [ ok ] Reloading Bareos Director: bareos-dir.



.. warning::
   This only affects new volumes. Existing volumes will not change there size.

.. _section-UniventionBackupStorage:

Backup Storage
^^^^^^^^^^^^^^



.. warning::
   Using the default configuration, Bareos will store backups on your local disk. You may want to store the data to another location to avoid using up all of your disk space.

The location for backups is :file:`/var/lib/bareos/storage` in the default configuration.

For example, to use a NAS device for storing backups, you can mount your NAS volume via NFS on :file:`/var/lib/bareos/storage`. Alternatively, you can mount the NAS volume to another directory of your own choice, and change the UCR variable ``bareos/filestorage`` to the corresponding path. The directory needs to be writable by user **bareos**.

.. code-block:: sh
   :caption: Example for changing the storage path

   root@ucs:/etc/bareos# <input>ucr set bareos/filestorage=/path/to/your/storage</input>
   Setting bareos/filestorage
   File: /etc/bareos/bareos-sd.conf



.. warning::
   You need to restart the Bareos storage daemon after having changed the storage path:

.. code-block:: sh

   root@ucs:/# <input>service bareos-sd restart</input>

Bareos Webui Configuration
^^^^^^^^^^^^^^^^^^^^^^^^^^

After installation you just need to setup your login credentials via UCR variables. Therefore, set the Univention configuration registry (UCR) variable ``bareos/webui/console/user1/username`` and ``bareos/webui/consoles/user1/password`` according to your needs. The director configuration is automatically reloaded if one of those two variables changes.

Alternatively you can also set those UCR variables via commandline.

.. code-block:: sh
   :caption: Example for changing webui login credentials

   root@ucs:~# <input>ucr set bareos/webui/console/user1/username="bareos"</input>
   Setting bareos/webui/console/user1/username
   File: /etc/bareos/bareos-dir.conf
   [ ok ] Reloading Bareos Director: bareos-dir.
   root@ucs:~# <input>ucr set bareos/webui/console/user1/password="secret"</input>
   Setting bareos/webui/console/user1/password
   File: /etc/bareos/bareos-dir.conf
   [ ok ] Reloading Bareos Director: bareos-dir.

When your login credentials are set, you can login into Bareos Webui by following the entry in your Administration UCS Overview or directly via `https://<UCS_SERVER>/bareos-webui/ <https://<UCS_SERVER>/bareos-webui/>`__.

.. image:: /include/images/univention-ucs-overview-administration.*
   :width: 80.0%



.. _section-UniventionAddClient:

Add a client to the backup
^^^^^^^^^^^^^^^^^^^^^^^^^^

Overview
''''''''

-  Install the Bareos client software on the target system, see :ref:`Adding a Bareos Client <SecondClient>`

-  Use the Univention Management Console to add the client to the backup, see the screenshot below

-  Copy the filedaemon resource configuration file from the Univention server to the target system

Bareos >= 16.2.4
''''''''''''''''

Server-side
           

The Univention Bareos application comes with an automatism for the client and job configuration. If you want to add a client to the Bareos director configuration, you need use the Univention Management Console, select the client you want to backup and set the :strong:`enable backup job` checkbox to true, as shown in the screenshot below.

.. image:: /include/images/univention-client-job-activation.*
   :width: 80.0%




If the name of the client is **testw1.example.com**, corresponding configuration files will be generated:

-  

   :file:`/etc/bareos/autogenerated/clients/testw1.example.com.include`

-  

   :file:`/etc/bareos/bareos-dir-export/client/testw1.example.com-fd/bareos-fd.d/director/bareos-dir.conf`

Generated configuration files under :file:`/etc/bareos/bareos-dir-export/client/` are intended for the target systems. After you have :ref:`installed the Bareos client on the target system <SecondClient>`, copy the generated client configuration over to the client and save it to following directories:

-  on Linux: :file:`/etc/bareos/bareos-fd.d/director/`

-  on Windows: :file:`C:\Program Files\Bareos\bareos-fd.d/director/`

.. code-block:: sh
   :caption: copy client configuration from the server to the testw1.example.com client (Linux)

   root@ucs:~# <input>CLIENTNAME=testw1.example.com</input>
   root@ucs:~# <input>scp /etc/bareos/bareos-dir-export/client/${CLIENTNAME}-fd/bareos-fd.d/director/*.conf root@${CLIENTNAME}:/etc/bareos/bareos-fd.d/director/</input>

Background
''''''''''

The settings for each job resource are defined by the template files you see below:

The files

-  

   :file:`/etc/bareos/autogenerated/clients/generic.template`

-  

   :file:`/etc/bareos/autogenerated/clients/windows.template`

are used as templates for new clients. For Windows clients the file :file:`windows.template` is used, the :file:`generic.template` is used for all other client types.

If you disable the Bareos backup for a client, the client will not be removed from the configuration files. Only the backup job will be set inactive.

If you add three client, your client directory will look similar to this:

.. code-block:: sh

   root@ucs:/etc/bareos/autogenerated/clients# <input>ls -l</input>
   -rw-r--r-- 1 root root 430 16. Mai 15:15 generic.template
   -rw-r----- 1 root bareos 513 21. Mai 14:46 testw1.example.com.include
   -rw-r----- 1 root bareos 518 21. Mai 14:49 testw2.example.com.include
   -rw-r----- 1 root bareos 518 16. Mai 18:17 testw3.example.com.include
   -rw-r--r-- 1 root root 439 16. Mai 15:15 windows.template

The client configuration file contains, as you can see below, the client connection and the job information:

.. code-block:: sh

   root@ucs:/etc/bareos/autogenerated/clients# <input>cat testw2.example.com.include</input>
   Client {
    Name = "testw2.example.com-fd"
    Address = "testw2.example.com"
    Password = "DBLtVnRKq5nRUOrnB3i3qAE38SiDtV8tyhzXIxqR"
   }

   Job {
    Name = "Backup-testw2.example.com" # job name
    Client = "testw2.example.com-fd" # client name
    JobDefs = "DefaultJob" # job definition for the job
    FileSet = "Windows All Drives" # FileSet (data which is backed up)
    Schedule = "WeeklyCycle" # schedule for the backup tasks
    Enabled = "Yes" #this is the ressource which is toggled on/off by enabling or disabling a backup from the univention gui
   }

Bareos < 16.2.0
'''''''''''''''

Older versions of Bareos handle generating the client configuration similar, but not identical:

If the name of the client is **testw1.example.com**, corresponding configuration files will be generated/adapted:

-  creates :file:`/etc/bareos/autogenerated/fd-configs/testw1.example.com.conf`

-  creates :file:`/etc/bareos/autogenerated/clients/testw1.example.com.include`

-  extends :file:`/etc/bareos/autogenerated/clients.include`

Here the files intended for the target systems are generated under :file:`/etc/bareos/autogenerated/fd-configs/` and they do not only definr a director resource, but are full configuration files for the client. After you have :ref:`installed the Bareos client on the target system <SecondClient>`, copy the generated client configuration over to the client and save it to

-  on Linux: :file:`/etc/bareos/bareos-fd.conf`

-  on Windows: :file:`C:\Program Files\Bareos\bareos-fd.conf`

.. code-block:: sh
   :caption: copy client configuration from the server to the testw1.example.com client (Linux)

   root@ucs:~# <input>CLIENTNAME=testw1.example.com</input>
   root@ucs:~# <input>scp /etc/bareos/autogenerated/fd-configs/${CLIENTNAME}.conf root@${CLIENTNAME}:/etc/bareos/bareos-fd.conf</input>

Debian.org / Ubuntu Universe
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`[TAG=Platform->Debian->Debian.org] <triple: Platform; Debian; Debian.org>` :index:`[TAG=Platform->Debian->8] <triple: Platform; Debian; 8>` :index:`[TAG=Platform->Ubuntu->Universe] <triple: Platform; Ubuntu; Universe>` \index[general]{Platform!Ubuntu!Universe!15.04} 

.. _section-DebianOrg:



The distributions of Debian >= 8 include a version of Bareos. Ubuntu Universe >= 15.04 does also include these packages.

In the further text, these version will be named **Bareos (Debian.org)** (also for the Ubuntu Universe version, as this is based on the Debian version).

.. _section-DebianOrgLimitations:

Limitations of the Debian.org/Ubuntu Universe version of Bareos
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Debian.org does not include the libfastlz compression library and therefore the Bareos (Debian.org) packages do not offer the fileset options ``compression=LZFAST``, ``compression=LZ4`` and ``compression=LZ4HC``.

-  Debian.org does not include the **bareos-webui** package.

.. _section-macosx:

Mac OS X
--------

:index:`[TAG=Platform->Mac->OS X] <triple: Platform; Mac; OS X>`

Bareos for MacOS X is available either

-  via the `Homebrew project <https://brew.sh/>`_ (http://formulae.brew.sh/formula/bareos-client) or

-  as pkg file from http://download.bareos.org/bareos/release/latest/MacOS/.

However, you have to choose upfront, which client you want to use. Otherwise conflicts do occur.

Both packages contain the |bareosFd| and :command:`bconsole`.

Installing the Bareos Client as PKG
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:index:`[TAG=Installation->MacOS] <pair: Installation; MacOS>`

The Bareos installer package for Mac OS X contains the |bareosFd| for Mac OS X 10.5 or later.

On your local Mac, you must be an admin user. The main user is an admin user.

Download the :file:`bareos-client*.pkg` installer package from http://download.bareos.org/bareos/release/latest/MacOS/.

Find the .pkg you just downloaded. Install the .pkg by holding the CTRL key, left-clicking the installer and choosing ''open''.

Follow the directions given to you and finish the installation.

Configuration
~~~~~~~~~~~~~

To make use of your |bareosFd| on your system, it is required to configure the |bareosDir| and the local |bareosFd|.

Configure the server-side by follow the instructions at :ref:`section-AddAClient`.

After configuring the server-side you can either transfer the necessary configuration file using following command or configure the client locally.

Option 1: Copy the director resource from the Bareos Director to the Client
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Assuming your client has the DNS entry :strong:`client2.example.com` and has been added to |bareosDir| as :config:option:`bareos-dir/client = client2-fd`\ :

.. code-block:: sh

   scp /etc/bareos/bareos-dir-export/client/client2-fd/bareos-fd.d/director/bareos-dir.conf root@client2.example.com:/usr/local/etc/bareos/bareos-fd.d/director/

This differs in so far, as on Linux the configuration files are located under :file:`/etc/bareos/`, while on MacOS they are located at :file:`/usr/local/etc/bareos/`.

Option 2: Edit the director resource on the Client
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Alternatively, you can edit the file :file:`/usr/local/etc/bareos/bareos-fd.d/director/bareos-dir.conf`.

This can be done by right-clicking the finder icon in your task bar, select ''Go to folder ...'' and paste :file:`/usr/local/etc/bareos/bareos-fd.d/director/`.

Select the :file:`bareos-dir.conf` file and open it.

Alternatively you can also call following command on the command console:

.. code-block:: sh

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

.. code-block:: sh
   :caption: Restart the |bareosFd|

   sudo launchctl stop  org.bareos.bareos-fd
   sudo launchctl start org.bareos.bareos-fd

Verify that the Bareos File Daemon is working
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Open the :command:`bconsole` on your |bareosDir| and check the status of the client with

.. code-block:: bareosconfig

   *<input>status client=client2-fd</input>

In case, the client does not react, following command are useful the check the status:

.. code-block:: sh
   :caption: Verify the status of |bareosFd|

   # check if bareos-fd is started by system:
   sudo launchctl list org.bareos.bareos-fd

   # get process id (PID) of bareos-fd
   pgrep bareos-fd

   # show files opened by bareos-fd
   sudo lsof -p `pgrep bareos-fd`

   # check what process is listening on the |bareosFd| port
   sudo lsof -n -iTCP:9102 | grep LISTEN

You can also manually start bareos-fd in debug mode by:

.. code-block:: sh
   :caption: Start |bareosFd| in debug mode

   sudo /usr/local/sbin/bareos-fd -f -d 100








