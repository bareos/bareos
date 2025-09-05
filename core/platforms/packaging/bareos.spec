#
# spec file for package bareos
# Copyright (c) 2011-2012 Bruno Friedmann (Ioda-Net) and Philipp Storz (dass IT)
#               2013-2025 Bareos GmbH & Co KG
#

Name:       bareos
Version:    0.0.0
Release:    0
Group:      Productivity/Archiving/Backup
License:    AGPL-3.0
BuildRoot:  %{_tmppath}/%{name}-root
URL:        https://www.bareos.org/
Vendor:     The Bareos Team
#Packager:  {_packager}

%define confdir           %{_sysconfdir}/bareos
%define configtemplatedir /usr/lib/%{name}/defaultconfigs
%define library_dir       %{_libdir}/%{name}
%define backend_dir       %{_libdir}/%{name}/backends
%define plugin_dir        %{_libdir}/%{name}/plugins
%define script_dir        /usr/lib/%{name}/scripts
%define working_dir       /var/lib/%{name}
%define bsr_dir           /var/lib/%{name}
# TODO: use /run ?
%define _subsysdir        /var/lock

#
# Generic daemon user and group
#
%define daemon_user             bareos
%define daemon_group            bareos

%define director_daemon_user    %{daemon_user}
%define storage_daemon_user     %{daemon_user}
%define file_daemon_user        root
%define storage_daemon_group    %{daemon_group}

# default settings
%define client_only 0
%define build_qt_monitor 1
%define glusterfs 0
%define droplet 1
%define have_git 1
%define install_suse_fw 0
%define systemd_support 0
%define python_plugins 1
%define contrib 1
%define webui 1
%define enable_grpc 1

# cmake build directory
%define CMAKE_BUILDDIR       cmake-build


BuildRequires: rpcgen
BuildRequires: libtirpc-devel

%if 0%{?fedora} || 0%{?suse_version}
BuildRequires: fmt-devel
%endif

#
# SUSE (openSUSE, SLES) specific settings
#
%if 0%{?sles_version} == 10
%define build_qt_monitor 0
%define have_git 0
%define python_plugins 0
%endif

%if 0%{?suse_version} > 1010 && 0%{?suse_version} < 1500
%define install_suse_fw 1
%define _fwdefdir   %{_sysconfdir}/sysconfig/SuSEfirewall2.d/services
%endif


%if 0%{?suse_version} > 1140
%define systemd_support 1
%endif


#
# RedHat (CentOS, Fedora, RHEL) specific settings
#

%if 0%{?fedora} >= 20
%define glusterfs 1
%define systemd_support 1
%endif

%if 0%{?rhel} >= 7
%define systemd_support 1
%endif

%if 0%{?rhel} >= 7 && (0%{?rhel} <= 9)
%define glusterfs 1
%endif

%if 0%{?rhel} == 7
%define webui 0
%define __python python3
%endif

# use modernized GCC 14 toolchain for C++20 support
%if 0%{?rhel} && 0%{?rhel} <= 9
BuildRequires: gcc-toolset-14-gcc
BuildRequires: gcc-toolset-14-annobin-plugin-gcc
BuildRequires: gcc-toolset-14-gcc-c++
%endif

# rhel <=8 does not have grpc
%if %{defined rhel} && (0%{?rhel} <= 8)
%define enable_grpc 0
%endif
# fedora <=39 does not have grpc
%if %{defined fedora} && (0%{?fedora} <= 39)
%define enable_grpc 0
%endif

%if 0%{?suse_version}
BuildRequires: gcc13
BuildRequires: gcc13-c++
%define enable_grpc 1
%endif

%if 0%{?systemd_support}
BuildRequires: systemd
# see https://en.opensuse.org/openSUSE:Systemd_packaging_guidelines
%if 0%{?suse_version} >= 1210
BuildRequires: systemd-rpm-macros
%endif
%{?systemd_requires}
%endif

%if 0%{?glusterfs}
BuildRequires: glusterfs-devel glusterfs-api-devel
%endif

%if 0%{?have_git}
BuildRequires: git-core
%endif

Source0: %{name}-%{version}.tar.gz

BuildRequires: cmake >= 3.17
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: glibc
BuildRequires: glibc-devel
BuildRequires: ncurses-devel
BuildRequires: pam-devel
BuildRequires: pkgconfig
BuildRequires: python-rpm-macros
BuildRequires: readline-devel
BuildRequires: libacl-devel
BuildRequires: libstdc++-devel
BuildRequires: zlib-devel
BuildRequires: openssl-devel
BuildRequires: lzo-devel
BuildRequires: logrotate
BuildRequires: postgresql-devel
BuildRequires: openssl
BuildRequires: libcap-devel
BuildRequires: mtx

%if 0%{?build_qt_monitor}
%if 0%{?suse_version}
BuildRequires: libqt5-qtbase-devel
%else

%if 0%{?rhel} > 7 || 0%{?fedora} >= 29
BuildRequires: qt5-qtbase-devel
%else
BuildRequires: qt-devel
%endif

%endif
%endif

%if 0%{?python_plugins}
BuildRequires: python3-devel >= 3.4
%endif

%if 0%{?suse_version}

# suse_version:
#   1500: SLE_15
#   1315: SLE_12
#   1110: SLE_11

BuildRequires: distribution-release
BuildRequires: shadow
BuildRequires: update-desktop-files
BuildRequires: pkgconfig(libxml-2.0)
BuildRequires: pkgconfig(json-c)

%if 0%{?suse_version} > 1010
# link identical files
BuildRequires: fdupes
BuildRequires: libjansson-devel
BuildRequires: lsb-release
%endif

%else
# non suse

BuildRequires: passwd


# Some magic to be able to determine what platform we are running on.
%if 0%{?rhel} || 0%{?fedora}

%if 0%{?rhel} && 0%{?rhel} < 9
BuildRequires: redhat-lsb
%endif

%if 0%{?fedora}
BuildRequires: fedora-release
%endif

BuildRequires: jansson-devel

%else
# non suse, non redhat: eg. mandriva.

BuildRequires: lsb-release

%endif

%endif

# dependency tricks for vixdisklib
%global __requires_exclude ^(.*libvixDiskLib.*|.*CXXABI_1.3.9.*)$

%define replace_python_shebang sed -i '1s|^#!.*|#!%{__python3} %{py3_shbang_opts}|'

Summary:    Backup Archiving REcovery Open Sourced - metapackage
Requires:   %{name}-director = %{version}
Requires:   %{name}-storage = %{version}
Requires:   %{name}-client = %{version}

%define dscr Bareos - Backup Archiving Recovery Open Sourced. \
Bareos is a set of computer programs that permit you (or the system \
administrator) to manage backup, recovery, and verification of computer \
data across a network of computers of different kinds. In technical terms, \
it is a network client/server based backup program. Bareos is relatively \
easy to use and efficient, while offering many advanced storage management \
features that make it easy to find and recover lost or damaged files. \
Bareos source code has been released under the AGPL version 3 license.




%description
%{dscr}


%if 0%{?suse_version}
%debug_package
%endif

# Notice : Don't try to change the order of package declaration
# You will have side effect with PreReq

%package    bconsole
Summary:    Bareos administration console (CLI)
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common = %{version}

%package    client
Summary:    Bareos client Meta-All-In-One package
Group:      Productivity/Archiving/Backup
Requires:   %{name}-bconsole = %{version}
Requires:   %{name}-filedaemon = %{version}
%if 0%{?suse_version}
Recommends: %{name}-traymonitor = %{version}
%endif

%package    director
Summary:    Bareos Director daemon
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common = %{version}
Requires:   %{name}-database-common = %{version}
Requires:   %{name}-database-tools
%if 0%{?suse_version}
Requires(pre): shadow
# Don't use this option on anything other then SUSE derived distributions
# as Fedora & others don't know this tag
Recommends: logrotate
%else
Requires(pre): shadow-utils
%endif
Provides:   %{name}-dir

%package    storage
Summary:    Bareos Storage daemon
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common = %{version}
Provides:   %{name}-sd
%if 0%{?suse_version}
Requires(pre): shadow
Recommends: bareos-tools
Recommends: logrotate
%else
Requires(pre): shadow-utils
# Recommends would be enough, however only supported by Fedora >= 24.
Requires: bareos-tools
%endif

