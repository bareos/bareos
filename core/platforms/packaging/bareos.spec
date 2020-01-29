#
# spec file for package bareos
# Copyright (c) 2011-2012 Bruno Friedmann (Ioda-Net) and Philipp Storz (dass IT)
#               2013-2018 Bareos GmbH & Co KG
#

Name: 		bareos
Version: 	0.0.0
Release: 	0
Group: 		Productivity/Archiving/Backup
License: 	AGPL-3.0
BuildRoot: 	%{_tmppath}/%{name}-root
URL: 		http://www.bareos.org/
Vendor: 	The Bareos Team
#Packager: 	{_packager}

%define library_dir    %{_libdir}/%{name}
%define backend_dir    %{_libdir}/%{name}/backends
%define plugin_dir     %{_libdir}/%{name}/plugins
%define script_dir     /usr/lib/%{name}/scripts
%define working_dir    /var/lib/%{name}
%define pid_dir        /var/lib/%{name}
%define bsr_dir        /var/lib/%{name}
# TODO: use /run ?
%define _subsysdir     /var/lock

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
%define build_sqlite3 1
%define glusterfs 0
%define droplet 0
%define have_git 1
%define ceph 0
%define install_suse_fw 0
%define systemd_support 0
%define python_plugins 1

# cmake build directory
%define CMAKE_BUILDDIR       cmake-build

# fedora 28 deprecated libwrap
%if 0%{?fedora} >= 28 || 0%{?rhel} > 7
%define use_libwrap 0
%else
%define use_libwrap 1
%endif

# rhel/centos 6 must not be built with libtirpc installed
%if 0%{?rhel} == 6
BuildConflicts: libtirpc-devel
%endif

# fedora 28: rpc was removed from libc
%if 0%{?fedora} >= 28 || 0%{?rhel} > 7
BuildRequires: rpcgen
BuildRequires: libtirpc-devel
%endif

#
# SUSE (openSUSE, SLES) specific settigs
#
%if 0%{?sles_version} == 10
%define build_qt_monitor 0
%define build_sqlite3 0
%define have_git 0
%define python_plugins 0
%endif

%if 0%{?suse_version} > 1010
%define install_suse_fw 1
%define _fwdefdir   %{_sysconfdir}/sysconfig/SuSEfirewall2.d/services
%endif


%if 0%{?suse_version} > 1140
%define systemd_support 1
%endif

# SLE 12
# SLE_15: droplet is not available due to SSL build problems
%if 0%{?sle_version} >= 120000 && 0%{?sle_version} < 150000
%define droplet 1
%endif

%if 0%{?sle_version} >= 120000
%define ceph 1
%endif

#
# RedHat (CentOS, Fedora, RHEL) specific settings
#
%if 0%{?rhel_version} > 0 && 0%{?rhel_version} < 500
%define RHEL4 1
%define client_only 1
%define build_qt_monitor 0
%define build_sqlite3 0
%define have_git 0
%define python_plugins 0
%endif

# centos/rhel 5: segfault when building qt monitor
%if 0%{?centos_version} == 505 || 0%{?rhel_version} == 505
%define build_qt_monitor 0
%define have_git 0
%define python_plugins 0
%endif

%if 0%{?fedora_version} >= 20
%define glusterfs 1
%define systemd_support 1
%endif

%if 0%{?rhel_version} >= 700 || 0%{?centos_version} >= 700
%define glusterfs 1
%define droplet 1
%define systemd_support 1
%endif

%if 0%{?rhel_version} == 800
%define droplet 0
%endif

%if 0%{?rhel_version} >= 700 && !0%{?centos_version}
%define ceph 1
%endif

%if 0%{?centos} >= 8
%define ceph 1
%endif

# use Developer Toolset 7 compiler as standard is too old
%if 0%{?centos_version} == 600 || 0%{?rhel_version} == 600
BuildRequires: devtoolset-7-gcc
BuildRequires: devtoolset-7-gcc-c++
%endif

%if 0%{?systemd_support}
BuildRequires: systemd
# see https://en.opensuse.org/openSUSE:Systemd_packaging_guidelines
%if 0%{?suse_version} >= 1210
BuildRequires: systemd-rpm-macros
%endif
%{?systemd_requires}
%endif

%if 0%{?droplet}
BuildRequires: libdroplet-devel
%endif

%if 0%{?glusterfs}
BuildRequires: glusterfs-devel glusterfs-api-devel
%endif

%if 0%{?ceph}
  %if 0%{?sle_version} >= 120200
BuildRequires: libcephfs-devel
BuildRequires: librados-devel
  %else
# the rhel macro is set in docker, but not in obs
    %if 0%{?rhel} == 7
BuildRequires: librados2-devel
BuildRequires: libcephfs1-devel
    %else
      %if 0%{?rhel} == 8
BuildRequires: librados-devel
BuildRequires: libradosstriper-devel
BuildRequires: libcephfs-devel
      %else
BuildRequires: ceph-devel
      %endif
    %endif
  %endif
%endif

%if 0%{?have_git}
BuildRequires: git-core
%endif

Source0: %{name}-%{version}.tar.bz2

