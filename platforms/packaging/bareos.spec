#
# spec file for package bareos
# Copyright (c) 2011-2012 Bruno Friedmann (Ioda-Net) and Philipp Storz (dass IT)
#                    2013 Bareos GmbH & Co KG
#
#   Redesign of the bareos specfile: goals (20110922)
#
#   * only support platforms that are available in OBS
#   * activate all options available (if reasonable)
#   * single-dir-install is not supported
#   * use special group (bconsole) for users that can access bconsole
#   * Single packages for:
#       * console package
#       * dir package ( bsmtp )
#       * sd package ( bls + btape + bcopy + bextract )
#       * fd package ( )
#       * tray monitor
#       * bareos-database-{sqlite,postgresql,mysql} (libs) (make_database/tables/grant rights)
#       * sql common abstract sql libraries (without db)
#       * libs common libraries (without db)
#       * tools without link to db libs (bwild, bregex)
#       * tools with link to db libs (dbcheck, bscan)
#       * bat
#       * doc
#
#	For openSUSE/SUSE we placed the /usr/sbin/rcscript
#	And added the firewall basics rules
#
#	Notice : the libbareoscats* package to be able to pass the shlib name policy are
#	explicitly named
#
# Please submit bugfixes or comments via http://bugs.opensuse.org/

Summary: 	The Network Backup Solution
Name: 		bareos
Version: 	13.2.2
Release: 	1.0
Group: 		Productivity/Archiving/Backup
License: 	AGPL-3.0
BuildRoot: 	%{_tmppath}/%{name}-root
URL: 		http://www.bareos.org/
Vendor: 	The Bareos Team
#Packager: 	{_packager}

%define _libversion    13.2.2

%define plugin_dir     %_libdir/bareos/plugins
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

%define build_bat 1
%define build_qt_monitor 1
%define build_sqlite3 1
%define systemd 0

# firewall installation
%define install_suse_fw 0

%if 0%{?suse_version} > 1010
%define _fwdefdir   %{_sysconfdir}/sysconfig/SuSEfirewall2.d/services
%define install_suse_fw 1
%endif

%if 0%{?suse_version} > 1140 || 0%{?fedora_version} > 14
%define systemd_support 1
%endif

# centos/rhel 5 : segfault when building qt monitor
%if 0%{?centos_version} == 505 || 0%{?rhel_version} == 505
%define build_bat 0
%define build_qt_monitor 0
%endif

%if 0%{?sles_version} == 10
%define build_bat 0
%define build_qt_monitor 0
%define build_sqlite3 0
%endif

%if 0%{?systemd_support}
BuildRequires: systemd
%{?systemd_requires}
%endif

Source0: %{name}_%{version}.tar.gz

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
%if 0%{?build_sqlite3}
%if 0%{?suse_version}
BuildRequires: sqlite3-devel
%else
BuildRequires: sqlite-devel
%endif
%endif
BuildRequires: mysql-devel
BuildRequires: postgresql-devel
BuildRequires: libqt4-devel
BuildRequires: openssl
BuildRequires: libcap-devel
BuildRequires: mtx

%if 0%{?suse_version}
# link identical files
BuildRequires: fdupes
BuildRequires: termcap
BuildRequires: pwdutils
BuildRequires: tcpd-devel
BuildRequires: update-desktop-files

# Some magic to be able to determine what platform we are running on.
%if !0%{sles_version}
BuildRequires: openSUSE-release
%else
%if 0%{?sles_version} && !0%{?sled_version}
BuildRequires: sles-release
%else
BuildRequires: sled-release
%endif
%endif

BuildRequires: lsb-release

%else
BuildRequires: qt4-devel
BuildRequires: libtermcap-devel
BuildRequires: passwd
BuildRequires: tcp_wrappers

