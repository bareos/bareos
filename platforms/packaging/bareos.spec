#
# spec file for package bareos
# Copyright (c) 2011-2012 Bruno Friedmann (Ioda-Net) and Philipp Storz (dass IT)
#               2013-2015 Bareos GmbH & Co KG
#

Name: 		bareos
Version: 	16.1.0
Release: 	0
Group: 		Productivity/Archiving/Backup
License: 	AGPL-3.0
BuildRoot: 	%{_tmppath}/%{name}-root
URL: 		http://www.bareos.org/
Vendor: 	The Bareos Team
#Packager: 	{_packager}

%define _libversion    16.1.0

%define library_dir    %{_libdir}/bareos
%define backend_dir    %{_libdir}/bareos/backends
%define plugin_dir     %{_libdir}/bareos/plugins
%define script_dir     /usr/lib/bareos/scripts
%define working_dir    /var/lib/bareos
%define pid_dir        /var/lib/bareos
%define bsr_dir        /var/lib/bareos
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
%define build_bat 1
%define build_qt_monitor 1
%define build_sqlite3 1
%define check_cmocka 1
%define glusterfs 0
%define have_git 1
%define ceph 0
%define install_suse_fw 0
%define systemd 0
%define python_plugins 1

#
# SUSE (openSUSE, SLES) specific settigs
#
%if 0%{?sles_version} == 10
%define build_bat 0
%define build_qt_monitor 0
%define build_sqlite3 0
%define check_cmocka 0
%define have_git 0
%define python_plugins 0
%endif

%if 0%{?suse_version} > 1010
%define install_suse_fw 1
%define _fwdefdir   %{_sysconfdir}/sysconfig/SuSEfirewall2.d/services
%endif

# SLES 11
%if 0%{?suse_version} == 1110
# cmocka package is broken on SLES11SP3, 32bit.
# We disable it in all SLES11 build.
%define check_cmocka 0
%endif

%if 0%{?suse_version} > 1140
%define systemd_support 1
%endif

# SLES 12
%if 0%{?suse_version} == 1315 && 0%{?is_opensuse} == 0
%define ceph 1
%endif

#
# RedHat (CentOS, Fedora, RHEL) specific settings
#
%if 0%{?rhel_version} > 0 && 0%{?rhel_version} < 500
%define RHEL4 1
%define client_only 1
%define build_bat 0
%define build_qt_monitor 0
%define build_sqlite3 0
%define check_cmocka 0
%define have_git 0
%define python_plugins 0
%endif

# centos/rhel 5 : segfault when building qt monitor
%if 0%{?centos_version} == 505 || 0%{?rhel_version} == 505
%define build_bat 0
%define build_qt_monitor 0
%define have_git 0
%define python_plugins 0
%endif

%if 0%{?rhel_version} >= 700 || 0%{?centos_version} >= 700 || 0%{?fedora_version} >= 19
%define systemd_support 1
%if 0%{?fedora_version} != 19
%define glusterfs 1
%endif
%endif

%if 0%{?rhel_version} >= 700
%define ceph 1
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

%if 0%{?ceph}
BuildRequires: ceph-devel
%endif

%if 0%{?have_git}
BuildRequires: git-core
%endif

Source0: %{name}-%{version}.tar.gz

#BuildRequires: elfutils
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
BuildRequires: libfastlz-devel
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

%if 0%{?build_bat} || 0%{?build_qt_monitor}
BuildRequires: libqt4-devel
%endif

%if 0%{?check_cmocka}
BuildRequires: libcmocka-devel >= 1.0.1
%endif

%if 0%{?python_plugins}
BuildRequires: python-devel >= 2.6
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
BuildRequires: tcp_wrappers

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
BuildRequires: tcp_wrappers-devel
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
%else
Requires(pre): shadow-utils
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
Requires:   %{name}-database-common = %{version}
Provides:   %{name}-catalog-sqlite3
Provides:   %{name}-database-backend
%endif

%package    database-tools
Summary:    Bareos CLI tools with database dependencies (bareos-dbcheck, bscan)
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

%if 0%{?build_bat}
%package    bat
Summary:    Bareos Admin Tool (GUI)
Group:      Productivity/Archiving/Backup
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
Requires:   tcp_wrappers-devel
%else
%if 0%{?rhel_version} || 0%{?centos_version}
Requires:   tcp_wrappers
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