BuildRequires: pam-devel

BuildRequires: cmake
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: glibc
BuildRequires: glibc-devel
BuildRequires: ncurses-devel
BuildRequires: perl
BuildRequires: readline-devel
BuildRequires: libstdc++-devel
BuildRequires: zlib-devel
BuildRequires: openssl-devel
BuildRequires: libacl-devel
BuildRequires: pkgconfig
BuildRequires: lzo-devel
BuildRequires: logrotate
%if 0%{?build_sqlite3}
%if 0%{?suse_version}
BuildRequires: sqlite3-devel
%else
BuildRequires: sqlite-devel
%endif
%endif
BuildRequires: mysql-devel
BuildRequires: postgresql-devel
BuildRequires: openssl
BuildRequires: libcap-devel
BuildRequires: mtx

%if 0%{?build_qt_monitor}
%if 0%{?suse_version}
BuildRequires: libqt5-qtbase-devel
%else

%if 0%{?centos_version} > 700 || 0%{?rhel_version} > 700 || 0%{?fedora} >= 29
BuildRequires: qt5-qtbase-devel
%else
BuildRequires: qt-devel
%endif

%endif
%endif


%if 0%{?python_plugins}
%if 0%{?centos_version} >= 800 || 0%{?rhel_version} >= 800 || 0%{?fedora} >= 31
BuildRequires: python2-devel >= 2.6
%else
BuildRequires: python-devel >= 2.6
%endif
%endif

%if 0%{?suse_version}

# suse_version:
#   1315: SLE_12
#   1110: SLE_11
#   1010: SLE_10

BuildRequires: distribution-release
BuildRequires: pwdutils
BuildRequires: tcpd-devel
BuildRequires: termcap
BuildRequires: update-desktop-files

%if 0%{?suse_version} > 1010
# link identical files
BuildRequires: fdupes
BuildRequires: libjansson-devel
BuildRequires: lsb-release
%endif

%else
# non suse

BuildRequires: libtermcap-devel
BuildRequires: passwd

%if %{use_libwrap}
BuildRequires: tcp_wrappers
%endif

# Some magic to be able to determine what platform we are running on.
%if 0%{?rhel_version} || 0%{?centos_version} || 0%{?fedora_version}

BuildRequires: redhat-lsb

# older versions require additional release packages
%if 0%{?rhel_version}   && 0%{?rhel_version} <= 600
BuildRequires: redhat-release
%endif

%if 0%{?centos_version} && 0%{?centos_version} <= 600
BuildRequires: redhat-release
%endif

%if 0%{?fedora_version}
BuildRequires: fedora-release
%endif

%if 0%{?rhel_version} >= 600 || 0%{?centos_version} >= 600 || 0%{?fedora_version} >= 14
BuildRequires: jansson-devel
%if %{use_libwrap}
BuildRequires: tcp_wrappers-devel
%endif
%endif
%else
# non suse, non redhat: eg. mandriva.

BuildRequires: lsb-release

%endif

%endif


Summary:    Backup Archiving REcovery Open Sourced - metapackage
Requires:   %{name}-director = %{version}
Requires:   %{name}-storage = %{version}
Requires:   %{name}-client = %{version}

%if 0%{?RHEL4}
%define dscr Bareos - Backup Archiving Recovery Open Sourced.
%else
%define dscr Bareos - Backup Archiving Recovery Open Sourced. \
Bareos is a set of computer programs that permit you (or the system \
administrator) to manage backup, recovery, and verification of computer \
data across a network of computers of different kinds. In technical terms, \
it is a network client/server based backup program. Bareos is relatively \
easy to use and efficient, while offering many advanced storage management \
features that make it easy to find and recover lost or damaged files. \
Bareos source code has been released under the AGPL version 3 license.
%endif

%description
%{dscr}

%if 0%{?opensuse_version} || 0%{?sle_version}
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
Requires(pre): pwdutils
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
Requires(pre): pwdutils
Recommends: bareos-tools
%else
Requires(pre): shadow-utils
# Recommends would be enough, however only supported by Fedora >= 24.
Requires: bareos-tools
%endif

%if 0%{?droplet}
%package    storage-droplet
Summary:    Object Storage support (through libdroplet) for the Bareos Storage daemon
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common  = %{version}
Requires:   %{name}-storage = %{version}
%endif

%if 0%{?glusterfs}
%package    storage-glusterfs
Summary:    GlusterFS support for the Bareos Storage daemon
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common  = %{version}
Requires:   %{name}-storage = %{version}
Requires:   glusterfs
%endif

%if 0%{?ceph}
%package    storage-ceph
Summary:    CEPH support for the Bareos Storage daemon
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common  = %{version}
Requires:   %{name}-storage = %{version}
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
Requires(pre): pwdutils
%else
Requires(pre): shadow-utils
%endif

%package    common
Summary:    Common files, required by multiple Bareos packages
Group:      Productivity/Archiving/Backup
Requires:   openssl
%if 0%{?suse_version}
Requires(pre): pwdutils
%else
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