# Some magic to be able to determine what platform we are running on.
%if 0%{?rhel_version} || 0%{?centos_version} || 0%{?fedora_version}
%if 0%{?fedora_version}
BuildRequires: fedora-release
%endif
# Favor lsb_release on platform that have proper support for it.
%if 0%{?rhel_version} >= 600 || 0%{?centos_version} >= 600 || 0%{?fedora_version}
BuildRequires: redhat-lsb
%else
# Older RHEL (5)/ CentOS (5)
BuildRequires: redhat-release
%endif
%else
# Non redhat like distribution for example mandriva.
BuildRequires: lsb-release
%endif
%endif

%if 0%{?rhel_version} >= 600 || 0%{?centos_version} >= 600 || 0%{?fedora_version} >= 14
BuildRequires: tcp_wrappers-devel
%endif

Summary:  Bares All-In-One package (dir,sd,fd)
#BuildArch: noarch
Requires: %{name}-director = %{version}
Requires: %{name}-storage = %{version}
Requires: %{name}-client = %{version}

%define dscr Bareos - Backup Archiving Recovery Open Sourced.\
Bareos is a set of computer programs that permit you (or the system\
administrator) to manage backup, recovery, and verification of computer\
data across a network of computers of different kinds. In technical terms,\
it is a network client/server based backup program. Bareos is relatively\
easy to use and efficient, while offering many advanced storage management\
features that make it easy to find and recover lost or damaged files.\
Bareos source code has been released under the AGPL version 3 license.

%description
%{dscr}

# Notice : Don't try to change the order of package declaration
# You will have side effect with PreReq

%package bconsole
Summary:  Provide ncurses administration console
Group:    Productivity/Archiving/Backup
Requires: %{name}-common = %{version}

%package client
# contains bconsole, fd, libs
Summary:  Meta-All-In-One package client package
Group:    Productivity/Archiving/Backup
Requires:   %{name}-bconsole = %{version}
Requires:   %{name}-filedaemon = %{version}
%if 0%{?suse_version}
Recommends: %{name}-traymonitor = %{version}
%endif

%package director
Summary:  Provide bareos director daemon
Group:    Productivity/Archiving/Backup
Requires: %{name}-common = %{version}
Requires: %{name}-database-common = %{version}
Requires: %{name}-database-tools
%if 0%{?suse_version}
# Don't use this option on anything other then SUSE derived distributions
# as Fedora & others don't know this tag
Recommends: logrotate
%endif
Provides: %{name}-dir

%package storage
Summary:  Provide bareos storage daemon
Group:    Productivity/Archiving/Backup
Requires: %{name}-common = %{version}
Provides: %{name}-sd

%package storage-tape
Summary:  Provide bareos storage daemon tape support
Group:    Productivity/Archiving/Backup
Requires: mtx
%if !0%{?suse_version}
Requires: mt-st
%endif

%package filedaemon
Summary:  Provide bareos file daemon service
Group:    Productivity/Archiving/Backup
Requires: %{name}-common = %{version}
Provides: %{name}-fd

%package common
Summary:  Generic libs needed by every package
Group:    Productivity/Archiving/Backup
Provides: %{name}-libs

%package database-common
Summary:  Generic abstration lib for the sql catalog
Group:    Productivity/Archiving/Backup
Requires: %{name}-common = %{version}
Requires: %{name}-database-backend = %{version}
Requires: openssl
Provides: %{name}-sql

%package database-postgresql
Summary:  Libs & tools for postgresql catalog
Group:    Productivity/Archiving/Backup
Requires: %{name}-database-common = %{version}
Provides: %{name}-catalog-postgresql
Provides: %{name}-database-backend

%package database-mysql
Summary:  Libs & tools for mysql catalog
Group:    Productivity/Archiving/Backup
Requires: %{name}-database-common = %{version}
Provides: %{name}-catalog-mysql
Provides: %{name}-database-backend

%if 0%{?build_sqlite3}
%package database-sqlite3
Summary:  Libs & tools for sqlite3 catalog
Group:    Productivity/Archiving/Backup
%if 0%{?suse_version}
Requires: sqlite3
%endif
Requires: %{name}-database-common = %{version}
Provides: %{name}-catalog-sqlite3
Provides: %{name}-database-backend
%endif