%package    storage-dedupable
Summary:    Dedupable storage format for the Bareos Storage daemon
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common  = %{version}
Requires:   %{name}-storage = %{version}

%if 0%{?droplet}
%package    storage-droplet
Summary:    Object Storage support (through libdroplet) for the Bareos Storage daemon
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common  = %{version}
Requires:   %{name}-storage = %{version}
%endif

%package    storage-dplcompat
Summary:    Object Storage support for the Bareos Storage daemon
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common  = %{version}
Requires:   %{name}-storage = %{version}

%if 0%{?glusterfs}
%package    storage-glusterfs
Summary:    GlusterFS support for the Bareos Storage daemon
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common  = %{version}
Requires:   %{name}-storage = %{version}
Requires:   glusterfs
%endif

%package    storage-tape
Summary:    Tape support for the Bareos Storage daemon
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common  = %{version}
Requires:   %{name}-storage = %{version}
Requires:   mtx
%if !0%{?suse_version}
Requires:   mt-st
%endif

%package    storage-fifo
Summary:    FIFO support for the Bareos Storage backend
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common  = %{version}
Requires:   %{name}-storage = %{version}

%package    filedaemon
Summary:    Bareos File daemon (backup and restore client)
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common = %{version}
Provides:   %{name}-fd
%if 0%{?suse_version}
Requires(pre): shadow
%else
Requires(pre): shadow-utils
%endif

%package    common
Summary:    Common files, required by multiple Bareos packages
Group:      Productivity/Archiving/Backup
Requires(pre): coreutils
Requires(pre): findutils
Requires(pre): gawk
Requires(pre): grep
Requires(pre): openssl
Requires(pre): sed
%if 0%{?suse_version}
Requires(pre): shadow
%else
Requires(pre): glibc-common
Requires(pre): shadow-utils
%endif
Provides:   %{name}-libs

%package    database-common
Summary:    Generic abstraction libs and files to connect to a database
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common = %{version}
Requires:   %{name}-database-backend = %{version}
Requires:   openssl
Provides:   %{name}-sql

%package    database-postgresql
Summary:    Libs & tools for postgresql catalog
Group:      Productivity/Archiving/Backup
Requires:   %{name}-database-common = %{version}
Provides:   %{name}-catalog-postgresql
Provides:   %{name}-database-backend

%package    database-tools
Summary:    Bareos CLI tools with database dependencies (bareos-dbcheck, bscan)
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common = %{version}
Requires:   %{name}-database-common = %{version}
Provides:   %{name}-dbtools

%package    tools
Summary:    Bareos CLI tools (bcopy, bextract, bls, bregex, bwild, bdedupestimate)
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common = %{version}

%if 0%{build_qt_monitor}
%package    traymonitor
Summary:    Bareos Tray Monitor (QT)
Group:      Productivity/Archiving/Backup
# Added to by pass the 09 checker rules (conflict with bareos-tray-monitor.conf)
# This is mostly wrong cause the two binaries can use it!
Conflicts:  %{name}-tray-monitor-gtk
Provides:   %{name}-tray-monitor-qt
%endif

%if 0%{?python_plugins}
%package    director-python3-plugin
Summary:    Python plugin for Bareos Director daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-director = %{version}
Requires:   bareos-director-python-plugins-common = %{version}
Provides:   bareos-director-python-plugin
Obsoletes:  bareos-director-python-plugin <= %{version}

%package    director-python-plugins-common
Summary:    Python plugin for Bareos Director daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-director = %{version}

%if 0%{?enable_grpc}
%package    filedaemon-grpc-python3-plugin
Summary:    Python plugin for Bareos File daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-filedaemon = %{version}
Requires:   bareos-filedaemon-python-plugins-common = %{version}
Provides:   bareos-filedaemon-grpc-python-plugin
Obsoletes:  bareos-filedaemon-grpc-python-plugin <= %{version}
Provides:   bareos-filedaemon-python-plugin
Obsoletes:  bareos-filedaemon-python-plugin <= %{version}
%endif

%package    filedaemon-python3-plugin
Summary:    Python plugin for Bareos File daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-filedaemon = %{version}
Requires:   bareos-filedaemon-python-plugins-common = %{version}
Provides:   bareos-filedaemon-python-plugin
Obsoletes:  bareos-filedaemon-python-plugin <= %{version}

%package    filedaemon-python-plugins-common
Summary:    Python plugin for Bareos File daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-filedaemon = %{version}


%package    filedaemon-ldap-python-plugin
Summary:    LDAP Python plugin for Bareos File daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-filedaemon = %{version}
Requires:   bareos-filedaemon-python-plugin = %{version}
Suggests:   bareos-filedaemon-python3-plugin = %{version}
Requires:   python3-ldap


%package    filedaemon-libcloud-python-plugin
Summary:    Libcloud Python plugin for Bareos File daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-filedaemon = %{version}
Requires:   bareos-filedaemon-python-plugin = %{version}
Suggests:   bareos-filedaemon-python3-plugin = %{version}

%package    filedaemon-postgresql-python-plugin
Summary:    PostgreSQL Python plugin for Bareos File daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-filedaemon = %{version}
Requires:   bareos-filedaemon-python-plugin = %{version}
Suggests:   bareos-filedaemon-python3-plugin = %{version}

%package    filedaemon-percona-xtrabackup-python-plugin
Summary:    Percona xtrabackup Python plugin for Bareos File daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-filedaemon = %{version}
Requires:   bareos-filedaemon-python-plugin = %{version}
Suggests:   bareos-filedaemon-python3-plugin = %{version}

%package    filedaemon-mariabackup-python-plugin
Summary:    Mariabackup Python plugin for Bareos File daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-filedaemon = %{version}
Requires:   bareos-filedaemon-python-plugin = %{version}
Suggests:   bareos-filedaemon-python3-plugin = %{version}


%package    storage-python3-plugin
Summary:    Python plugin for Bareos Storage daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-storage = %{version}
Requires:   bareos-storage-python-plugins-common = %{version}
Provides:   bareos-storage-python-plugin
Obsoletes:  bareos-storage-python-plugin <= %{version}

%package    storage-python-plugins-common
Summary:    Python plugin for Bareos Storage daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-storage = %{version}

# vmware switch is set via --define="vmware 1" in build script when
# vix disklib is detected

%if 0%{?vmware}
# VMware Plugin BEGIN

%package -n     bareos-vadp-dumper
Summary:        VADP Dumper - vStorage APIs for Data Protection Dumper program
Group:          Productivity/Archiving/Backup
Requires:       bareos-vmware-vix-disklib

%description -n bareos-vadp-dumper
Uses vStorage API to connect to VMWare and dump data like virtual disks snapshots
to be used by other programs.


%package -n     bareos-vmware-plugin
Summary:        Bareos VMware plugin
Group:          Productivity/Archiving/Backup
Requires:       bareos-vadp-dumper
Requires:       bareos-filedaemon-python-plugin >= 15.2
Suggests:       bareos-filedaemon-python3-plugin = %{version}

%description -n bareos-vmware-plugin
Uses the VMware API to take snapshots of running VMs and takes
full and incremental backup so snapshots. Restore of a snapshot
is currently supported to the origin VM.

%package -n     bareos-vmware-plugin-compat
Summary:        Bareos VMware plugin compatibility
Group:          Productivity/Archiving/Backup
Requires:       bareos-vmware-plugin

%description -n bareos-vmware-plugin-compat
Keeps bareos/plugins/vmware_plugin subdirectory, which have been used in Bareos <= 16.2.

# VMware Plugin END
%endif

%description director-python3-plugin
%{dscr}

This package contains the python 3 plugin for the director daemon

%description director-python-plugins-common
%{dscr}

This package contains the common files for the python director plugins.

%if 0%{?enable_grpc}
%description filedaemon-grpc-python3-plugin
%{dscr}

This package contains the grpc python 3 plugin for the file daemon

%endif
%description filedaemon-python3-plugin
%{dscr}

This package contains the python 3 plugin for the file daemon

%description filedaemon-python-plugins-common
%{dscr}

This package contains the common files for the python filedaemon plugins.

%description filedaemon-ldap-python-plugin
%{dscr}

This package contains the LDAP python plugin for the file daemon

%description filedaemon-libcloud-python-plugin
%{dscr}