%package    database-mysql
Summary:    Libs & tools for mysql catalog
Group:      Productivity/Archiving/Backup
Requires:   %{name}-database-common = %{version}
Provides:   %{name}-catalog-mysql
Provides:   %{name}-database-backend

%if 0%{?build_sqlite3}
%package    database-sqlite3
Summary:    Libs & tools for sqlite3 catalog
Group:      Productivity/Archiving/Backup
%if 0%{?suse_version}
Requires:   sqlite3
%endif
%if 0%{?fedora_version}
Requires:   sqlite
%endif
Requires:   %{name}-database-common = %{version}
Provides:   %{name}-catalog-sqlite3
Provides:   %{name}-database-backend
%endif

%package    database-tools
Summary:    Bareos CLI tools with database dependencies (bareos-dbcheck, bareos-dbcopy, bscan)
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common = %{version}
Requires:   %{name}-database-common = %{version}
Provides:   %{name}-dbtools

%package    tools
Summary:    Bareos CLI tools (bcopy, bextract, bls, bregex, bwild)
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

%package    devel
Summary:    Devel headers
Group:      Development/Languages/C and C++
Requires:   %{name}-common = %{version}
Requires:   zlib-devel
Requires:   libacl-devel
Requires:   postgresql-devel
Requires:   libcap-devel
%if 0%{?build_sqlite3}
%if 0%{?suse_version}
Requires:   sqlite3-devel
%else
Requires:   sqlite-devel
%endif
%endif
%if 0%{?rhel_version} || 0%{?centos_version} || 0%{?fedora_version}
Requires:   openssl-devel
%else
Requires:   libopenssl-devel
%endif
%if 0%{?rhel_version} >= 600 || 0%{?centos_version} >= 600 || 0%{?fedora_version}
%if %{use_libwrap}
Requires:   tcp_wrappers-devel
%endif
%else
%if 0%{?rhel_version} || 0%{?centos_version}
%if %{use_libwrap}
Requires:   tcp_wrappers
%endif
%else
Requires:   tcpd-devel
%endif
%endif
%if 0%{?rhel_version} >= 700 || 0%{?centos_version} >= 700 || 0%{?fedora_version} >= 19
Requires:   mariadb-devel
%else
%if 0%{?rhel_version} || 0%{?centos_version} || 0%{?fedora_version}
Requires:   mysql-devel
%else
Requires:   libmysqlclient-devel
%endif
%endif

%package    regress-config
Summary:    Required files for bareos-regress
Group:      Development/Languages/C and C++
Requires:   %{name}-common = %{version}

%if 0%{?python_plugins}
%package    director-python-plugin
Summary:    Python plugin for Bareos Director daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-director = %{version}

%package    filedaemon-python-plugin
Summary:    Python plugin for Bareos File daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-filedaemon = %{version}

%package    filedaemon-ldap-python-plugin
Summary:    LDAP Python plugin for Bareos File daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-filedaemon = %{version}
Requires:   bareos-filedaemon-python-plugin = %{version}
Requires:   python-ldap

%package    filedaemon-ovirt-python-plugin
Summary:    Ovirt Python plugin for Bareos File daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-filedaemon = %{version}
Requires:   bareos-filedaemon-python-plugin = %{version}
Requires:   python-pycurl
Requires:   python-lxml
Requires:   python-ovirt-engine-sdk4

%package    filedaemon-percona-xtrabackup-python-plugin
Summary:    LDAP Python plugin for Bareos File daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-filedaemon = %{version}
Requires:   bareos-filedaemon-python-plugin = %{version}
#Requires:   python-percona

%package    storage-python-plugin
Summary:    Python plugin for Bareos Storage daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-storage = %{version}

%description director-python-plugin
%{dscr}

This package contains the python plugin for the director daemon

%description filedaemon-python-plugin
%{dscr}

This package contains the python plugin for the file daemon

%description filedaemon-ldap-python-plugin
%{dscr}

This package contains the LDAP python plugin for the file daemon

%description filedaemon-ovirt-python-plugin
%{dscr}

This package contains the Ovirt python plugin for the file daemon

%description filedaemon-percona-xtrabackup-python-plugin
%{dscr}

This package contains the Percona python plugin for the file daemon

%description storage-python-plugin
%{dscr}

This package contains the python plugin for the storage daemon

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

%if 0%{?ceph}
%package    filedaemon-ceph-plugin
Summary:    CEPH plugin for Bareos File daemon
Group:      Productivity/Archiving/Backup
Requires:   bareos-filedaemon = %{version}

%description filedaemon-ceph-plugin
%{dscr}

This package contains the CEPH plugins for the file daemon

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

%if 0%{?droplet}
%description storage-droplet
%{dscr}

This package contains the Storage backend for Object Storage (through libdroplet).
%endif

%if 0%{?glusterfs}
%description storage-glusterfs
%{dscr}

This package contains the Storage backend for GlusterFS.
%endif

%if 0%{?ceph}
%description storage-ceph
%{dscr}

This package contains the Storage backend for CEPH.
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

%description database-mysql
%{dscr}

This package contains the shared library to use mysql as catalog db.

%if 0%{?build_sqlite3}
%description database-sqlite3
%{dscr}

