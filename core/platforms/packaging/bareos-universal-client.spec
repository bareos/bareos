#
# spec file for bareos univeral client
# Copyright (c) 2011-2012 Bruno Friedmann (Ioda-Net) and Philipp Storz (dass IT)
#               2013-2024 Bareos GmbH & Co KG
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

%define library_dir    %{_libdir}/%{name}
%define backend_dir    %{_libdir}/%{name}/backends
%define plugin_dir     %{_libdir}/%{name}/plugins
%define script_dir     /usr/lib/%{name}/scripts
%define working_dir    /var/lib/%{name}
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
%define client_only 1
%define have_git 1
%define systemd_support 1
# cmake build directory
%define CMAKE_BUILDDIR       cmake-build

BuildRequires: libstdc++-static

%if 0%{?systemd_support}
BuildRequires: systemd
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

# use modernized GCC 14 toolchain for C++20 support
%if 0%{?rhel} && 0%{?rhel} <= 9
BuildRequires: gcc-toolset-14-gcc
BuildRequires: gcc-toolset-14-annobin-plugin-gcc
BuildRequires: gcc-toolset-14-gcc-c++
%endif

%if 0%{?suse_version}

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


%if 0%{?opensuse_version} || 0%{?sle_version}
%debug_package
%endif

# Notice : Don't try to change the order of package declaration
# You will have side effect with PreReq

%package    universal-client
Summary:    Bareos File daemon (backup and restore client)
Group:      Productivity/Archiving/Backup
Provides:   %{name}-fd
Requires(pre): /usr/sbin/useradd
Requires(pre): /usr/sbin/groupadd
Requires(pre): coreutils
Requires(pre): findutils
Requires(pre): gawk
Requires(pre): grep
Requires(pre): openssl
Requires(pre): sed
Requires(pre): /usr/sbin/useradd
Requires(pre): /usr/sbin/groupadd
Provides:   %{name}-libs

%description universal-client
%{dscr}

This package contains the File Daemon
(Bareos client daemon to read/write data from the backed up computer)

%prep
# this is a hack so we always build in "bareos" and not in "bareos-version"
%setup -c -n bareos
[ -d bareos-* ] && mv bareos-*/* . || :


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
%if 0%{?sle_version} == 150400
CC=gcc-11  ; export CC
CXX=g++-11 ; export CXX
%else
  %if 0%{?sle_version} == 150300 || 0%{?suse_version} > 1500
CC=gcc-10  ; export CC
CXX=g++-10 ; export CXX
  %else
    %if 0%{?suse_version}
CC=gcc-9  ; export CC
CXX=g++-9 ; export CXX
    %endif
  %endif
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
  -Dconfdir=%{_sysconfdir}/bareos \
  -Dmandir=%{_mandir} \
  -Darchivedir=/var/lib/%{name}/storage \
  -Dbackenddir=%{backend_dir} \
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
  -Dwebuiconfdir=%{_sysconfdir}/bareos-webui \
  -DVERSION_STRING=%version \
  -DBUILD_UNIVERSAL_CLIENT=yes
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

# remove links to libraries
find %{buildroot}/%{library_dir} -type l -name "libbareos*.so" -maxdepth 1 -exec rm {} \;
ls -la %{buildroot}/%{library_dir}

# install systemd service files
%if 0%{?systemd_support}
install -d -m 755 %{buildroot}%{_unitdir}
install -m 644 %{CMAKE_BUILDDIR}/core/platforms/systemd/bareos-fd.service %{buildroot}%{_unitdir}
%if !0%{?client_only}
install -m 644 %{CMAKE_BUILDDIR}/core/platforms/systemd/bareos-dir.service %{buildroot}%{_unitdir}
install -m 644 %{CMAKE_BUILDDIR}/core/platforms/systemd/bareos-sd.service %{buildroot}%{_unitdir}
%endif
%if 0%{?suse_version}
ln -sf service %{buildroot}%{_sbindir}/rcbareos-dir
ln -sf service %{buildroot}%{_sbindir}/rcbareos-fd
ln -sf service %{buildroot}%{_sbindir}/rcbareos-sd
%endif
%endif

%files universal-client
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

%define post_scsicrypto() \
if [ -f "%{_sysconfdir}/%{name}/.enable-cap_sys_rawio" ]; then \
   %{script_dir}/bareos-config set_scsicrypto_capabilities; \
fi\
%nil

%post universal-client
%post_backup_file /etc/%{name}/bareos-fd.conf
%{script_dir}/bareos-config initialize_local_hostname
%{script_dir}/bareos-config initialize_passwords
%if 0%{?suse_version} >= 1210
%service_add_post bareos-fd.service
/bin/systemctl enable bareos-fd.service >/dev/null 2>&1 || true
%else
%add_service_start bareos-fd
%endif
/sbin/ldconfig


%posttrans universal-client
%posttrans_restore_file /etc/%{name}/bareos-fd.conf

%pre universal-client
%create_group %{daemon_group}
%create_user  %{storage_daemon_user}
exit 0


%preun universal-client
%if 0%{?suse_version} >= 1210
%service_del_preun bareos-fd.service
%else
%stop_on_removal bareos-fd
%endif

%postun universal-client
%if 0%{?suse_version} >= 1210
%service_del_postun bareos-fd.service
%else
%restart_on_update bareos-fd
%endif
%insserv_cleanup
/sbin/ldconfig

%changelog