This package contains the Libcloud python plugin for the file daemon

%description filedaemon-postgresql-python-plugin
%{dscr}

This package contains the PostgreSQL python plugin for the file daemon

%description filedaemon-percona-xtrabackup-python-plugin
%{dscr}

This package contains the Percona python plugin for the file daemon

%description filedaemon-mariabackup-python-plugin
%{dscr}

This package contains the Mariabackup python plugin for the file daemon


%description storage-python3-plugin
%{dscr}

This package contains the python 3 plugin for the storage daemon

%description storage-python-plugins-common
%{dscr}

This package contains the common files for the python storage plugins.
%endif

%if 0%{?glusterfs}
%package    filedaemon-glusterfs-plugin
Summary:    GlusterFS plugin for Bareos File daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-filedaemon = %{version}
Requires:   glusterfs

%description filedaemon-glusterfs-plugin
%{dscr}

This package contains the GlusterFS plugin for the file daemon

%endif

%if 0%{?webui}
%package webui
Summary:       Bareos Web User Interface
Group:         Productivity/Archiving/Backup

BuildRequires: httpd
BuildRequires: httpd-devel

Requires: php >= 7.0.0
Requires: php-fpm
Requires: php-bz2
Requires: php-ctype
Requires: php-curl
Requires: php-date
Requires: php-dom
Requires: php-fileinfo
Requires: php-filter
Requires: php-gettext
Requires: php-gd
Requires: php-hash
Requires: php-iconv
Requires: php-intl
Requires: php-json
Requires: php-mbstring
Requires: php-openssl
Requires: php-pcre
Requires: php-reflection
Requires: php-session
Requires: php-simplexml
Requires: php-spl
Requires: php-xml
Requires: php-xmlreader
Requires: php-xmlwriter
Requires: php-zip

%if !0%{?suse_version}
Requires: php-libxml
%endif

Requires: httpd
%if 0%{?rhel} || 0%{?fedora}
%define _apache_conf_dir /etc/httpd/conf.d/
%define www_daemon_user apache
%define www_daemon_group apache
%endif

%if 0%{?suse_version}
Conflicts: mod_php_any
%define _apache_conf_dir /etc/apache2/conf.d/
%define www_daemon_user wwwrun
%define www_daemon_group www
%endif

%description webui
%{dscr}

This package contains the webui (Bareos Web User Interface).
%endif

%if 0%{?contrib}

%package     contrib-tools
Summary:     Additional tools, not part of the Bareos project
Group:       Productivity/Archiving/Backup
Requires:    python-bareos
Requires:    bareos-filedaemon

%description contrib-tools
%{dscr}

This package provides some additional tools, not part of the Bareos project.


%package     contrib-filedaemon-python-plugins
Summary:     Additional File Daemon Python plugins, not part of the Bareos project
Group:       Productivity/Archiving/Backup
Requires:    bareos-filedaemon-python-plugin
Suggests:   bareos-filedaemon-python3-plugin = %{version}

%description contrib-filedaemon-python-plugins
%{dscr}

This package provides additional File Daemon Python plugins, not part of the Bareos project.


%package     contrib-director-python-plugins
Summary:     Additional Director Python plugins, not part of the Bareos project
Group:       Productivity/Archiving/Backup
Requires:    bareos-director-python-plugin
Suggests:   bareos-director-python3-plugin = %{version}

%description contrib-director-python-plugins
%{dscr}

This package provides additional Bareos Director Python plugins, not part of the Bareos project.

# endif: contrib
%endif


%description client
%{dscr}

This package is a meta package requiring the packages
containing the fd and the console.

This is for client only installation.

%description bconsole
%{dscr}

This package contains the bconsole (the CLI interface program)

%description director
%{dscr}

This package contains the Director Service (Bareos main service daemon)

%description storage
%{dscr}

This package contains the Storage Daemon
(Bareos service to read and write data from/to media)

%description storage-tape
%{dscr}

This package contains the Storage Daemon tape support
(Bareos service to read and write data from/to tape media)

%description storage-dedupable
%{dscr}

This package contains the Storage Backend for the dedupable storage format.

%if 0%{?droplet}
%description storage-droplet
%{dscr}

This package contains the Storage backend for Object Storage (through libdroplet).
%endif

%description storage-dplcompat
%{dscr}

This package contains the Storage backend for Object Storage (via scripts).

%if 0%{?glusterfs}
%description storage-glusterfs
%{dscr}

This package contains the Storage backend for GlusterFS.
%endif

%description storage-fifo
%{dscr}

This package contains the Storage backend for FIFO files.
This package is only required, when a resource "Archive Device = fifo"
should be used by the Bareos Storage Daemon.

%description filedaemon
%{dscr}

This package contains the File Daemon
(Bareos client daemon to read/write data from the backed up computer)

%description common
%{dscr}

This package contains the shared libraries that are used by multiple daemons and tools.

%description database-common
%{dscr}

This package contains the shared libraries that abstract the catalog interface

%description database-postgresql
%{dscr}

This package contains the shared library to access postgresql as catalog db.

%description database-tools
%{dscr}

This package contains Bareos database tools.

%description tools
%{dscr}

This package contains Bareos tools.

%if 0%{?build_qt_monitor}
%description traymonitor
%{dscr}

This package contains the tray monitor (QT based).
%endif