%package database-tools
Summary:  Provides bareos-dbcheck, bscan
Group:    Productivity/Archiving/Backup
Requires: %{name}-common = %{version}, %{name}-database-common = %{version}
Provides: %{name}-dbtools

%package tools
Summary:  Provides bcopy, bextract, bls, bregex, bwild
Group:    Productivity/Archiving/Backup
Requires: %{name}-common = %{version}

%if 0%{build_qt_monitor}
%package traymonitor
Summary:  Qt based tray monitor
Group:    Productivity/Archiving/Backup
# Added to by pass the 09 checker rules (conflict with bareos-tray-monitor.conf)
# This is mostly wrong cause the two binaries can use it!
Conflicts:  %{name}-tray-monitor-gtk
Provides:   %{name}-tray-monitor-qt
%endif

%if 0%{?build_bat}
%package bat
Summary:  Provide Bareos Admin Tool gui
Group:    Productivity/Archiving/Backup
%endif

%package devel
Summary:  Devel headers
Group:    Development/Languages/C and C++
Requires: %{name}-common = %{version}
Requires: tcpd-devel
Requires: zlib-devel
Requires: libacl-devel
Requires: libmysqlclient-devel
Requires: postgresql-devel
%if 0%{?build_sqlite3}
%if 0%{?suse_version}
Requires: sqlite3-devel
%else
Requires: sqlite-devel
%endif
%endif
Requires: libopenssl-devel
Requires: libcap-devel

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

%description filedaemon
%{dscr}

This package contains the File Daemon
(Bareoss client daemon to read/write data from the backed up computer)

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

This package contains bareoss database tools.

%description tools
%{dscr}

This package contains bareoss tools.

%if 0%{?build_qt_monitor}
%description traymonitor
%{dscr}

This package contains the new tray monitor that uses qt.
%endif

%if 0%{?build_bat}
%description bat
%{dscr}

This package contains the Bareos Admin Tool (BAT).
Bat is a graphical interface for bareos.
%endif

%description devel
%{dscr}

This package contains bareos development files.

%prep
%setup

%build
%if %{undefined suse_version}
export PATH=$PATH:/usr/lib64/qt4/bin:/usr/lib/qt4/bin
%endif
export MTX=/usr/sbin/mtx
# Notice keep the upstream order of ./configure --help
%configure \
  --prefix=%{_prefix} \
  --sbindir=%{_sbindir} \
  --with-sbin-perm=755 \
  --sysconfdir=%{_sysconfdir}/bareos \
  --mandir=%{_mandir} \
  --docdir=%{_docdir}/%{name} \
  --htmldir=%{_docdir}/%{name}/html \
  --with-archivedir=/var/lib/bareos/storage \
  --with-scriptdir=%{script_dir} \
  --with-working-dir=%{working_dir} \
  --with-plugindir=%{plugin_dir} \
  --with-pid-dir=%{pid_dir} \
  --with-bsrdir=%{bsr_dir} \
  --with-logdir=/var/log/bareos \
  --with-subsys-dir=%{_subsysdir} \
  --enable-smartalloc \
  --disable-conio \
  --enable-readline \
  --enable-batch-insert \
  --enable-dynamic-cats-backends \
  --enable-scsi-crypto \
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



%install
%if 0%{?suse_version}
    # work-around for SLE_11
    #%%install -d 755 %%{buildroot}%%{_sysconfdir}/init.d
    %makeinstall DESTDIR=%{buildroot} install
%else
    #%%install -d 755 %%{buildroot}%%{_sysconfdir}/rc.d/init.d
    make DESTDIR=%{buildroot} install
%endif

install -d 755 %{buildroot}/usr/share/applications
install -d 755 %{buildroot}/usr/share/pixmaps

install -d 755 %{buildroot}%{working_dir}

#Cleaning
for F in  \
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

%if 0%{?build_bat}
%if 0%{?suse_version} > 1010
%suse_update_desktop_file -i bat System Utility Archiving
%endif
%endif