%if 0%{?build_bat}
%description bat
%{dscr}

This package contains the Bareos Admin Tool (BAT).
BAT is a graphical interface for Bareos.
%endif

%description devel
%{dscr}

This package contains bareos development files.

%prep
%setup

%build
%if !0%{?suse_version}
export PATH=$PATH:/usr/lib64/qt4/bin:/usr/lib/qt4/bin
%endif
export MTX=/usr/sbin/mtx
# Notice keep the upstream order of ./configure --help
%configure \
  --prefix=%{_prefix} \
  --libdir=%{library_dir} \
  --sbindir=%{_sbindir} \
  --with-sbin-perm=755 \
  --sysconfdir=%{_sysconfdir} \
  --with-confdir=%{_sysconfdir}/bareos \
  --mandir=%{_mandir} \
  --docdir=%{_docdir}/%{name} \
  --htmldir=%{_docdir}/%{name}/html \
  --with-archivedir=/var/lib/bareos/storage \
  --with-backenddir=%{backend_dir} \
  --with-scriptdir=%{script_dir} \
  --with-working-dir=%{working_dir} \
  --with-plugindir=%{plugin_dir} \
  --with-pid-dir=%{pid_dir} \
  --with-bsrdir=%{bsr_dir} \
  --with-logdir=/var/log/bareos \
  --with-subsys-dir=%{_subsysdir} \
%if 0%{?python_plugins}
  --with-python \
%endif
  --enable-smartalloc \
  --disable-conio \
  --enable-readline \
  --enable-batch-insert \
  --enable-dynamic-cats-backends \
  --enable-dynamic-storage-backends \
  --enable-scsi-crypto \
  --enable-lmdb \
  --enable-ndmp \
  --enable-ipv6 \
  --enable-acl \
  --enable-xattr \
%if 0%{?build_bat}
  --enable-bat \
%endif
%if 0%{?build_qt_monitor}
  --enable-traymonitor \
%endif
%if 0%{?client_only}
  --enable-client-only \
%endif
  --with-postgresql \
  --with-mysql \
%if 0%{?build_sqlite3}
  --with-sqlite3 \
%endif
  --with-tcp-wrappers \
  --with-dir-user=%{director_daemon_user} \
  --with-dir-group=%{daemon_group} \
  --with-sd-user=%{storage_daemon_user} \
  --with-sd-group=%{storage_daemon_group} \
  --with-fd-user=%{file_daemon_user} \
  --with-fd-group=%{daemon_group} \
  --with-dir-password="XXX_REPLACE_WITH_DIRECTOR_PASSWORD_XXX" \
  --with-fd-password="XXX_REPLACE_WITH_CLIENT_PASSWORD_XXX" \
  --with-sd-password="XXX_REPLACE_WITH_STORAGE_PASSWORD_XXX" \
  --with-mon-dir-password="XXX_REPLACE_WITH_DIRECTOR_MONITOR_PASSWORD_XXX" \
  --with-mon-fd-password="XXX_REPLACE_WITH_CLIENT_MONITOR_PASSWORD_XXX" \
  --with-mon-sd-password="XXX_REPLACE_WITH_STORAGE_MONITOR_PASSWORD_XXX" \
  --with-openssl \
  --with-basename="XXX_REPLACE_WITH_LOCAL_HOSTNAME_XXX" \
  --with-hostname="XXX_REPLACE_WITH_LOCAL_HOSTNAME_XXX" \
%if 0%{?systemd_support}
  --with-systemd \
%endif
  --enable-includes

#Add flags
%__make CFLAGS="$RPM_OPT_FLAGS" CXXFLAGS="$RPM_OPT_FLAGS" %{?_smp_mflags};

%check
# run unit tests
%__make CFLAGS="$RPM_OPT_FLAGS" CXXFLAGS="$RPM_OPT_FLAGS" %{?_smp_mflags} check;

%install
%if 0%{?suse_version}
    %makeinstall DESTDIR=%{buildroot} install
%else
    make DESTDIR=%{buildroot} install