%prep
# this is a hack so we always build in "bareos" and not in "bareos-version"
%setup -c -n bareos
[ -d bareos-* ] && mv bareos-*/* .
%if 0%{?contrib}
%replace_python_shebang contrib/misc/bsmc/bin/bsmc
%replace_python_shebang contrib/misc/triggerjob/bareos-triggerjob.py
%replace_python_shebang contrib/misc/chunk_check/chunk_check.py
%endif


%build
# Cleanup defined in Fedora Packaging:Guidelines
# and required repetitive local build of at least CentOS 5.
if [ "%{?buildroot}" -a "%{?buildroot}" != "/" ]; then
    rm -rf "%{?buildroot}"
fi
%if !0%{?suse_version}
export PATH=$PATH:/usr/lib64/qt5/bin:/usr/lib/qt5/bin
%endif
export MTX=/usr/sbin/mtx


mkdir %{CMAKE_BUILDDIR}
pushd %{CMAKE_BUILDDIR}

# use modernized GCC 14 toolchain for C++20 support
%if 0%{?rhel} && 0%{?rhel} <= 9
source /opt/rh/gcc-toolset-14/enable
%endif

# use modern compiler on suse
%if 0%{?suse_version}
CC=gcc-13  ; export CC
CXX=g++-13 ; export CXX
%endif

CFLAGS="${CFLAGS:-%optflags}" ; export CFLAGS ;
CXXFLAGS="${CXXFLAGS:-%optflags}" ; export CXXFLAGS ;

# use our own cmake call instead of cmake macro as it is different on different platforms/versions
cmake  .. \
  -DCMAKE_VERBOSE_MAKEFILE=ON \
  -DCMAKE_INSTALL_PREFIX:PATH=/usr \
  -DCMAKE_INSTALL_LIBDIR:PATH=/usr/lib \
  -DINCLUDE_INSTALL_DIR:PATH=/usr/include \
  -DLIB_INSTALL_DIR:PATH=/usr/lib \
  -DSYSCONF_INSTALL_DIR:PATH=/etc \
  -DSHARE_INSTALL_PREFIX:PATH=/usr/share \
  -DBUILD_SHARED_LIBS:BOOL=ON \
  -Dprefix=%{_prefix}\
  -Dlibdir=%{library_dir} \
  -Dsbindir=%{_sbindir} \
  -Dsysconfdir=%{_sysconfdir} \
  -Dconfdir=%{confdir} \
  -Dmandir=%{_mandir} \
  -Darchivedir=/var/lib/%{name}/storage \
  -Dbackenddir=%{backend_dir} \
  -Dconfigtemplatedir=%{configtemplatedir} \
  -Dscriptdir=%{script_dir} \
  -Dworkingdir=%{working_dir} \
  -Dplugindir=%{plugin_dir} \
  -Dlogdir=/var/log/bareos \
  -Dsubsysdir=%{_subsysdir} \
  -Dscsi-crypto=yes \
  -Dndmp=yes \
%if 0%{?build_qt_monitor}
  -Dtraymonitor=yes \
%endif
%if 0%{?client_only}
  -Dclient-only=yes \
%endif
  -Ddir-user=%{director_daemon_user} \
  -Ddir-group=%{daemon_group} \
  -Dsd-user=%{storage_daemon_user} \
  -Dsd-group=%{storage_daemon_group} \
  -Dfd-user=%{file_daemon_user} \
  -Dfd-group=%{daemon_group} \
  -Ddir-password="XXX_REPLACE_WITH_DIRECTOR_PASSWORD_XXX" \
  -Dfd-password="XXX_REPLACE_WITH_CLIENT_PASSWORD_XXX" \
  -Dsd-password="XXX_REPLACE_WITH_STORAGE_PASSWORD_XXX" \
  -Dmon-dir-password="XXX_REPLACE_WITH_DIRECTOR_MONITOR_PASSWORD_XXX" \
  -Dmon-fd-password="XXX_REPLACE_WITH_CLIENT_MONITOR_PASSWORD_XXX" \
  -Dmon-sd-password="XXX_REPLACE_WITH_STORAGE_MONITOR_PASSWORD_XXX" \
  -Dbasename="XXX_REPLACE_WITH_LOCAL_HOSTNAME_XXX" \
  -Dhostname="XXX_REPLACE_WITH_LOCAL_HOSTNAME_XXX" \
%if 0%{?systemd_support}
  -Dsystemd=yes \
%endif
%if !0%{?webui}
  -DENABLE_WEBUI=no \
%endif
%if 0%{?enable_grpc}
  -DENABLE_GRPC=yes \
%endif
  -Dwebuiconfdir=%{_sysconfdir}/bareos-webui \
  -DVERSION_STRING=%version
%if 0%{?make_build:1}
%make_build
%else
%__make %{?_smp_mflags};
%endif

%check
# run unit tests
pushd %{CMAKE_BUILDDIR}

# run the tests and fail build if test fails
REGRESS_DEBUG=1 ctest -V -S CTestScript.cmake || echo "ctest result:$?"

%install
pushd %{CMAKE_BUILDDIR}
make DESTDIR=%{buildroot} install/fast

popd



install -d -m 755 %{buildroot}/usr/share/applications
install -d -m 755 %{buildroot}/usr/share/pixmaps
install -d -m 755 %{buildroot}%{backend_dir}
install -d -m 755 %{buildroot}%{working_dir}
install -d -m 755 %{buildroot}%{plugin_dir}
install -d -m 750 %{buildroot}%{confdir}/bareos-dir-export/client

#Cleaning
for F in  \
%if 0%{?client_only}
    %{_mandir}/man1/bregex.1.gz \
    %{_mandir}/man1/bsmtp.1.gz \
    %{_mandir}/man1/bwild.1.gz \
    %{_mandir}/man8/bareos-dbcheck.8.gz \
    %{_mandir}/man8/bareos-dir.8.gz \
    %{_mandir}/man8/bareos-sd.8.gz \
    %{_mandir}/man8/bareos.8.gz \
    %{_mandir}/man8/bcopy.8.gz \
    %{_mandir}/man8/bextract.8.gz \
    %{_mandir}/man8/bls.8.gz \
    %{_mandir}/man8/bpluginfo.8.gz \
    %{_mandir}/man8/bscan.8.gz \
    %{_mandir}/man8/bscrypto.8.gz \
    %{_mandir}/man8/btape.8.gz \
    %{_sysconfdir}/logrotate.d/bareos-dir \
    %{_sysconfdir}/logrotate.d/bareos-mtx \
    %{_sysconfdir}/rc.d/init.d/bareos-dir \
    %{_sysconfdir}/rc.d/init.d/bareos-sd \
    %{script_dir}/disk-changer \
    %{script_dir}/mtx-changer \
    %{_sysconfdir}/%{name}/mtx-changer.conf \
%endif
%if 0%{?install_suse_fw} == 0
    %{_sysconfdir}/sysconfig/SuSEfirewall2.d/services/bareos-dir \
    %{_sysconfdir}/sysconfig/SuSEfirewall2.d/services/bareos-sd \
    %{_sysconfdir}/sysconfig/SuSEfirewall2.d/services/bareos-fd \
%endif
%if 0%{?systemd_support}
    %{_sysconfdir}/rc.d/init.d/bareos-dir \
    %{_sysconfdir}/rc.d/init.d/bareos-sd \
    %{_sysconfdir}/rc.d/init.d/bareos-fd \
    %{_sysconfdir}/init.d/bareos-dir \
    %{_sysconfdir}/init.d/bareos-sd \
    %{_sysconfdir}/init.d/bareos-fd \
%endif
%if !0%{?vmware}
    %{_sbindir}/bareos_vadp_dumper \
    %{_sbindir}/bareos_vadp_dumper_wrapper.sh \
    %{_sbindir}/vmware_cbt_tool.py \
%endif
    %{_sbindir}/btestls \
    %{script_dir}/bareos \
    %{script_dir}/bareos-ctl-dir \
    %{script_dir}/bareos-ctl-fd \
    %{script_dir}/bareos-ctl-funcs \
    %{script_dir}/bareos-ctl-sd \
    %{script_dir}/btraceback.dbx \
    %{script_dir}/btraceback.mdb \
    %{_docdir}/%{name}/INSTALL \
    %{_sbindir}/%{name}
do
rm -f "%{buildroot}/$F"
done

# remove links to libraries
# for i in #{buildroot}/#{_libdir}/libbareos*; do printf "$i: "; readelf -a $i | grep SONAME; done
find %{buildroot}/%{library_dir} -type l -name "libbareos*.so" -maxdepth 1 -exec rm {} \;
ls -la %{buildroot}/%{library_dir}

%if ! 0%{?python_plugins}
rm -f %{buildroot}/%{plugin_dir}/python-*.so
rm -f %{buildroot}/%{plugin_dir}/*.py*
rm -f %{buildroot}/%{_sysconfdir}/%{name}/bareos-dir.d/plugin-python-ldap.conf
%endif

%if ! 0%{?glusterfs}
rm -f %{buildroot}/%{script_dir}/bareos-glusterfind-wrapper
%endif

# remove man page if qt tray monitor is not built
%if !0%{?build_qt_monitor}
rm %{buildroot}%{_mandir}/man1/bareos-tray-monitor.*
%endif

# remove vmware plugin files when vmware is not built
%if  !0%{?vmware}
rm -f %{buildroot}%{plugin_dir}/bareos-fd-vmware.py*
%endif
# install systemd service files
%if 0%{?systemd_support}
install -d -m 755 %{buildroot}%{_unitdir}
install -m 644 %{CMAKE_BUILDDIR}/core/platforms/systemd/bareos-dir.service %{buildroot}%{_unitdir}
install -m 644 %{CMAKE_BUILDDIR}/core/platforms/systemd/bareos-fd.service %{buildroot}%{_unitdir}
install -m 644 %{CMAKE_BUILDDIR}/core/platforms/systemd/bareos-sd.service %{buildroot}%{_unitdir}
%if 0%{?suse_version}
ln -sf service %{buildroot}%{_sbindir}/rcbareos-dir
ln -sf service %{buildroot}%{_sbindir}/rcbareos-fd
ln -sf service %{buildroot}%{_sbindir}/rcbareos-sd
%endif
%endif

# Create the Readme files for the meta packages
[ -d %{buildroot}%{_docdir}/%{name}/ ]  || install -d -m 755 %{buildroot}%{_docdir}/%{name}
echo "This meta package emulates the former bareos-client package" > %{buildroot}%{_docdir}/%{name}/README.bareos-client
echo "This is a meta package to install a full bareos system" > %{buildroot}%{_docdir}/%{name}/README.bareos

# create dir for bareos-vmware-plugin-compat
mkdir -p %{?buildroot}/%{_libdir}/bareos/plugins/vmware_plugin

%files
%defattr(-, root, root)
%{_docdir}/%{name}/README.bareos

%if 0%{?webui}
%files webui
%defattr(-,root,root,-)
%doc webui/README.md webui/copyright
%doc webui/doc/README-TRANSLATION.md
%doc webui/tests/selenium
%{_datadir}/%{name}-webui/
%dir /etc/bareos-webui
%{configtemplatedir}/bareos-dir.d/console/admin.conf.example
# %%{configtemplatedir}/bareos-dir.d/profile/webui-admin.conf
%{configtemplatedir}/bareos-dir.d/profile/webui-limited.conf.example
# %%{configtemplatedir}/bareos-dir.d/profile/webui-readonly.conf
%config(noreplace) /etc/bareos-webui/directors.ini
%config(noreplace) /etc/bareos-webui/configuration.ini
%config(noreplace) %{_apache_conf_dir}/bareos-webui.conf
%endif

%files client
%defattr(-, root, root)
%dir %{_docdir}/%{name}
%{_docdir}/%{name}/README.bareos-client


%if 0%{?vmware}
# VMware Plugin BEGIN

%files -n bareos-vadp-dumper
%defattr(-,root,root)
%{_sbindir}/bareos_vadp_dumper*
%doc core/src/vmware/LICENSE.vadp

%files -n bareos-vmware-plugin
%defattr(-,root,root)
%dir %{_libdir}/bareos/
%{_sbindir}/vmware_cbt_tool.py
%{plugin_dir}/bareos-fd-vmware.py*
%doc core/src/vmware/LICENSE core/src/vmware/README.md

%files -n bareos-vmware-plugin-compat
%defattr(-,root,root)
%{_libdir}/bareos/plugins/vmware_plugin/

#VMware Plugin END
%endif

%files bconsole
# console package
%defattr(-, root, root)
%{_bindir}/bconsole
%{_sbindir}/bconsole
%{_mandir}/man1/bconsole.1.gz
%{configtemplatedir}/bconsole.conf


%if !0%{?client_only}

%files director
# dir package (bareos-dir)
%defattr(-, root, root)
%if 0%{?suse_version}
%if !0%{?systemd_support}
%{_sysconfdir}/init.d/bareos-dir
%endif
%{_sbindir}/rcbareos-dir
%if 0%{?install_suse_fw}
# use noreplace if user has adjusted its list of IP
%attr(0644, root, root) %config(noreplace) %{_fwdefdir}/bareos-dir
%endif
%else
%if !0%{?systemd_support}
%{_sysconfdir}/rc.d/init.d/bareos-dir
%endif
%endif
%{configtemplatedir}/bareos-dir.d/catalog/MyCatalog.conf
%{configtemplatedir}/bareos-dir.d/client/bareos-fd.conf
%{configtemplatedir}/bareos-dir.d/console/bareos-mon.conf
%{configtemplatedir}/bareos-dir.d/director/bareos-dir.conf
%{configtemplatedir}/bareos-dir.d/fileset/Catalog.conf
%{configtemplatedir}/bareos-dir.d/fileset/LinuxAll.conf
%{configtemplatedir}/bareos-dir.d/fileset/SelfTest.conf
"%{configtemplatedir}/bareos-dir.d/fileset/Windows All Drives.conf"
%{configtemplatedir}/bareos-dir.d/job/backup-bareos-fd.conf
%{configtemplatedir}/bareos-dir.d/job/BackupCatalog.conf
%{configtemplatedir}/bareos-dir.d/jobdefs/DefaultJob.conf
%{configtemplatedir}/bareos-dir.d/job/RestoreFiles.conf
%{configtemplatedir}/bareos-dir.d/messages/Daemon.conf
%{configtemplatedir}/bareos-dir.d/messages/Standard.conf
%{configtemplatedir}/bareos-dir.d/pool/Differential.conf
%{configtemplatedir}/bareos-dir.d/pool/Full.conf
%{configtemplatedir}/bareos-dir.d/pool/Incremental.conf
%{configtemplatedir}/bareos-dir.d/pool/Scratch.conf
%{configtemplatedir}/bareos-dir.d/profile/operator.conf
%{configtemplatedir}/bareos-dir.d/profile/webui-admin.conf
%{configtemplatedir}/bareos-dir.d/profile/webui-readonly.conf
%{configtemplatedir}/bareos-dir.d/schedule/WeeklyCycleAfterBackup.conf
%{configtemplatedir}/bareos-dir.d/schedule/WeeklyCycle.conf
%{configtemplatedir}/bareos-dir.d/storage/File.conf
%{configtemplatedir}/bareos-dir.d/storage/File.conf.example
%if 0%{?build_qt_monitor}
# TODO: shouldn't this belong into the tray-monitor package?
%dir %{configtemplatedir}/tray-monitor.d/director
%{configtemplatedir}/tray-monitor.d/director/Director-local.conf
%endif
%attr(2750, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir-export/
%config(noreplace) %{_sysconfdir}/logrotate.d/%{name}-dir
# we do not have any dir plugin but the python plugin
#%%{plugin_dir}/*-dir.so
%{script_dir}/delete_catalog_backup
%{script_dir}/make_catalog_backup
%{_sbindir}/bareos-dir
%dir %{_docdir}/%{name}
%{_mandir}/man8/bareos-dir.8.gz
%{_mandir}/man8/bareos.8.gz
%if 0%{?systemd_support}
%{_unitdir}/bareos-dir.service
%endif

# query.sql is not a config file,
# but can be personalized by end user.
# a rpmlint rule is add to filter the warning
%config(noreplace) %{script_dir}/query.sql

%files storage
# sd package (bareos-sd, bls, btape, bcopy, bextract)
%defattr(-, root, root)
%if 0%{?suse_version}
%if !0%{?systemd_support}
%{_sysconfdir}/init.d/bareos-sd
%endif
%{_sbindir}/rcbareos-sd
%if 0%{?install_suse_fw}
# use noreplace if user has adjusted its list of IP
%attr(0644, root, root) %config(noreplace) %{_fwdefdir}/bareos-sd
%endif
%else
%if !0%{?systemd_support}
%{_sysconfdir}/rc.d/init.d/bareos-sd
%endif
%endif
%{_sbindir}/bareos-sd
%{script_dir}/disk-changer
%{plugin_dir}/autoxflate-sd.so
%{backend_dir}/libbareossd-file*.so
%{_mandir}/man8/bareos-sd.8.gz
%if 0%{?systemd_support}
%{_unitdir}/bareos-sd.service
%endif
%attr(0775, %{storage_daemon_user}, %{daemon_group}) %dir /var/lib/%{name}/storage
%dir %{configtemplatedir}/bareos-sd.d
%dir %{configtemplatedir}/bareos-sd.d/autochanger
%dir %{configtemplatedir}/bareos-sd.d/device
%dir %{configtemplatedir}/bareos-sd.d/director
%dir %{configtemplatedir}/bareos-sd.d/ndmp
%dir %{configtemplatedir}/bareos-sd.d/messages
%dir %{configtemplatedir}/bareos-sd.d/storage
%{configtemplatedir}/bareos-sd.d/autochanger/FileStorage.conf.example
%{configtemplatedir}/bareos-sd.d/device/FileStorage.conf.example
%{configtemplatedir}/bareos-sd.d/device/FileStorage.conf
%{configtemplatedir}/bareos-sd.d/director/bareos-dir.conf
%{configtemplatedir}/bareos-sd.d/director/bareos-mon.conf
%{configtemplatedir}/bareos-sd.d/messages/Standard.conf
%{configtemplatedir}/bareos-sd.d/storage/bareos-sd.conf
%if 0%{?build_qt_monitor}
%dir %{configtemplatedir}/tray-monitor.d/storage
%{configtemplatedir}/tray-monitor.d/storage/StorageDaemon-local.conf
%endif

%files storage-tape
# tape specific files
%defattr(-, root, root)
%{backend_dir}/libbareossd-tape*.so
%{script_dir}/mtx-changer
%config(noreplace) %{_sysconfdir}/%{name}/mtx-changer.conf
%config(noreplace) %{_sysconfdir}/logrotate.d/%{name}-mtx
%{_mandir}/man8/bscrypto.8.gz
%{_mandir}/man8/btape.8.gz
%{_sbindir}/bscrypto
%{_sbindir}/btape
%{plugin_dir}/scsicrypto-sd.so
%{plugin_dir}/scsitapealert-sd.so
%{configtemplatedir}/bareos-dir.d/storage/Tape.conf.example
%{configtemplatedir}/bareos-sd.d/autochanger/autochanger-0.conf.example
%{configtemplatedir}/bareos-sd.d/device/tapedrive-0.conf.example

%files storage-fifo
%defattr(-, root, root)
%{backend_dir}/libbareossd-fifo*.so
%{configtemplatedir}/bareos-dir.d/storage/NULL.conf.example
%{configtemplatedir}/bareos-sd.d/device/NULL.conf.example

%files storage-dedupable
%defattr(-, root, root)
%{backend_dir}/libbareossd-dedupable*.so
%{configtemplatedir}/bareos-dir.d/storage/Dedupable.conf.example
%{configtemplatedir}/bareos-sd.d/device/Dedupable.conf.example

%if 0%{?droplet}
%files storage-droplet
%defattr(-, root, root)
%{backend_dir}/libbareossd-droplet*.so
%{configtemplatedir}/bareos-dir.d/storage/S3_Object.conf.example
%{configtemplatedir}/bareos-sd.d/device/S3_ObjectStorage.conf.example
%dir %{configtemplatedir}/bareos-sd.d/device/droplet/
%{configtemplatedir}/bareos-sd.d/device/droplet/*.example
%endif

%files storage-dplcompat
%defattr(-, root, root)
%{backend_dir}/libbareossd-dplcompat*.so
%{script_dir}/s3cmd-wrapper.sh
%{configtemplatedir}/bareos-dir.d/storage/dplcompat.conf.example
%{configtemplatedir}/bareos-sd.d/device/dplcompat.conf.example

%if 0%{?glusterfs}
%files storage-glusterfs
%defattr(-, root, root)
%{backend_dir}/libbareossd-gfapi*.so
%{configtemplatedir}/bareos-dir.d/storage/Gluster.conf.example
%{configtemplatedir}/bareos-sd.d/device/GlusterStorage.conf.example
%endif

# not client_only
%endif

%files filedaemon
# fd package (bareos-fd, plugins)
%defattr(-, root, root)
%if 0%{?suse_version}
%if !0%{?systemd_support}
%{_sysconfdir}/init.d/bareos-fd
%endif
%{_sbindir}/rcbareos-fd
%if 0%{?install_suse_fw}
# use noreplace if user has adjusted its list of IP
%attr(0644, root, root) %config(noreplace) %{_fwdefdir}/bareos-fd
%endif
%else
%if !0%{?systemd_support}
%{_sysconfdir}/rc.d/init.d/bareos-fd
%endif
%endif
%{_sbindir}/bareos-fd
%{plugin_dir}/bpipe-fd.so
%{_mandir}/man8/bareos-fd.8.gz
# tray monitor
%if 0%{?systemd_support}
%{_unitdir}/bareos-fd.service
%endif
%dir %{configtemplatedir}/bareos-fd.d/
%dir %{configtemplatedir}/bareos-fd.d/client
%dir %{configtemplatedir}/bareos-fd.d/director
%dir %{configtemplatedir}/bareos-fd.d/messages
%{configtemplatedir}/bareos-fd.d/client/myself.conf
%{configtemplatedir}/bareos-fd.d/director/bareos-dir.conf
%{configtemplatedir}/bareos-fd.d/director/bareos-mon.conf
%{configtemplatedir}/bareos-fd.d/messages/Standard.conf
%if 0%{?build_qt_monitor}
%dir %{configtemplatedir}/tray-monitor.d/client
%{configtemplatedir}/tray-monitor.d/client/FileDaemon-local.conf
%endif


%files common
# common shared libraries (without db)
%defattr(-, root, root)
%attr(2755, root, %{daemon_group})           %dir %{_sysconfdir}/%{name}
%if !0%{?client_only}
# these directories belong to bareos-common,
# as other packages may contain configurations for the director.
%dir %{configtemplatedir}/bareos-dir.d
%dir %{configtemplatedir}/bareos-dir.d/catalog
%dir %{configtemplatedir}/bareos-dir.d/client
%dir %{configtemplatedir}/bareos-dir.d/console
%dir %{configtemplatedir}/bareos-dir.d/counter
%dir %{configtemplatedir}/bareos-dir.d/director
%dir %{configtemplatedir}/bareos-dir.d/fileset
%dir %{configtemplatedir}/bareos-dir.d/job
%dir %{configtemplatedir}/bareos-dir.d/jobdefs
%dir %{configtemplatedir}/bareos-dir.d/messages
%dir %{configtemplatedir}/bareos-dir.d/pool
%dir %{configtemplatedir}/bareos-dir.d/profile
%dir %{configtemplatedir}/bareos-dir.d/schedule
%dir %{configtemplatedir}/bareos-dir.d/storage
%dir %{configtemplatedir}/bareos-dir.d/user
# tray monitor configurate is installed by the target daemons
%if 0%{?build_qt_monitor}
%dir %{configtemplatedir}/tray-monitor.d
%endif
%endif
%dir %{backend_dir}
%{library_dir}/libbareosfastlz.so*
%{library_dir}/libbareos.so*
%{library_dir}/libbareosfind.so*
%{library_dir}/libbareoslmdb.so*
%if !0%{?client_only}
%{library_dir}/libbareosndmp.so*
%{library_dir}/libbareossd.so*
%endif
# generic stuff needed from multiple bareos packages
%dir /usr/lib/%{name}/
%dir %{script_dir}
%{script_dir}/bareos-config
%{script_dir}/bareos-config-lib.sh
%{script_dir}/btraceback.gdb
%if "%{_libdir}" != "/usr/lib/"
%dir %{_libdir}/%{name}/
%endif
%dir %{plugin_dir}
%if !0%{?client_only}
%{_bindir}/bsmtp
%{_sbindir}/bsmtp
%endif
%{_sbindir}/btraceback
%if !0%{?client_only}
%{_mandir}/man1/bsmtp.1.gz
%endif
%{_mandir}/man8/btraceback.8.gz
%attr(0770, %{daemon_user}, %{daemon_group}) %dir %{working_dir}
%attr(0775, %{daemon_user}, %{daemon_group}) %dir /var/log/%{name}
%license AGPL-3.0.txt LICENSE.txt
%doc core/README.*
#TODO: cmake does not create build directory
#doc build/

%if !0%{?client_only}

%files database-common
# catalog independent files
%defattr(-, root, root)
%{library_dir}/libbareossql*.so.*
%dir %{script_dir}/ddl
%dir %{script_dir}/ddl/creates
%dir %{script_dir}/ddl/drops
%dir %{script_dir}/ddl/grants
%dir %{script_dir}/ddl/updates
%{script_dir}/create_bareos_database
%{script_dir}/drop_bareos_database
%{script_dir}/drop_bareos_tables
%{script_dir}/grant_bareos_privileges
%{script_dir}/make_bareos_tables
%{script_dir}/update_bareos_tables
%{script_dir}/ddl/versions.map

%files database-postgresql
# postgresql catalog files
%defattr(-, root, root)
%{script_dir}/ddl/*/postgresql*.sql

