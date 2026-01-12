#
# spec file for bareos univeral client
# Copyright (c) 2011-2012 Bruno Friedmann (Ioda-Net) and Philipp Storz (dass IT)
#               2013-2026 Bareos GmbH & Co KG
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
%define configtemplatedir %{_prefix}/lib/%{name}/defaultconfigs
%define library_dir       %{_libdir}/%{name}
%define backend_dir       %{_libdir}/%{name}/backends
%define plugin_dir        %{_libdir}/%{name}/plugins
%define script_dir        %{_prefix}/lib/%{name}/scripts
%define working_dir       %{_sharedstatedir}/%{name}
%define log_dir           /var/log/%{name}
%define bsr_dir           %{_sharedstatedir}/%{name}

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
%define systemd_support 1
%define build_qt_monitor 0

# cmake build directory
%define CMAKE_BUILDDIR       cmake-build


# use modernized GCC 14 toolchain for C++20 support
%if 0%{?rhel} && 0%{?rhel} <= 9
BuildRequires: gcc-toolset-14-gcc
BuildRequires: gcc-toolset-14-annobin-plugin-gcc
BuildRequires: gcc-toolset-14-gcc-c++
%define glusterfs 1
%define enable_grpc 0
%endif


%if 0%{?suse_version} && 0%{?suse_version} <= 1599
BuildRequires: gcc15
BuildRequires: gcc15-c++
%endif

%if 0%{?fedora} || 0%{?suse_version}
BuildRequires: fmt-devel
%endif

BuildRequires: systemd
# see https://en.opensuse.org/openSUSE:Systemd_packaging_guidelines
%if 0%{?suse_version}
BuildRequires: systemd-rpm-macros
%endif
%{?systemd_requires}

Source0: %{name}-%{version}.tar.gz

BuildRequires: cmake >= 3.17
BuildRequires: gcc
BuildRequires: gcc-c++
BuildRequires: git-core
BuildRequires: glibc
BuildRequires: glibc-devel
BuildRequires: libacl-devel
BuildRequires: libcap-devel
BuildRequires: libstdc++-static
BuildRequires: logrotate
BuildRequires: lzo-devel
BuildRequires: make
BuildRequires: mtx
BuildRequires: ncurses-devel
BuildRequires: openssl
BuildRequires: openssl-devel
BuildRequires: pam-devel
BuildRequires: pkgconfig
BuildRequires: pkgconfig(jansson)
BuildRequires: pkgconfig(json-c)
BuildRequires: pkgconfig(libxml-2.0)
BuildRequires: postgresql-devel
BuildRequires: python-rpm-macros
BuildRequires: readline-devel
BuildRequires: zlib-devel


%if 0%{?suse_version}

# suse_version:
#   1600: SLE_16
#   1500: SLE_15

BuildRequires: distribution-release
BuildRequires: shadow
BuildRequires: update-desktop-files

# link identical files
BuildRequires: fdupes
BuildRequires: lsb-release

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

%else
# non suse, non redhat: eg. mandriva.
BuildRequires: lsb-release

%endif

%endif

# dependency tricks for vixdisklib
%global __requires_exclude ^(.*libvixDiskLib.*|.*CXXABI_1.3.9.*)$


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

%if 0%{?suse_version} && 0%{?suse_version} < 1600
%debug_package
%endif

# Notice : Don't try to change the order of package declaration
# You will have side effect with PreReq

%package       universal-client
Summary:       Bareos File daemon (backup and restore client)
Group:         Productivity/Archiving/Backup
Requires(pre): /usr/sbin/useradd
Requires(pre): /usr/sbin/groupadd
Requires(pre): coreutils
Requires(pre): findutils
Requires(pre): gawk
Requires(pre): grep
Requires(pre): openssl
Requires(pre): sed
Provides:      %{name}-libs
Provides:      %{name}-fd
Provides:      user(%{daemon_user})
Provides:      group(%{daemon_group})



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

# use modern compiler on suse 15
%if 0%{?suse_version} && 0%{?suse_version} <= 1599
CC=gcc-15  ; export CC
CXX=g++-15 ; export CXX
%endif

CFLAGS="${CFLAGS:-%optflags}" ; export CFLAGS ;
CXXFLAGS="${CXXFLAGS:-%optflags}" ; export CXXFLAGS ;

