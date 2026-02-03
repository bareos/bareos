#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2013-2025 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation, which is
#   listed in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.
#
#  SPEC File for Bareos on AIX
#
Name: 		bareos
Version:    17.2.6
Release: 	0
Group: 		Productivity/Archiving/Backup
License: 	AGPL-3.0
BuildRoot: 	%{_tmppath}/%{name}-root
URL: 		http://www.bareos.org/
Vendor: 	The Bareos Team

%define library_dir    /opt/freeware/lib
%define backend_dir    /opt/freeware/lib/backends
%define plugin_dir     /opt/freeware/lib/plugins
%define script_dir     /opt/freeware/bareos/scripts
%define working_dir    /opt/freeware/var/bareos/working
%define bsr_dir        /opt/freeware/var/bareos/working

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
%define client_only 1
%define build_qt_monitor 0
%define check_cmocka 1
%define objectstorage 0
%define have_git 0
%define ceph 0
%define install_suse_fw 0
%define systemd 0
%define python_plugins 0


Source0: %{name}-%{version}.tar.gz

BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: zlib-devel
BuildRequires: coreutils


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

# Notice : Don't try to change the order of package declaration
# You will have side effect with PreReq
%package    filedaemon
Summary:    Bareos File daemon (backup and restore client)
Group:      Productivity/Archiving/Backup
Requires:   %{name}-common = %{version}
Provides:   %{name}-fd


%package    common
Summary:    Common files, required by multiple Bareos packages
Group:      Productivity/Archiving/Backup
Provides:   %{name}-libs
Requires:   libstdc++
Requires:   libgcc
Requires:   zlib

%description filedaemon
%{dscr}

This package contains the File Daemon
(Bareos client daemon to read/write data from the backed up computer)

%description common
%{dscr}

This package contains the shared libraries that are used by multiple daemons and tools.

%prep
%setup

%build
# Cleanup defined in Fedora Packaging:Guidelines
# and required repetitive local build of at least CentOS 5.
if [ "%{?buildroot}" -a "%{?buildroot}" != "/" ]; then
    rm -rf "%{?buildroot}"
fi

export MTX=/usr/sbin/mtx
export PATH=/opt/freeware/bin:/usr/bin:/etc:/usr/sbin:/usr/ucb:/usr/bin/X11:
export CFLAGS="-DSYSV -D_AIX -D_AIX32 -D_AIX41 -D_AIX43 -D_AIX51 -D_AIX52 -D_AIX53 -D_AIX61 -D_AIX71 -D_ALL_SOURCE -DFUNCPROTO=15 -O -I/opt/freeware/include"
export CC=gcc
export CXX=g++
export CXXFLAGS=$CFLAGS
export F77=g77
export FFLAGS="-O -I/opt/freeware/include"
export LD=ld
export LDFLAGS="-L/usr/lib/threads -L/opt/freeware/lib -Wl,-blibpath:/opt/freeware/lib:/opt/freeware/bareos/lib:/usr/lib:/lib -Wl,-bbigtoc -Wl,-bmaxdata:0x80000000 -Wl,-brtl"

# Notice keep the upstream order of ./configure --help

#configure --enable-client-only --enable-includes
#./configure --enable-client-only

./configure  \
  --program-prefix=  \
  --prefix=/opt/freeware  \
  --exec-prefix=/opt/freeware  \
  --bindir=/opt/freeware/bin  \
  --sbindir=/opt/freeware/sbin  \
  --sysconfdir=/etc  \
  --datadir=/opt/freeware/share  \
  --includedir=/opt/freeware/include  \
  --libdir=/opt/freeware/lib  \
  --libexecdir=/opt/freeware/libexec  \
  --localstatedir=/opt/freeware/var  \
  --sharedstatedir=/opt/freeware/com  \
  --mandir=/opt/freeware/share/man  \
  --infodir=/opt/freeware/share/info  \
  --enable-client-only  \
  --enable-includes


#Add flags
%__make CFLAGS="$RPM_OPT_FLAGS" CXXFLAGS="$RPM_OPT_FLAGS" %{?_smp_mflags};

%check

%install
%if 0%{?suse_version}
    %makeinstall DESTDIR=%{buildroot} install
%else
    make DESTDIR=%{buildroot} install
%endif
make DESTDIR=%{buildroot} install-autostart

mkdir -p -m 755 %{buildroot}/usr/share/applications
mkdir -p -m 755 %{buildroot}/usr/share/pixmaps
mkdir -p -m 755 %{buildroot}%{backend_dir}
mkdir -p -m 755 %{buildroot}%{working_dir}
mkdir -p -m 755 %{buildroot}%{plugin_dir}
mkdir -p -m 755 %{buildroot}/var/log/bareos
mkdir -p -m 755 %{buildroot}/usr/lib/bareos

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
    %{script_dir}/btraceback.dbx \
    %{script_dir}/btraceback.mdb \
    %{_sbindir}/%{name}
do
rm -f "%{buildroot}/$F"
done

ls -la %{buildroot}/%{library_dir}


%files
%defattr(-, root, root)

%files filedaemon
# fd package (bareos-fd, plugins)
%defattr(-, root, root)
%attr(0750, %{file_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-fd.d/
%attr(0750, %{file_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-fd.d/client
%attr(0750, %{file_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-fd.d/director
%attr(0750, %{file_daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/bareos-fd.d/messages
%attr(0640, %{file_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-fd.d/client/myself.conf
%attr(0640, %{file_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-fd.d/director/bareos-dir.conf
%attr(0640, %{file_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-fd.d/director/bareos-mon.conf
%attr(0640, %{file_daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/bareos-fd.d/messages/Standard.conf
%if 0%{?build_qt_monitor}
%attr(0755, %{daemon_user}, %{daemon_group}) %dir %{_sysconfdir}/bareos/tray-monitor.d/client
%attr(0644, %{daemon_user}, %{daemon_group}) %config(noreplace) %{_sysconfdir}/bareos/tray-monitor.d/client/FileDaemon-local.conf
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
#{plugin_dir}/bpipe-fd.so
%{_mandir}/man8/bareos-fd.8.gz
# tray monitor
%if 0%{?systemd_support}
%{_unitdir}/bareos-fd.service
%endif

%files common
# common shared libraries (without db)
%defattr(-, root, root)
%attr(0755, root, %{daemon_group})           %dir %{_sysconfdir}/bareos
# generic stuff needed from multiple bareos packages
%dir %{library_dir}
%{library_dir}/libbareos*.so
#if "%{_libdir}" != "/usr/lib/"
#dir %{_libdir}/bareos/
#endif
%dir %{plugin_dir}
%{_sbindir}/btraceback
%{_mandir}/man8/btraceback.8.gz
%attr(0770, %{daemon_user}, %{daemon_group}) %dir %{working_dir}
%attr(0775, %{daemon_user}, %{daemon_group}) %dir /var/log/bareos


%define create_group() \
lsgroup %1 > /dev/null || mkgroup %1 \
%nil

# shell: use /bin/false, because nologin has different paths on different distributions
%define create_user() \
lsuser %1 > /dev/null || mkuser gecos="%1" home=%{working_dir} groups=%{daemon_group} %1 \
%nil


%post filedaemon


%post common

%pre filedaemon
%create_group %{daemon_group}
%create_user  %{storage_daemon_user}
exit 0

%pre common
%create_group %{daemon_group}
%create_user  %{daemon_user}
exit 0

%preun filedaemon
/etc/rc.d/init.d/bareos-fd stop


%postun filedaemon

%changelog