%files database-tools
# dbtools with link to db libs (dbcheck, bscan)
%defattr(-, root, root)
%{_sbindir}/bareos-dbcheck
%{_sbindir}/bscan
%{_mandir}/man8/bareos-dbcheck.8.gz
%{_mandir}/man8/bscan.8.gz

%files tools
# tools without link to db libs (bwild, bregex)
%defattr(-, root, root)
%{_bindir}/bregex
%{_bindir}/bwild
%{_sbindir}/bcopy
%{_sbindir}/bextract
%{_sbindir}/bls
%{_sbindir}/testfind
%{_sbindir}/bregex
%{_sbindir}/bwild
%{_sbindir}/bpluginfo
%{_sbindir}/bdedupestimate
%{_mandir}/man1/bwild.1.gz
%{_mandir}/man1/bregex.1.gz
%{_mandir}/man8/bcopy.8.gz
%{_mandir}/man8/bextract.8.gz
%{_mandir}/man8/bls.8.gz
%{_mandir}/man8/bpluginfo.8.gz

%if 0%{?build_qt_monitor}
%files traymonitor
%defattr(-,root, root)
%config %{_sysconfdir}/xdg/autostart/bareos-tray-monitor.desktop
%{_bindir}/bareos-tray-monitor
%{_mandir}/man1/bareos-tray-monitor.1.gz
/usr/share/applications/bareos-tray-monitor.desktop
/usr/share/pixmaps/bareos-tray-monitor.png
%dir %{configtemplatedir}/tray-monitor.d/monitor
%{configtemplatedir}/tray-monitor.d/monitor/bareos-mon.conf
%endif