# use our own cmake call instead of cmake macro as it is different on different platforms/versions
cmake  .. \
  -DCMAKE_VERBOSE_MAKEFILE=ON \
  -DCMAKE_INSTALL_PREFIX:PATH=%{_prefix} \
  -DCMAKE_INSTALL_LIBDIR:PATH=%{_prefix}/lib \
  -DINCLUDE_INSTALL_DIR:PATH=%{_includedir} \
  -DLIB_INSTALL_DIR:PATH=%{_prefix}/lib \
  -DSYSCONF_INSTALL_DIR:PATH=%{_sysconfdir} \
  -DSHARE_INSTALL_PREFIX:PATH=%{_datadir} \
  -DBUILD_SHARED_LIBS:BOOL=ON \
  -Dprefix=%{_prefix}\
  -Dlibdir=%{library_dir} \
  -Dsbindir=%{_sbindir} \
  -Dsysconfdir=%{_sysconfdir} \
  -Dconfdir=%{confdir} \
  -Dmandir=%{_mandir} \
  -Darchivedir=%{_sharedstatedir}/%{name}/storage \
  -Dbackenddir=%{backend_dir} \
  -Dconfigtemplatedir=%{configtemplatedir} \
  -Dscriptdir=%{script_dir} \
  -Dworkingdir=%{working_dir} \
  -Dplugindir=%{plugin_dir} \
  -Dlogdir=%{log_dir} \
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
  -Dsystemd=yes \
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
install -d -m 750 %{buildroot}%{confdir}

# remove links to libraries
find %{buildroot}/%{library_dir} -type l -name "libbareos*.so" -maxdepth 1 -exec rm {} \;
ls -la %{buildroot}/%{library_dir}

# install systemd service files
install -d -m 755 %{buildroot}%{_unitdir}
install -m 644 %{CMAKE_BUILDDIR}/core/platforms/systemd/bareos-fd.service %{buildroot}%{_unitdir}
%if 0%{?suse_version}
ln -sf service %{buildroot}%{_sbindir}/rcbareos-fd
%endif

%files universal-client
# fd package (bareos-fd, plugins)
%defattr(-, root, root)
%if 0%{?suse_version}
%{_sbindir}/rcbareos-fd
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


# common shared libraries (without db)
%defattr(-, root, root)
%attr(2755, root, %{daemon_group}) %dir %{_sysconfdir}/%{name}
%attr(2755, root, %{daemon_group}) %dir %{configtemplatedir}
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
%endif
%dir %{backend_dir}
%{library_dir}/libbareosfastlz.so*
%{library_dir}/libbareos.so*
%{library_dir}/libbareosfind.so*
%{library_dir}/libbareoslmdb.so*
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
%{_sbindir}/btraceback
%{_mandir}/man8/btraceback.8.gz
%attr(0770, %{daemon_user}, %{daemon_group}) %dir %{working_dir}
%attr(0775, %{daemon_user}, %{daemon_group}) %dir /var/log/%{name}
%license AGPL-3.0.txt LICENSE.txt
%doc core/README.*



# check if LOGFILE is writable,
# to prevent failures on immutable systems (eg. Fedora Silverblue).
# In that case, output to stdout.
%define logging_start() \
timestamp() { \
  date "+%%F %%R" \
} \
COMPONENT=%1 \
SECTION=%2 \
LOGFILE="%{log_dir}/%{name}-${COMPONENT}-install.log" \
mkdir -p "%{log_dir}" || true \
if touch "${LOGFILE}"; then \
  exec >>"${LOGFILE}" 2>&1 \
fi \
echo "[$(timestamp)] %{name}-${COMPONENT} %{version} %%${SECTION}: start" \
if [ "${BAREOS_INSTALL_DEBUG}" ]; then \
  set -x \
fi \
%nil

%define logging_end() \
echo "[$(timestamp)] %{name}-${COMPONENT} %{version} %%${SECTION}: done" \
%nil

%define create_group() \
getent group %1 > /dev/null || groupadd -r %1 \
%nil

# shell: use /bin/false, because nologin has different paths on different distributions
%define create_user() \
getent passwd %1 > /dev/null || useradd -r --comment "%1" --home-dir %{working_dir} -g %{daemon_group} --shell /bin/false %1 \
%nil