# install tray monitor
# %if 0%{?build_qt_monitor}
# %if 0%{?suse_version} > 1010
# disables, because suse_update_desktop_file complains
# that there are two desktop file (applications and autostart)
# ##suse_update_desktop_file bareos-tray-monitor System Backup
# %endif
# %endif

# install the sample-query.sql file as default query file
#install -m 644 examples/sample-query.sql %{buildroot}%{script_dir}/query.sql

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
%{_sbindir}/bconsole
%{_mandir}/man8/bconsole.8.gz

%files director
# dir package (bareos-dir, bsmtp)
%defattr(-, root, root)
%if 0%{?suse_version}
%{_sysconfdir}/init.d/bareos-dir
%{_sbindir}/rcbareos-dir
%if 0%{?install_suse_fw}
# use noreplace if user has adjusted its list of IP
%attr(0644, root, root) %config(noreplace) %{_fwdefdir}/bareos-dir
%endif
%else
%{_sysconfdir}/rc.d/init.d/bareos-dir
%endif
%attr(0640, %{director_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-dir.conf
%config(noreplace) %{_sysconfdir}/logrotate.d/%{name}-dir
%{plugin_dir}/*-dir.so
%{script_dir}/delete_catalog_backup
%{script_dir}/make_catalog_backup
%{script_dir}/make_catalog_backup.pl
%{_sbindir}/bareos-dir
%{_sbindir}/bsmtp
%dir %{_docdir}/%{name}
%{_mandir}/man1/bsmtp.1.gz
%{_mandir}/man8/bareos-dir.8.gz
%{_mandir}/man8/bareos.8.gz
%if 0%{?systemd_support}
%{_unitdir}/bareos-dir.service
%endif

# This is not a config file, but we need what %config is able to do
# the query.sql can be personalized by end user.
# a rpmlint rule is add to filter the warning
%config(noreplace) %{script_dir}/query.sql

%files storage
# sd package (bareos-sd, bls, btape, bcopy, bextract)
%defattr(-, root, root)
%attr(0640, %{storage_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-sd.conf
%if 0%{?suse_version}
%{_sysconfdir}/init.d/bareos-sd
%{_sbindir}/rcbareos-sd
%if 0%{?install_suse_fw}
# use noreplace if user has adjusted its list of IP
%attr(0644, root, root) %config(noreplace) %{_fwdefdir}/bareos-sd
%endif
%else
%{_sysconfdir}/rc.d/init.d/bareos-sd
%endif
%{_sbindir}/bscrypto
%{plugin_dir}/*-sd.so
%{script_dir}/disk-changer
%{_sbindir}/bareos-sd
%{_mandir}/man8/bscrypto.8.gz
%{_mandir}/man8/bareos-sd.8.gz
%if 0%{?systemd_support}
%{_unitdir}/bareos-sd.service
%endif
%attr(0775, %{storage_daemon_user}, %{daemon_group}) %dir /var/lib/bareos/storage

%files storage-tape
# tape specific files
%defattr(-, root, root)
%{script_dir}/mtx-changer
%config(noreplace) %{_sysconfdir}/bareos/mtx-changer.conf
%{_mandir}/man8/btape.8.gz
%{_sbindir}/btape

%files filedaemon
# fd package (bareos-fd, plugins)
%defattr(-, root, root)
%attr(0640, %{file_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-fd.conf
%if 0%{?suse_version}
%{_sysconfdir}/init.d/bareos-fd
%{_sbindir}/rcbareos-fd
%if 0%{?install_suse_fw}
# use noreplace if user has adjusted its list of IP
%attr(0644, root, root) %config(noreplace) %{_fwdefdir}/bareos-fd
%endif
%else
%{_sysconfdir}/rc.d/init.d/bareos-fd
%endif
%{_sbindir}/bareos-fd
%{plugin_dir}/*-fd.so
%{_mandir}/man8/bareos-fd.8.gz
# tray monitor
%if 0%{?systemd_support}
%{_unitdir}/bareos-fd.service
%endif

%files common
# common shared libraries (without db)
%defattr(-, root, root)
%attr(-, root, %{daemon_group}) %dir %{_sysconfdir}/bareos
%{_libdir}/libbareos-%{_libversion}.so
%{_libdir}/libbareos.so
%{_libdir}/libbareoscfg-%{_libversion}.so
%{_libdir}/libbareoscfg.so
%{_libdir}/libbareosfind-%{_libversion}.so
%{_libdir}/libbareosfind.so
%{_libdir}/libbareosndmp-%{_libversion}.so
%{_libdir}/libbareosndmp.so
# generic stuff needed from multiple bareos packages
%dir /usr/lib/bareos/
%dir %{script_dir}
%{script_dir}/bareos-config
%{script_dir}/bareos-config-lib.sh
%{script_dir}/btraceback.gdb
%if "%{_libdir}/bareos/" != "/usr/lib/bareos/"
%dir %{_libdir}/bareos/
%endif
%dir %{plugin_dir}
%{_sbindir}/btraceback
%{_mandir}/man8/btraceback.8.gz
%attr(0770, %{daemon_user}, %{daemon_group}) %dir %{working_dir}
%attr(0775, %{daemon_user}, %{daemon_group}) %dir /var/log/bareos


%files database-common
# catalog independent files
%defattr(-, root, root)
%dir %{script_dir}/ddl
%dir %{script_dir}/ddl/creates
%dir %{script_dir}/ddl/drops
%dir %{script_dir}/ddl/grants
%dir %{script_dir}/ddl/updates
%{_libdir}/libbareossql.so
%{_libdir}/libbareoscats.so
%{_libdir}/libbareossql-%{_libversion}.so
%{_libdir}/libbareoscats-%{_libversion}.so
%{script_dir}/create_bareos_database
%{script_dir}/drop_bareos_database
%{script_dir}/drop_bareos_tables
%{script_dir}/grant_bareos_privileges
%{script_dir}/make_bareos_tables
%{script_dir}/update_bareos_tables

%files database-postgresql
# postgresql catalog files
%defattr(-, root, root)
%{script_dir}/ddl/*/postgresql*.sql
%{_libdir}/libbareoscats-postgresql.so
%{_libdir}/libbareoscats-postgresql-%{_libversion}.so

%files database-mysql
# mysql catalog files
%defattr(-, root, root)
%{script_dir}/ddl/*/mysql*.sql
%{_libdir}/libbareoscats-mysql.so
%{_libdir}/libbareoscats-mysql-%{_libversion}.so

%if 0%{?build_sqlite3}
%files database-sqlite3
# sqlite3 catalog files
%defattr(-, root, root)
%{script_dir}/ddl/*/sqlite3*.sql
%{_libdir}/libbareoscats-sqlite3.so
%{_libdir}/libbareoscats-sqlite3-%{_libversion}.so
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
%{_sbindir}/bcopy
%{_sbindir}/bextract
%{_sbindir}/bls
%{_sbindir}/bregex
%{_sbindir}/bwild
%{_sbindir}/bpluginfo
%{_mandir}/man8/bcopy.8.gz
%{_mandir}/man8/bextract.8.gz
%{_mandir}/man8/bls.8.gz
%{_mandir}/man8/bwild.8.gz
%{_mandir}/man8/bregex.8.gz
%{_mandir}/man8/bpluginfo.8.gz

%if 0%{?build_qt_monitor}
%files traymonitor
%defattr(-,root, root)
%attr(-, root, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/tray-monitor.conf
%config %{_sysconfdir}/xdg/autostart/bareos-tray-monitor.desktop
%{_sbindir}/bareos-tray-monitor
%{_mandir}/man1/bareos-tray-monitor.1.gz
/usr/share/applications/bareos-tray-monitor.desktop
/usr/share/pixmaps/bareos-tray-monitor.xpm
%endif

%if 0%{?build_bat}
%files bat
%defattr(-, root, root)
%attr(-, root, %{daemon_group}) %{_sbindir}/bat
%attr(640, root, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bat.conf
%{_prefix}/share/pixmaps/bat.png
%{_prefix}/share/pixmaps/bat.svg
%{_prefix}/share/applications/bat.desktop
%{_mandir}/man1/bat.1.gz
%dir %{_docdir}/%{name}
%dir %{_docdir}/%{name}/html
%{_docdir}/%{name}/html/clients.html
%{_docdir}/%{name}/html/console.html
%{_docdir}/%{name}/html/filesets.html
%{_docdir}/%{name}/html/index.html
%{_docdir}/%{name}/html/joblist.html
%{_docdir}/%{name}/html/jobplot.html
%{_docdir}/%{name}/html/jobs.html
%{_docdir}/%{name}/html/restore.html
%{_docdir}/%{name}/html/status.png
%{_docdir}/%{name}/html/storage.html
%{_docdir}/%{name}/html/mail-message-new.png
%{_docdir}/%{name}/html/media.html
%endif

%files devel
%defattr(-, root, root)
/usr/include/bareos
%{_libdir}/*.la

#
# Define some macros for updating the system settings.
# SUSE has most macros by default others have not so we
# define the missing here.
#
%if 0%{?suse_version}
%define add_service_start() \
/bin/true \
%nil
%else
%if 0%{?systemd_support}
%define add_service_start() \
/bin/systemctl daemon-reload >/dev/null 2>&1 || true \
%nil
%define stop_on_removal() \
/bin/systemctl --no-reload disable %1.service > /dev/null 2>&1 || \
/bin/systemctl stop %1.service > /dev/null 2>&1 || true \
%nil
%define restart_on_update() \
/bin/systemctl try-restart %1.service >/dev/null 2>&1 || true \
%nil
%else
%define add_service_start() \
/sbin/chkconfig --add %1 \
%nil
%define stop_on_removal() \
/sbin/service %1 stop >/dev/null 2>&1 || \
/sbin/chkconfig --del %1 || true \
%nil
%define restart_on_update() \
/sbin/service %1 condrestart >/dev/null 2>&1 || true \
%nil
%endif
%define insserv_cleanup() \
/bin/true \
%nil
%endif

%define create_group() \
getent group %1 > /dev/null || groupadd -r %1 \
%nil

# shell: use /bin/false, because nologin has different paths on different distributions
%define create_user() \
getent passwd %1 > /dev/null || useradd -r --comment "%1" --home %{working_dir} -g %{daemon_group} --shell /bin/false %1 \
%nil

%post director
%{script_dir}/bareos-config initialize_local_hostname
%{script_dir}/bareos-config initialize_passwords
%{script_dir}/bareos-config initialize_database_driver
%add_service_start bareos-dir

%post storage
# pre script has already generated the storage daemon user,
# but here we add the user to additional groups
%{script_dir}/bareos-config setup_sd_user
%{script_dir}/bareos-config initialize_local_hostname
%{script_dir}/bareos-config initialize_passwords
%add_service_start bareos-sd

%post filedaemon
%{script_dir}/bareos-config initialize_local_hostname
%{script_dir}/bareos-config initialize_passwords
%add_service_start bareos-fd

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
if [ "$1" = 0 ]; then
%stop_on_removal bareos-dir
fi

%preun storage
if [ "$1" = 0 ]; then
%stop_on_removal bareos-sd
fi

%preun filedaemon
if [ "$1" = 0 ]; then
%stop_on_removal bareos-fd
fi

%postun director
if [ "$1" -ge "1" ]; then
%restart_on_update bareos-dir
fi
%insserv_cleanup

%postun storage
if [ "$1" -ge "1" ]; then
%restart_on_update bareos-sd
fi
%insserv_cleanup

%postun filedaemon
if [ "$1" -ge "1" ]; then
%restart_on_update bareos-fd
fi
%insserv_cleanup

%changelog