# client_only
%endif

%if 0%{?python_plugins}
%if 0%{?enable_grpc}
%files filedaemon-grpc-python3-plugin
%defattr(-, root, root)
%{plugin_dir}/grpc-fd.so
%{plugin_dir}/grpc/bareos-grpc-fd-plugin-bridge
%{plugin_dir}/grpc/grpc-test-module
%endif

%files filedaemon-python3-plugin
%defattr(-, root, root)
%{plugin_dir}/python3-fd.so
%{python3_sitearch}/bareosfd*.so

%files filedaemon-python-plugins-common
%{plugin_dir}/bareos-fd-local-fileset.py*
%{plugin_dir}/BareosFdPluginBaseclass.py*
%{plugin_dir}/BareosFdPluginLocalFilesBaseclass.py*
%{plugin_dir}/BareosFdWrapper.py*
%{script_dir}/bareos_encode_string.py

%files filedaemon-ldap-python-plugin
%defattr(-, root, root)
%{plugin_dir}/bareos-fd-ldap.py*
%{configtemplatedir}/bareos-dir.d/fileset/plugin-ldap.conf.example
%{configtemplatedir}/bareos-dir.d/job/backup-ldap.conf.example
%{configtemplatedir}/bareos-dir.d/job/restore-ldap.conf.example