# NOTE: rpm macro with parameter. Rest of line is ignored (taken as parameter).
# usage:
# if %%rpm_version_lt <version1> <version2>
# then
#   <do something>
# fi
%define rpm_version_lt() \
[ "%1" ] && [ "%2" ] && [ "$(printf "%1\\n%2\\n" | sort --version-sort | head -n 1)" = "%1" ] \
%nil

# usage:
# %%if_package_version_lt <packagename> <version>
#   <do something>
# fi
%define if_package_version_lt() \
OLDVER=$(rpm -q %1 --qf "%%{version}"); \
if %rpm_version_lt $OLDVER %2 \
then \
%nil

# With the introduction of config subdirectories (bareos-16.2)
# some config files have been renamed (or even splitted into multiple files).
# However, bareos is still able to work with the old config files,
# but rpm renames them to *.rpmsave.
# To keep the bareos working after updating to bareos-16.2,
# we implement a workaround:
#   * pre (or post): if the old config exists, make a copy of it.
#   * (rpm exchanges files on disk)
#   * posttrans:
#       if the old config file don't exists but we have created a backup before,
#       restore the old config file.
#       Remove our backup, if it exists.

%define pre_backup_file() \
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

#
# pre, post and posttrans scriplets
#


%pre universal-client
%logging_start universal-client pre
if [ $1 -gt 1 ]; then
  # upgrade
  OLDVER=$(rpm -q %{name}-universal-client --qf "%%{version}")
  if %rpm_version_lt $OLDVER 16.2.0
  then
    %pre_backup_file /etc/%{name}/bareos-fd.conf
  elif %rpm_version_lt $OLDVER 25.0.0
  then
    # The postun of the old package will try to restart the service.
    # However, it will fail as the required config files
    # will only be restored in posttrans.
    # Therefore we create this marker and restart the service in posttrans.
    if /bin/systemctl --quiet is-active bareos-fd.service; then
      mkdir -p "%{_localstatedir}/lib/rpm-state/bareos/restart/"
      touch "%{_localstatedir}/lib/rpm-state/bareos/restart/bareos-fd.service"
    fi
    %pre_backup_file "%{_sysconfdir}/%{name}/bareos-fd.d/client/myself.conf"
    %pre_backup_file "%{_sysconfdir}/%{name}/bareos-fd.d/director/bareos-dir.conf"
    %pre_backup_file "%{_sysconfdir}/%{name}/bareos-fd.d/director/bareos-mon.conf"
    %pre_backup_file "%{_sysconfdir}/%{name}/bareos-fd.d/messages/Standard.conf"
    %pre_backup_file "%{_sysconfdir}/%{name}/tray-monitor.d/client/FileDaemon-local.conf"
  fi
fi
%create_group %{daemon_group}
%create_user  %{storage_daemon_user}
/sbin/ldconfig
%logging_end
exit 0

%post universal-client
%logging_start universal-client post
%{script_dir}/bareos-config deploy_config "bareos-fd"
%service_add_post bareos-fd.service
/bin/systemctl enable --now bareos-fd.service >/dev/null 2>&1 || true
/sbin/ldconfig
%logging_end

%preun universal-client
%logging_start universal-client preun
%service_del_preun bareos-fd.service
%logging_end

%postun universal-client
%logging_start universal-client postun
%service_del_postun bareos-fd.service
%logging_end

%posttrans universal-client
%logging_start universal-client posttrans
# update from bareos < 16.2
%posttrans_restore_file /etc/%{name}/bareos-fd.conf
# update from bareos < 25
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-fd.d/client/myself.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-fd.d/director/bareos-dir.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-fd.d/director/bareos-mon.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/bareos-fd.d/messages/Standard.conf"
%posttrans_restore_file "%{_sysconfdir}/%{name}/tray-monitor.d/client/FileDaemon-local.conf"
if [ -e "%{_localstatedir}/lib/rpm-state/bareos/restart/bareos-fd.service" ]; then
  if ! /bin/systemctl --quiet is-active bareos-fd.service; then
    /bin/systemctl start bareos-fd.service >/dev/null 2>&1 || true
  fi
  rm "%{_localstatedir}/lib/rpm-state/bareos/restart/bareos-fd.service"
fi
%logging_end

%changelog