This package contains the shared library to use sqlite as catalog db.
%endif

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

%description devel
%{dscr}

This package contains bareos development files.

%description regress-config
%{dscr}

This package contains required files for Bareos regression testing.



%prep
# this is a hack so we always build in "bareos" and not in "bareos-version"
%setup -c -n bareos
mv bareos-*/* .

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

# use Developer Toolset 7 compiler as standard is too old
%if 0%{?centos_version} == 600 || 0%{?rhel_version} == 600
export PATH=/opt/rh/devtoolset-7/root/usr/bin:/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin:/root/bin
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
  -DDEBUG_PREFIX_MAP:BOOL=OFF \
  -Dprefix=%{_prefix}\
  -Dlibdir=%{library_dir} \
  -Dsbindir=%{_sbindir} \
  -Dsbin-perm=755 \
  -Dsysconfdir=%{_sysconfdir} \
  -Dconfdir=%{_sysconfdir}/bareos \
  -Dmandir=%{_mandir} \
  -Ddocdir=%{_docdir}/%{name} \
  -Dhtmldir=%{_docdir}/%{name}/html \
  -Darchivedir=/var/lib/%{name}/storage \
  -Dbackenddir=%{backend_dir} \
  -Dscriptdir=%{script_dir} \
  -Dworkingdir=%{working_dir} \
  -Dplugindir=%{plugin_dir} \
  -Dpiddir=%{pid_dir} \
  -Dbsrdir=%{bsr_dir} \
  -Dlogdir=/var/log/bareos \
  -Dsubsysdir=%{_subsysdir} \
%if 0%{?python_plugins}
  -Dpython=yes \
%endif
  -Dreadline=yes \
  -Dbatch-insert=yes \
  -Ddynamic-cats-backends=yes \
  -Ddynamic-storage-backends=yes \
  -Dscsi-crypto=yes \
  -Dlmdb=yes \
  -Dndmp=yes \
  -Dipv6=yes \
  -Dacl=yes \
  -Dxattr=yes \
%if 0%{?build_bat}
  -Dbat=yes \
%endif
%if 0%{?build_qt_monitor}
  -Dtraymonitor=yes \
%endif
%if 0%{?client_only}
  -Dclient-only=yes \
%endif
  -Dpostgresql=yes \
  -Dmysql=yes \
%if 0%{?build_sqlite3}
  -Dsqlite3=yes \
%endif
%if %{use_libwrap}
  -Dtcp-wrappers=yes \
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
  -Dopenssl=yes \
  -Dbasename="XXX_REPLACE_WITH_LOCAL_HOSTNAME_XXX" \
  -Dhostname="XXX_REPLACE_WITH_LOCAL_HOSTNAME_XXX" \
%if 0%{?systemd_support}
  -Dsystemd=yes \
%endif
  -Dincludes=yes \
  -DVERSION_STRING=%version

#Add flags
%__make CFLAGS="$RPM_OPT_FLAGS" CXXFLAGS="$RPM_OPT_FLAGS" %{?_smp_mflags};

%check
# run unit tests
pushd %{CMAKE_BUILDDIR}
make clean
ctest -R gtest: -D Continuous || result=$?
result=$?
if [ $result -eq 1 ]; then
  echo "ctest result $result is expected and OK"
  exit 0
else
  echo "ctest result $result is not 1, ERROR"
fi



%install
##if 0#{?suse_version}
#    #makeinstall DESTDIR=#{buildroot} install
##else
#    make DESTDIR=#{buildroot} install
##endif
#make DESTDIR=#{buildroot} install-autostart

pushd %{CMAKE_BUILDDIR}
make  DESTDIR=%{buildroot} install

popd

#rm -Rvf build


install -d -m 755 %{buildroot}/usr/share/applications
install -d -m 755 %{buildroot}/usr/share/pixmaps
install -d -m 755 %{buildroot}%{backend_dir}
install -d -m 755 %{buildroot}%{working_dir}
install -d -m 755 %{buildroot}%{plugin_dir}

#Cleaning
for F in  \
%if 0%{?client_only}
    %{_mandir}/man1/bregex.1.gz \
    %{_mandir}/man1/bsmtp.1.gz \
    %{_mandir}/man1/bwild.1.gz \
    %{_mandir}/man8/bareos-dbcheck.8.gz \
    %{_mandir}/man8/bareos-dbcopy.8.gz \
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
    %{script_dir}/bareos_config \
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

# install tray monitor
# #if 0#{?build_qt_monitor}
# #if 0#{?suse_version} > 1010
# disables, because suse_update_desktop_file complains
# that there are two desktop file (applications and autostart)
# ##suse_update_desktop_file bareos-tray-monitor System Backup
# #endif
# #endif


# remove man page if qt tray monitor is not built
%if !0%{?build_qt_monitor}
rm %{buildroot}%{_mandir}/man1/bareos-tray-monitor.*
%endif

# install systemd service files
%if 0%{?systemd_support}
install -d -m 755 %{buildroot}%{_unitdir}
install -m 644 platforms/systemd/bareos-dir.service %{buildroot}%{_unitdir}
install -m 644 platforms/systemd/bareos-fd.service %{buildroot}%{_unitdir}
install -m 644 platforms/systemd/bareos-sd.service %{buildroot}%{_unitdir}
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

%files
%defattr(-, root, root)
%{_docdir}/%{name}/README.bareos

%files client
%defattr(-, root, root)
%dir %{_docdir}/%{name}
%{_docdir}/%{name}/README.bareos-client


%files bconsole
# console package
%defattr(-, root, root)
%attr(0640, root, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bconsole.conf
%{_bindir}/bconsole
%{_sbindir}/bconsole
%{_mandir}/man1/bconsole.1.gz

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
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/catalog/MyCatalog.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/client/bareos-fd.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/console/bareos-mon.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/director/bareos-dir.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/fileset/Catalog.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/fileset/LinuxAll.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/fileset/SelfTest.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) "%{_sysconfdir}/%{name}/bareos-dir.d/fileset/Windows All Drives.conf"
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/job/backup-bareos-fd.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/job/BackupCatalog.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/jobdefs/DefaultJob.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/job/RestoreFiles.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/messages/Daemon.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/messages/Standard.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/pool/Differential.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/pool/Full.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/pool/Incremental.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/pool/Scratch.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/profile/operator.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/schedule/WeeklyCycleAfterBackup.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/schedule/WeeklyCycle.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-dir.d/storage/File.conf
%attr(0750, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir-export/
%if 0%{?build_qt_monitor}
%attr(0755, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/tray-monitor.d/director
%attr(0644, %{daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/tray-monitor.d/director/Director-local.conf
%endif
%config(noreplace) %{_sysconfdir}/logrotate.d/%{name}-dir
# we do not have any dir plugin but the python plugin
#%%{plugin_dir}/*-dir.so
%{script_dir}/delete_catalog_backup
%{script_dir}/make_catalog_backup
%{script_dir}/make_catalog_backup.pl
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
%attr(0750, %{storage_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-sd.d
%attr(0750, %{storage_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-sd.d/autochanger
%attr(0750, %{storage_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-sd.d/device
%attr(0750, %{storage_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-sd.d/director
%attr(0750, %{storage_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-sd.d/ndmp
%attr(0750, %{storage_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-sd.d/messages
%attr(0750, %{storage_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-sd.d/storage
%attr(0640, %{storage_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-sd.d/device/FileStorage.conf
%attr(0640, %{storage_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-sd.d/director/bareos-dir.conf
%attr(0640, %{storage_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-sd.d/director/bareos-mon.conf
%attr(0640, %{storage_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-sd.d/messages/Standard.conf
%attr(0640, %{storage_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-sd.d/storage/bareos-sd.conf
%if 0%{?build_qt_monitor}
%attr(0755, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/tray-monitor.d/storage
%attr(0644, %{daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/tray-monitor.d/storage/StorageDaemon-local.conf
%endif
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
%{_mandir}/man8/bareos-sd.8.gz
%if 0%{?systemd_support}
%{_unitdir}/bareos-sd.service
%endif
%attr(0775, %{storage_daemon_user}, %{daemon_group}) %dir /var/lib/%{name}/storage

%files storage-tape
# tape specific files
%defattr(-, root, root)
%{backend_dir}/libbareossd-gentape*.so
%{backend_dir}/libbareossd-tape*.so
%{script_dir}/mtx-changer
%config(noreplace) %{_sysconfdir}/%{name}/mtx-changer.conf
%{_mandir}/man8/bscrypto.8.gz
%{_mandir}/man8/btape.8.gz
%{_sbindir}/bscrypto
%{_sbindir}/btape
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/storage/Tape.conf.example
%attr(0640, %{storage_daemon_user},  %{daemon_group}) %{_sysconfdir}/%{name}/bareos-sd.d/autochanger/autochanger-0.conf.example
%attr(0640, %{storage_daemon_user},  %{daemon_group}) %{_sysconfdir}/%{name}/bareos-sd.d/device/tapedrive-0.conf.example
%{plugin_dir}/scsicrypto-sd.so
%{plugin_dir}/scsitapealert-sd.so

%files storage-fifo
%defattr(-, root, root)
%{backend_dir}/libbareossd-fifo*.so
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/storage/NULL.conf.example
%attr(0640, %{storage_daemon_user}, %{daemon_group})  %{_sysconfdir}/%{name}/bareos-sd.d/device/NULL.conf.example

%if 0%{?droplet}
%files storage-droplet
%defattr(-, root, root)
%{backend_dir}/libbareossd-chunked*.so
%{backend_dir}/libbareossd-droplet*.so
%attr(0640, %{director_daemon_user},%{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/storage/S3_Object.conf.example
%attr(0640, %{storage_daemon_user},%{daemon_group})  %{_sysconfdir}/%{name}/bareos-sd.d/device/S3_ObjectStorage.conf.example
%dir %{_sysconfdir}/%{name}/bareos-sd.d/device/droplet/
%attr(0640, %{storage_daemon_user},%{daemon_group})  %{_sysconfdir}/%{name}/bareos-sd.d/device/droplet/*.example
%endif

%if 0%{?glusterfs}
%files storage-glusterfs
%defattr(-, root, root)
%{backend_dir}/libbareossd-gfapi*.so
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/storage/Gluster.conf.example
%attr(0640, %{storage_daemon_user}, %{daemon_group})  %{_sysconfdir}/%{name}/bareos-sd.d/device/GlusterStorage.conf.example
%endif

%if 0%{?ceph}
%files storage-ceph
%defattr(-, root, root)
%{backend_dir}/libbareossd-rados*.so
%{backend_dir}/libbareossd-cephfs*.so
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/storage/Rados.conf.example
%attr(0640, %{storage_daemon_user}, %{daemon_group})  %{_sysconfdir}/%{name}/bareos-sd.d/device/RadosStorage.conf.example
%endif

%endif # not client_only

%files filedaemon
# fd package (bareos-fd, plugins)
%defattr(-, root, root)
%attr(0750, %{file_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-fd.d/
%attr(0750, %{file_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-fd.d/client
%attr(0750, %{file_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-fd.d/director
%attr(0750, %{file_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-fd.d/messages
%attr(0640, %{file_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-fd.d/client/myself.conf
%attr(0640, %{file_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-fd.d/director/bareos-dir.conf
%attr(0640, %{file_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-fd.d/director/bareos-mon.conf
%attr(0640, %{file_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-fd.d/messages/Standard.conf
%if 0%{?build_qt_monitor}
%attr(0755, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/tray-monitor.d/client
%attr(0644, %{daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/tray-monitor.d/client/FileDaemon-local.conf
%endif
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

%files common
# common shared libraries (without db)
%defattr(-, root, root)
%attr(0755, root, %{daemon_group})           %dir %{_sysconfdir}/%{name}
%if !0%{?client_only}
# these directories belong to bareos-common,
# as other packages may contain configurations for the director.
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-dir.d
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-dir.d/catalog
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-dir.d/client
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-dir.d/console
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-dir.d/counter
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-dir.d/director
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-dir.d/fileset
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-dir.d/job
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-dir.d/jobdefs
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-dir.d/messages
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-dir.d/pool
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-dir.d/profile
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-dir.d/schedule
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-dir.d/storage
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/bareos-dir.d/user
# tray monitor configurate is installed by the target daemons
%if 0%{?build_qt_monitor}
%attr(0755, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/tray-monitor.d
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
%{script_dir}/bareos-explorer
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
%doc AGPL-3.0.txt LICENSE README.*
#TODO: cmake does not create build directory
#doc build/

%if !0%{?client_only}

%files database-common
# catalog independent files
%defattr(-, root, root)
%{library_dir}/libbareossql*.so.*
%{library_dir}/libbareoscats*.so.*
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
%{backend_dir}/libbareoscats-postgresql.so*

%files database-mysql
# mysql catalog files
%defattr(-, root, root)
%{script_dir}/ddl/*/mysql*.sql
%{backend_dir}/libbareoscats-mysql.so*