%files filedaemon-libcloud-python-plugin
%defattr(-, root, root)
%{plugin_dir}/bareos-fd-libcloud.py*
%{plugin_dir}/BareosFdPluginLibcloud.py*
%{plugin_dir}/BareosLibcloudApi.py*
%dir %{plugin_dir}/bareos_libcloud_api
%{plugin_dir}/bareos_libcloud_api/*

%files filedaemon-postgresql-python-plugin
%defattr(-, root, root)
%{plugin_dir}/bareos-fd-postgresql.py*

%files filedaemon-percona-xtrabackup-python-plugin
%defattr(-, root, root)
%{plugin_dir}/bareos-fd-percona-xtrabackup.py*

%files filedaemon-mariabackup-python-plugin
%defattr(-, root, root)
%{plugin_dir}/bareos-fd-mariabackup.py*


%files director-python3-plugin
%defattr(-, root, root)
%{plugin_dir}/python3-dir.so
%{python3_sitearch}/bareosdir*.so

%files director-python-plugins-common
%{plugin_dir}/BareosDirPluginBaseclass.py*
%{plugin_dir}/bareos-dir-class-plugin.py*
%{plugin_dir}/BareosDirWrapper.py*

%files storage-python3-plugin
%defattr(-, root, root)
%{plugin_dir}/python3-sd.so
%{python3_sitearch}/bareossd*.so

%files storage-python-plugins-common
%{plugin_dir}/BareosSdPluginBaseclass.py*
%{plugin_dir}/BareosSdWrapper.py*
%{plugin_dir}/bareos-sd-class-plugin.py*

# python_plugins
%endif

%if 0%{?glusterfs}
%files filedaemon-glusterfs-plugin
%{script_dir}/bareos-glusterfind-wrapper
%{plugin_dir}/gfapi-fd.so
%{configtemplatedir}/bareos-dir.d/fileset/plugin-gfapi.conf.example
%{configtemplatedir}/bareos-dir.d/job/BackupGFAPI.conf.example
%{configtemplatedir}/bareos-dir.d/job/RestoreGFAPI.conf.example
%endif

%if 0%{?contrib}

%files       contrib-tools
%defattr(-, root, root)
%{_bindir}/chunk_check.py
%{_bindir}/bareos-triggerjob.py
%{_bindir}/bsmc
%attr(0640, %{daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bsmc.conf
%{script_dir}/reschedule_job_as_full.sh


%files       contrib-filedaemon-python-plugins
%defattr(-, root, root)
%{plugin_dir}/bareos_mysql_dump
%{plugin_dir}/mariadb-dump
%{plugin_dir}/bareos_tasks
%{plugin_dir}/openvz7


%files       contrib-director-python-plugins
%defattr(-, root, root)
%{plugin_dir}/BareosDirPluginNscaSender.py*
%{plugin_dir}/bareos-dir-nsca-sender.py*

# endif: contrib
%endif


#
# Define some macros for updating the system settings.
#
%if 0%{?suse_version}

%if 0%{?systemd_support}
%define insserv_cleanup() (/bin/true; %nil)
%else
%if 0%{!?add_service_start:1}
%define add_service_start() \
SERVICE=%1 \
#service_add $1 \
%fillup_and_insserv $SERVICE \
%nil
%endif
%endif

%else
# non suse, systemd

%define insserv_cleanup() \
/bin/true \
%nil

%if 0%{?systemd_support}
# non suse, systemd

%define add_service_start() \
/bin/systemctl daemon-reload >/dev/null 2>&1 || true \
/bin/systemctl enable %1.service >/dev/null 2>&1 || true \
%nil

%define stop_on_removal() \
test -n "$FIRST_ARG" || FIRST_ARG=$1 \
if test "$FIRST_ARG" = "0" ; then \
  /bin/systemctl stop %1.service > /dev/null 2>&1 || true \
fi \
%nil

%define restart_on_update() \
test -n "$FIRST_ARG" || FIRST_ARG=$1 \
if test "$FIRST_ARG" -ge 1 ; then \
  /bin/systemctl try-restart %1.service >/dev/null 2>&1 || true \
fi \
%nil

%else
# non suse, init.d

%define add_service_start() \
/sbin/chkconfig --add %1 \
%nil

%define stop_on_removal() \
test -n "$FIRST_ARG" || FIRST_ARG=$1 \
if test "$FIRST_ARG" = "0" ; then \
  /sbin/service %1 stop >/dev/null 2>&1 || \
  /sbin/chkconfig --del %1 || true \
fi \
%nil

%define restart_on_update() \
test -n "$FIRST_ARG" || FIRST_ARG=$1 \
if test "$FIRST_ARG" -ge 1 ; then \
  /sbin/service %1 condrestart >/dev/null 2>&1 || true \
fi \
%nil

%endif

%endif

%define create_group() \
getent group %1 > /dev/null || groupadd -r %1 \
%nil

# shell: use /bin/false, because nologin has different paths on different distributions
%define create_user() \
getent passwd %1 > /dev/null || useradd -r --comment "%1" --home %{working_dir} -g %{daemon_group} --shell /bin/false %1 \
%nil

# With the introduction of config subdirectories (bareos-16.2)
# some config files have been renamed (or even splitted into multiple files).
# However, bareos is still able to work with the old config files,
# but rpm renames them to *.rpmsave.
# To keep the bareos working after updating to bareos-16.2,
# we implement a workaround:
#   * post: if the old config exists, make a copy of it.
#   * (rpm exchanges files on disk)
#   * posttrans:
#       if the old config file don't exists but we have created a backup before,
#       restore the old config file.
#       Remove our backup, if it exists.
# This update helper should be removed with bareos-17.

%define post_backup_file() \
FILE=%* \
if [ -f "${FILE}" ]; then \
      cp -a "${FILE}" "${FILE}.rpmupdate.%{version}.keep"; \
fi; \
%nil

%define posttrans_restore_file() \
FILE=%* \
if [ ! -e "${FILE}" -a -e "${FILE}.rpmupdate.%{version}.keep" ]; then \
   mv "${FILE}.rpmupdate.%{version}.keep" "${FILE}"; \
fi; \
if [ -e "${FILE}.rpmupdate.%{version}.keep" ]; then \
   rm "${FILE}.rpmupdate.%{version}.keep"; \
fi; \
%nil

%define post_scsicrypto() \
if [ -f "%{_sysconfdir}/%{name}/.enable-cap_sys_rawio" ]; then \
   %{script_dir}/bareos-config set_scsicrypto_capabilities; \
fi\
%nil

%if 0%{?webui}
%post webui
# update from bareos < 25
%post_backup_file "/etc/bareos/bareos-dir.d/profile/webui-admin.conf"
%post_backup_file "/etc/bareos/bareos-dir.d/profile/webui-readonly.conf"

%if 0%{?suse_version}
a2enmod rewrite &> /dev/null || true
a2enmod proxy &> /dev/null || true
a2enmod proxy_fcgi &> /dev/null || true
a2enmod fcgid &> /dev/null || true
%endif
%endif

%posttrans webui
# update from bareos < 25
%posttrans_restore_file "/etc/bareos/bareos-dir.d/profile/webui-admin.conf"
%posttrans_restore_file "/etc/bareos/bareos-dir.d/profile/webui-readonly.conf"

%post director
# update from bareos < 16.2
%post_backup_file /etc/%{name}/bareos-dir.conf
# update from bareos < 25
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/catalog/MyCatalog.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/client/bareos-fd.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/console/bareos-mon.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/director/bareos-dir.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/fileset/Catalog.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/fileset/LinuxAll.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/fileset/SelfTest.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/fileset/Windows All Drives.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/job/backup-bareos-fd.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/job/BackupCatalog.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/jobdefs/DefaultJob.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/job/RestoreFiles.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/messages/Daemon.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/messages/Standard.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/pool/Differential.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/pool/Full.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/pool/Incremental.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/pool/Scratch.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/profile/operator.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/schedule/WeeklyCycleAfterBackup.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/schedule/WeeklyCycle.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-dir.d/storage/File.conf"
%post_backup_file "%{_sysconfdir}/%{name}/tray-monitor.d/director/Director-local.conf"

%{script_dir}/bareos-config deploy_config_new "bareos-dir" || true
%if 0%{?suse_version} >= 1210
%service_add_post bareos-dir.service
/bin/systemctl enable bareos-dir.service >/dev/null 2>&1 || true
%else
%add_service_start bareos-dir
%endif

%posttrans director
# update from bareos < 16.2
%posttrans_restore_file /etc/%{name}/bareos-dir.conf
# update from bareos < 25
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/catalog/MyCatalog.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/client/bareos-fd.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/console/bareos-mon.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/director/bareos-dir.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/fileset/Catalog.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/fileset/LinuxAll.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/fileset/SelfTest.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/fileset/Windows All Drives.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/job/backup-bareos-fd.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/job/BackupCatalog.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/jobdefs/DefaultJob.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/job/RestoreFiles.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/messages/Daemon.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/messages/Standard.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/pool/Differential.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/pool/Full.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/pool/Incremental.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/pool/Scratch.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/profile/operator.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/schedule/WeeklyCycleAfterBackup.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/schedule/WeeklyCycle.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-dir.d/storage/File.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/tray-monitor.d/director/Director-local.conf"

%post tools
%post_scsicrypto

%post storage
# update from bareos < 16.2
%post_backup_file /etc/%{name}/bareos-sd.conf
# update from bareos < 25
%post_backup_file "%{_sysconfdir}/%{name}/bareos-sd.d/device/FileStorage.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-sd.d/director/bareos-dir.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-sd.d/director/bareos-mon.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-sd.d/messages/Standard.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-sd.d/storage/bareos-sd.conf"
%post_backup_file "%{_sysconfdir}/%{name}/tray-monitor.d/storage/StorageDaemon-local.conf"

%{script_dir}/bareos-config deploy_config_new "bareos-sd" || true
# pre script has already generated the storage daemon user,
# but here we add the user to additional groups
%{script_dir}/bareos-config setup_sd_user
%post_scsicrypto
%if 0%{?suse_version} >= 1210
%service_add_post bareos-sd.service
/bin/systemctl enable bareos-sd.service >/dev/null 2>&1 || true
%else
%add_service_start bareos-sd
%endif

%posttrans storage
# update from bareos < 16.2
%posttrans_restore_file /etc/%{name}/bareos-sd.conf
# update from bareos < 25
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-sd.d/device/FileStorage.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-sd.d/director/bareos-dir.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-sd.d/director/bareos-mon.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-sd.d/messages/Standard.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-sd.d/storage/bareos-sd.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/tray-monitor.d/storage/StorageDaemon-local.conf"

%post storage-fifo
%post_backup_file /etc/%{name}/bareos-sd.d/device-fifo.conf

%posttrans storage-fifo
%posttrans_restore_file /etc/%{name}/bareos-sd.d/device-fifo.conf


%post storage-tape
%post_backup_file /etc/%{name}/bareos-sd.d/device-tape-with-autoloader.conf
%post_scsicrypto

%posttrans storage-tape
%posttrans_restore_file /etc/%{name}/bareos-sd.d/device-tape-with-autoloader.conf
%post_scsicrypto

%post filedaemon
# update from bareos < 16.2
%post_backup_file /etc/%{name}/bareos-fd.conf
# update from bareos < 25
%post_backup_file "%{_sysconfdir}/%{name}/bareos-fd.d/client/myself.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-fd.d/director/bareos-dir.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-fd.d/director/bareos-mon.conf"
%post_backup_file "%{_sysconfdir}/%{name}/bareos-fd.d/messages/Standard.conf"
%post_backup_file "%{_sysconfdir}/%{name}/tray-monitor.d/client/FileDaemon-local.conf"
%{script_dir}/bareos-config deploy_config_new "bareos-fd" || true
%if 0%{?suse_version} >= 1210
%service_add_post bareos-fd.service
/bin/systemctl enable bareos-fd.service >/dev/null 2>&1 || true
%else
%add_service_start bareos-fd
%endif

%posttrans filedaemon
# update from bareos < 16.2
%posttrans_restore_file /etc/%{name}/bareos-fd.conf
# update from bareos < 25
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-fd.d/client/myself.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-fd.d/director/bareos-dir.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-fd.d/director/bareos-mon.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-fd.d/messages/Standard.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/tray-monitor.d/client/FileDaemon-local.conf"

%if 0%{?python_plugins}

%post filedaemon-ldap-python-plugin
%post_backup_file /etc/%{name}/bareos-dir.d/plugin-python-ldap.conf

%posttrans filedaemon-ldap-python-plugin
%posttrans_restore_file /etc/%{name}/bareos-dir.d/plugin-python-ldap.conf

%endif


%post bconsole
# update from bareos < 25
%post_backup_file "%{_sysconfdir}/%{name}/bconsole.conf"

%{script_dir}/bareos-config deploy_config_new "bconsole" || true


%posttrans bconsole
# update from bareos < 25
%posttrans_restore_file "%{_sysconfdir}/%{name}/bconsole.conf"

%post common
/sbin/ldconfig

%postun common
/sbin/ldconfig

%post database-common
/sbin/ldconfig

%postun database-common
/sbin/ldconfig

%post database-postgresql
/sbin/ldconfig

%post database-tools
%post_scsicrypto

%postun database-postgresql
/sbin/ldconfig


%if 0%{?build_qt_monitor}

%post traymonitor
# update from bareos < 16.2
%post_backup_file /etc/%{name}/tray-monitor.conf
# update from bareos < 25
%post_backup_file "%{_sysconfdir}/%{name}/tray-monitor.d/monitor/bareos-mon.conf"

%{script_dir}/bareos-config deploy_config_new "tray-monitor" || true


%posttrans traymonitor
# update from bareos < 16.2
%posttrans_restore_file /etc/%{name}/tray-monitor.conf
# update from bareos < 25
%posttrans_restore_file "%{_sysconfdir}/%{name}/tray-monitor.d/monitor/bareos-mon.conf"

%endif


%pre director
%create_group %{daemon_group}
%create_user  %{director_daemon_user}
exit 0

%pre storage
%create_group %{daemon_group}
%create_user  %{storage_daemon_user}
exit 0

%pre filedaemon
%create_group %{daemon_group}
%create_user  %{storage_daemon_user}
exit 0

%pre common
%create_group %{daemon_group}
%create_user  %{daemon_user}
exit 0

%preun director
%if 0%{?suse_version} >= 1210
%service_del_preun bareos-dir.service
%else
%stop_on_removal bareos-dir
%endif

%preun storage
%if 0%{?suse_version} >= 1210
%service_del_preun bareos-sd.service
%else
%stop_on_removal bareos-sd
%endif

%preun filedaemon
%if 0%{?suse_version} >= 1210
%service_del_preun bareos-fd.service
%else
%stop_on_removal bareos-fd
%endif

%postun director
# to prevent aborting jobs, no restart on update
%insserv_cleanup

%postun storage
# to prevent aborting jobs, no restart on update
%insserv_cleanup

%postun filedaemon
%if 0%{?suse_version} >= 1210
%service_del_postun bareos-fd.service
%else
%restart_on_update bareos-fd
%endif
%insserv_cleanup

%changelog