%endif
make DESTDIR=%{buildroot} install-autostart

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
    %{_sysconfdir}/bareos/mtx-changer.conf \
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
    %{script_dir}/bareos \
    %{script_dir}/bareos_config \
    %{script_dir}/btraceback.dbx \
    %{script_dir}/btraceback.mdb \
    %{script_dir}/bareos-ctl-funcs \
    %{script_dir}/bareos-ctl-dir \
    %{script_dir}/bareos-ctl-fd \
    %{script_dir}/bareos-ctl-sd \
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
rm -f %{buildroot}/%{_sysconfdir}/bareos/bareos-dir.d/plugin-python-ldap.conf
%endif

%if 0%{?build_bat}
%if 0%{?suse_version} > 1010
%suse_update_desktop_file -i bat System Utility Archiving
%endif
%endif

# install tray monitor
# #if 0#{?build_qt_monitor}
# #if 0#{?suse_version} > 1010
# disables, because suse_update_desktop_file complains
# that there are two desktop file (applications and autostart)
# ##suse_update_desktop_file bareos-tray-monitor System Backup
# #endif
# #endif

# remove man page if bat is not built
%if !0%{?build_bat}
rm %{buildroot}%{_mandir}/man1/bat.1.gz
%endif

# remove man page if qt tray monitor is not built
%if !0%{?build_qt_monitor}
rm %{buildroot}%{_mandir}/man1/bareos-tray-monitor.1.gz
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
%attr(0640, root, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bconsole.conf
%{script_dir}/bconsole
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
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-dir.conf
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
%attr(0640, %{storage_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-sd.conf
%attr(0750, %{storage_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-sd.d
%attr(0750, %{storage_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-sd.d/autochanger
%attr(0750, %{storage_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-sd.d/device
%attr(0750, %{storage_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-sd.d/director
%attr(0750, %{storage_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-sd.d/ndmp
%attr(0750, %{storage_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-sd.d/messages
%attr(0750, %{storage_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-sd.d/storage
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
%attr(0775, %{storage_daemon_user}, %{daemon_group}) %dir /var/lib/bareos/storage

%files storage-tape
# tape specific files
%defattr(-, root, root)
%{backend_dir}/libbareossd-gentape*.so
%{backend_dir}/libbareossd-tape*.so
%{script_dir}/mtx-changer
%config(noreplace) %{_sysconfdir}/bareos/mtx-changer.conf
%{_mandir}/man8/bscrypto.8.gz
%{_mandir}/man8/btape.8.gz
%{_sbindir}/bscrypto
%{_sbindir}/btape
%attr(0640, %{storage_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-sd.d/device-tape-with-autoloader.conf
%{plugin_dir}/scsicrypto-sd.so
%{plugin_dir}/scsitapealert-sd.so

%files storage-fifo
%defattr(-, root, root)
%{backend_dir}/libbareossd-fifo*.so
%attr(0640, %{storage_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-sd.d/device-fifo.conf

%if 0%{?glusterfs}
%files storage-glusterfs
%defattr(-, root, root)
%{backend_dir}/libbareossd-gfapi*.so
%attr(0640, %{storage_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-sd.d/device-gluster.conf
%endif

%if 0%{?ceph}
%files storage-ceph
%defattr(-, root, root)
%{backend_dir}/libbareossd-rados*.so
%{backend_dir}/libbareossd-cephfs*.so
%attr(0640, %{storage_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-sd.d/device-ceph-rados.conf
%endif

%endif # not client_only

%files filedaemon
# fd package (bareos-fd, plugins)
%defattr(-, root, root)
%attr(0640, %{file_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-fd.conf
%attr(0750, %{file_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-fd.d/
%attr(0750, %{file_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-fd.d/client
%attr(0750, %{file_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-fd.d/director
%attr(0750, %{file_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-fd.d/messages
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
%attr(-, root, %{daemon_group}) %dir %{_sysconfdir}/bareos
%if !0%{?client_only}
# these directories belong to bareos-common,
# as other packages may contain configurations for the director.
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-dir.d
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-dir.d/catalog
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-dir.d/client
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-dir.d/console
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-dir.d/counter
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-dir.d/director
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-dir.d/fileset
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-dir.d/job
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-dir.d/jobdefs
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-dir.d/messages
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-dir.d/pool
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-dir.d/profile
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-dir.d/schedule
%attr(0750, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-dir.d/storage
%endif
%dir %{backend_dir}
%{library_dir}/libbareos-*.so
%{library_dir}/libbareoscfg-*.so
%{library_dir}/libbareosfind-*.so
%{library_dir}/libbareoslmdb-*.so
%if !0%{?client_only}
%{library_dir}/libbareosndmp-*.so
%{library_dir}/libbareossd-*.so
%endif
# generic stuff needed from multiple bareos packages
%dir /usr/lib/bareos/
%dir %{script_dir}
%{script_dir}/bareos-config
%{script_dir}/bareos-config-lib.sh
%{script_dir}/bareos-explorer
%{script_dir}/btraceback.gdb
%if "%{_libdir}" != "/usr/lib/"
%dir %{_libdir}/bareos/
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
%attr(0775, %{daemon_user}, %{daemon_group}) %dir /var/log/bareos
%doc AGPL-3.0.txt AUTHORS LICENSE README.*
%doc build/

%if !0%{?client_only}

%files database-common
# catalog independent files
%defattr(-, root, root)
%{library_dir}/libbareossql-*.so
%{library_dir}/libbareoscats-*.so
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
%{backend_dir}/libbareoscats-postgresql.so
%{backend_dir}/libbareoscats-postgresql-*.so

%files database-mysql
# mysql catalog files
%defattr(-, root, root)
%{script_dir}/ddl/*/mysql*.sql
%{backend_dir}/libbareoscats-mysql.so
%{backend_dir}/libbareoscats-mysql-*.so

%if 0%{?build_sqlite3}
%files database-sqlite3
# sqlite3 catalog files
%defattr(-, root, root)
%{script_dir}/ddl/*/sqlite3*.sql
%{backend_dir}/libbareoscats-sqlite3.so
%{backend_dir}/libbareoscats-sqlite3-*.so
%endif

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
%attr(-, root, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/tray-monitor.conf
%config %{_sysconfdir}/xdg/autostart/bareos-tray-monitor.desktop
%{_bindir}/bareos-tray-monitor
%{_mandir}/man1/bareos-tray-monitor.1.gz
/usr/share/applications/bareos-tray-monitor.desktop
/usr/share/pixmaps/bareos-tray-monitor.xpm
%endif

%if 0%{?build_bat}
%files bat
%defattr(-, root, root)
%attr(-, root, %{daemon_group}) %{_bindir}/bat
%attr(640, root, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bat.conf
%{_prefix}/share/pixmaps/bat.png
%{_prefix}/share/pixmaps/bat.svg
%{_prefix}/share/applications/bat.desktop
%{_mandir}/man1/bat.1.gz
%dir %{_docdir}/%{name}
%dir %{_docdir}/%{name}/html/
%doc %{_docdir}/%{name}/html/bat/
%endif

%endif # client_only

%files devel
%defattr(-, root, root)
/usr/include/bareos
%{library_dir}/*.la

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
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-dir.d/plugin-python-ldap.conf

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
%{plugin_dir}/gfapi-fd.so
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-dir.d/plugin-gfapi.conf
%endif

%if 0%{?ceph}
%files filedaemon-ceph-plugin
%{plugin_dir}/cephfs-fd.so
%{plugin_dir}/rados-fd.so
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-dir.d/plugin-cephfs.conf
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-dir.d/plugin-rados.conf
%endif

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

%post director
%{script_dir}/bareos-config initialize_local_hostname
%{script_dir}/bareos-config initialize_passwords
%{script_dir}/bareos-config initialize_database_driver
%if 0%{?suse_version} >= 1210
%service_add_post bareos-dir.service
/bin/systemctl enable bareos-dir.service >/dev/null 2>&1 || true
%else
%add_service_start bareos-dir
%endif

%post storage
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

%post filedaemon
%{script_dir}/bareos-config initialize_local_hostname
%{script_dir}/bareos-config initialize_passwords
%if 0%{?suse_version} >= 1210
%service_add_post bareos-fd.service
/bin/systemctl enable bareos-fd.service >/dev/null 2>&1 || true
%else
%add_service_start bareos-fd
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
%{script_dir}/bareos-config initialize_local_hostname
%{script_dir}/bareos-config initialize_passwords
%endif

%if 0%{?build_bat}
%post bat
%{script_dir}/bareos-config initialize_local_hostname
%{script_dir}/bareos-config initialize_passwords
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