%if 0%{?build_sqlite3}
%files database-sqlite3
# sqlite3 catalog files
%defattr(-, root, root)
%{script_dir}/ddl/*/sqlite3*.sql
%{backend_dir}/libbareoscats-sqlite3.so*
%endif

%files database-tools
# dbtools with link to db libs (dbcheck, bscan, dbcopy)
%defattr(-, root, root)
%{_sbindir}/bareos-dbcheck
%{_sbindir}/bareos-dbcopy
%{_sbindir}/bscan
%{_mandir}/man8/bareos-dbcheck.8.gz
%{_mandir}/man8/bareos-dbcopy.8.gz
%{_mandir}/man8/bscan.8.gz

%files tools
# tools without link to db libs (bwild, bregex)
%defattr(-, root, root)
%{_bindir}/bregex
%{_bindir}/bwild
%{_sbindir}/bcopy
%{_sbindir}/bextract
%{_sbindir}/bls
%{_sbindir}/bregex
%{_sbindir}/bwild
%{_sbindir}/bpluginfo
%{_mandir}/man1/bwild.1.gz
%{_mandir}/man1/bregex.1.gz
%{_mandir}/man8/bcopy.8.gz
%{_mandir}/man8/bextract.8.gz
%{_mandir}/man8/bls.8.gz
%{_mandir}/man8/bpluginfo.8.gz

%if 0%{?build_qt_monitor}
%files traymonitor
%defattr(-,root, root)
%attr(0755, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/%{name}/tray-monitor.d/monitor
%attr(0644, %{daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/tray-monitor.d/monitor/bareos-mon.conf
%config %{_sysconfdir}/xdg/autostart/bareos-tray-monitor.desktop
%{_bindir}/bareos-tray-monitor
%{_mandir}/man1/bareos-tray-monitor.1.gz
/usr/share/applications/bareos-tray-monitor.desktop
/usr/share/pixmaps/bareos-tray-monitor.xpm
%endif


%endif # client_only

%files devel
%defattr(-, root, root)
/usr/include/%{name}

%if 0%{?python_plugins}
%files filedaemon-python-plugin
%defattr(-, root, root)
%{plugin_dir}/python-fd.so
%{plugin_dir}/bareos-fd.py*
%{plugin_dir}/bareos-fd-local-fileset.py*
%{plugin_dir}/bareos-fd-mock-test.py*
%{plugin_dir}/BareosFdPluginBaseclass.py*
%{plugin_dir}/BareosFdPluginLocalFileset.py*
%{plugin_dir}/BareosFdWrapper.py*
%{plugin_dir}/bareos_fd_consts.py*

%files filedaemon-ldap-python-plugin
%defattr(-, root, root)
%{plugin_dir}/bareos-fd-ldap.py*
%{plugin_dir}/BareosFdPluginLDAP.py*
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/fileset/plugin-ldap.conf.example
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/job/backup-ldap.conf.example
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/job/restore-ldap.conf.example

%files filedaemon-ovirt-python-plugin
%defattr(-, root, root)
%{plugin_dir}/bareos-fd-ovirt.py*
%{plugin_dir}/BareosFdPluginOvirt.py*
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/fileset/plugin-ovirt.conf.example
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/job/backup-ovirt.conf.example

%files filedaemon-percona-xtrabackup-python-plugin
%defattr(-, root, root)
%{plugin_dir}/bareos-fd-percona-xtrabackup.py*
%{plugin_dir}/BareosFdPluginPerconaXtraBackup.py*
##attr(0640, #{director_daemon_user}, #{daemon_group}) #{_sysconfdir}/#{name}/bareos-dir.d/fileset/plugin-percona-xtrabackup.conf.example
##attr(0640, #{director_daemon_user}, #{daemon_group}) #{_sysconfdir}/#{name}/bareos-dir.d/job/backup-percona-xtrabackup.conf.example
##attr(0640, #{director_daemon_user}, #{daemon_group}) #{_sysconfdir}/#{name}/bareos-dir.d/job/restore-percona-xtrabackup.conf.example

%files director-python-plugin
%defattr(-, root, root)
%{plugin_dir}/python-dir.so
%{plugin_dir}/bareos-dir.py*
%{plugin_dir}/bareos_dir_consts.py*
%{plugin_dir}/BareosDirPluginBaseclass.py*
%{plugin_dir}/bareos-dir-class-plugin.py*
%{plugin_dir}/BareosDirWrapper.py*

%files storage-python-plugin
%defattr(-, root, root)
%{plugin_dir}/python-sd.so
%{plugin_dir}/bareos-sd.py*
%{plugin_dir}/bareos_sd_consts.py*
%{plugin_dir}/BareosSdPluginBaseclass.py*
%{plugin_dir}/BareosSdWrapper.py*
%{plugin_dir}/bareos-sd-class-plugin.py*

%endif # python_plugins

%if 0%{?glusterfs}
%files filedaemon-glusterfs-plugin
%{script_dir}/bareos-glusterfind-wrapper
%{plugin_dir}/gfapi-fd.so
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/fileset/plugin-gfapi.conf.example
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/job/BackupGFAPI.conf.example
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/job/RestoreGFAPI.conf.example
%endif

%if 0%{?ceph}
%files filedaemon-ceph-plugin
%{plugin_dir}/cephfs-fd.so
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/fileset/plugin-cephfs.conf.example
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/job/BackupCephfs.conf.example
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/job/RestoreCephfs.conf.example
%{plugin_dir}/rados-fd.so
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/fileset/plugin-rados.conf.example
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/job/BackupRados.conf.example
%attr(0640, %{director_daemon_user}, %{daemon_group}) %{_sysconfdir}/%{name}/bareos-dir.d/job/RestoreRados.conf.example
%endif

%files regress-config
%defattr(-, root, root)
%{script_dir}/%{name}
%{script_dir}/bareos-ctl-*
%{_bindir}/timelimit
%{_sbindir}/timelimit
%{_sbindir}/btestls
# must be readable by package build user of bareos-regress.
%attr(0644, %{daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/%{name}/bareos-regress.conf


#
# Define some macros for updating the system settings.
#
%if 0%{?RHEL4}
%define add_service_start() ( /sbin/chkconfig --add %1; %nil)
%define stop_on_removal() ( /sbin/service %1 stop >/dev/null 2>&1 ||  /sbin/chkconfig --del %1 || true; %nil)
%define restart_on_update() (/sbin/service %1 condrestart >/dev/null 2>&1 || true; %nil)
%define insserv_cleanup() (/bin/true; %nil)
%define create_group() (getent group %1 > /dev/null || groupadd -r %1; %nil);
%define create_user() ( getent passwd %1 > /dev/null || useradd -r -c "%1" -d %{working_dir} -g %{daemon_group} -s /bin/false %1; %nil);
%else
# non RHEL4
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

%endif

# With the introduction of config subdirectories (bareos-16.2)
# some config files have been renamed (or even splitted into multiple files).
# However, bareos is still able to work with the old config files,
# but rpm renames them to *.rpmsave.
# To keep the bareos working after updating to bareos-16.2,
# we implement a workaroung:
#   * post: if the old config exists, make a copy of it.
#   * (rpm exchanges files on disk)
#   * posttrans:
#       if the old config file don't exists but we have created a backup before,
#       restore the old config file.
#       Remove our backup, if it exists.
# This update helper should be removed with bareos-17.

%define post_backup_file() \
if [ -f  %1 ]; then \
      cp -a %1 %1.rpmupdate.%{version}.keep; \
fi; \
%nil

%define posttrans_restore_file() \
if [ ! -e %1 -a -e %1.rpmupdate.%{version}.keep ]; then \
   mv %1.rpmupdate.%{version}.keep %1; \
fi; \
if [ -e %1.rpmupdate.%{version}.keep ]; then \
   rm %1.rpmupdate.%{version}.keep; \
fi; \
%nil


%post director
%post_backup_file /etc/%{name}/bareos-dir.conf
%{script_dir}/bareos-config initialize_local_hostname
%{script_dir}/bareos-config initialize_passwords
%{script_dir}/bareos-config initialize_database_driver
%if 0%{?suse_version} >= 1210
%service_add_post bareos-dir.service
/bin/systemctl enable bareos-dir.service >/dev/null 2>&1 || true
%else
%add_service_start bareos-dir
%endif

%posttrans director
%posttrans_restore_file /etc/%{name}/bareos-dir.conf

%post storage
%post_backup_file /etc/%{name}/bareos-sd.conf
# pre script has already generated the storage daemon user,
# but here we add the user to additional groups
%{script_dir}/bareos-config setup_sd_user
%{script_dir}/bareos-config initialize_local_hostname
%{script_dir}/bareos-config initialize_passwords
%if 0%{?suse_version} >= 1210
%service_add_post bareos-sd.service
/bin/systemctl enable bareos-sd.service >/dev/null 2>&1 || true
%else
%add_service_start bareos-sd
%endif

%posttrans storage
%posttrans_restore_file /etc/%{name}/bareos-sd.conf


%if 0%{?ceph}
%post storage-ceph
%post_backup_file /etc/%{name}/bareos-sd.d/device-ceph-rados.conf

%posttrans storage-ceph
%posttrans_restore_file /etc/%{name}/bareos-sd.d/device-ceph-rados.conf
%endif


%post storage-fifo
%post_backup_file /etc/%{name}/bareos-sd.d/device-fifo.conf

%posttrans storage-fifo
%posttrans_restore_file /etc/%{name}/bareos-sd.d/device-fifo.conf


%post storage-tape
%post_backup_file /etc/%{name}/bareos-sd.d/device-tape-with-autoloader.conf

%posttrans storage-tape
%posttrans_restore_file /etc/%{name}/bareos-sd.d/device-tape-with-autoloader.conf


%post filedaemon
%post_backup_file /etc/%{name}/bareos-fd.conf
%{script_dir}/bareos-config initialize_local_hostname
%{script_dir}/bareos-config initialize_passwords
%if 0%{?suse_version} >= 1210
%service_add_post bareos-fd.service
/bin/systemctl enable bareos-fd.service >/dev/null 2>&1 || true
%else
%add_service_start bareos-fd
%endif

%posttrans filedaemon
%posttrans_restore_file /etc/%{name}/bareos-fd.conf


%if 0%{?ceph}
%post filedaemon-ceph-plugin
%post_backup_file /etc/%{name}/bareos-dir.d/plugin-cephfs.conf
%post_backup_file /etc/%{name}/bareos-dir.d/plugin-rados.conf

%posttrans filedaemon-ceph-plugin
%posttrans_restore_file /etc/%{name}/bareos-dir.d/plugin-cephfs.conf
%posttrans_restore_file /etc/%{name}/bareos-dir.d/plugin-rados.conf
%endif


%if 0%{?python_plugins}

%post filedaemon-ldap-python-plugin
%post_backup_file /etc/%{name}/bareos-dir.d/plugin-python-ldap.conf
%posttrans filedaemon-ldap-python-plugin
%posttrans_restore_file /etc/%{name}/bareos-dir.d/plugin-python-ldap.conf

#post filedaemon-percona-xtrabackup-python-plugin
#post_backup_file /etc/#{name}/bareos-dir.d/plugin-python-percona-xtrabackup.conf
#posttrans filedaemon-percona-xtrabackup-python-plugin
#posttrans_restore_file /etc/#{name}/bareos-dir.d/plugin-python-percona-xtrabackup.conf

%endif


%post bconsole
%{script_dir}/bareos-config initialize_local_hostname
%{script_dir}/bareos-config initialize_passwords

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

%postun database-postgresql
/sbin/ldconfig

%post database-mysql
/sbin/ldconfig

%postun database-mysql
/sbin/ldconfig

%if 0%{?build_sqlite3}
%post database-sqlite3
/sbin/ldconfig

%postun database-sqlite3
/sbin/ldconfig
%endif


%if 0%{?build_qt_monitor}

%post traymonitor
%post_backup_file /etc/%{name}/tray-monitor.conf
%{script_dir}/bareos-config initialize_local_hostname
%{script_dir}/bareos-config initialize_passwords

%posttrans traymonitor
%posttrans_restore_file /etc/%{name}/tray-monitor.conf

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
