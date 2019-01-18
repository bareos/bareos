# Determine Windows BITS (32/64) from name (mingw32-.../ming64-...)
%define WINDOWS_BITS %(echo %name | grep 64 >/dev/null 2>&1 && echo "64" || echo "32")


%define mingw mingw64
%define __strip %{_mingw64_strip}
%define __objdump %{_mingw64_objdump}
%define _use_internal_dependency_generator 0
#define __find_requires %%{_mingw64_findrequires}
%define __find_provides %{_mingw64_findprovides}
#define __os_install_post #{_mingw64_debug_install_post} \
#                          #{_mingw64_install_post}
%define bindir %{_mingw64_bindir}

# flavors:
#   If name contains debug, enable debug during build.
#   If name contains prevista, build for windows < vista.
%define flavors release debug
%define dirs_with_unittests lib findlib
%define bareos_configs bareos-dir.d/ bareos-fd.d/ bareos-sd.d/ tray-monitor.d/ bconsole.conf

Name:           %{mingw}-winbareos
Version:        0.0.0
Release:        0
Summary:        Bareos build for Windows
License:        LGPLv2+
Group:          Development/Libraries
URL:            http://bareos.org
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildArch:      noarch
#!BuildIgnore: post-build-checks
Source0:        bareos-%{version}.tar.bz2

%define addonsdir /bareos-addons/
BuildRequires:  bareos-addons

BuildRequires:  %{mingw}-filesystem
BuildRequires:  %{mingw}-cross-gcc
BuildRequires:  %{mingw}-cross-gcc-c++
BuildRequires:  %{mingw}-cross-binutils
BuildRequires:  %{mingw}-cross-pkg-config
BuildRequires:  %{mingw}-libqt5-qtbase
BuildRequires:  %{mingw}-libqt5-qtbase-devel
BuildRequires:  %{mingw}-cross-libqt5-qmake
BuildRequires:  %{mingw}-libwinpthread1
BuildRequires:  %{mingw}-winpthreads-devel
BuildRequires:  %{mingw}-libopenssl-devel
BuildRequires:  %{mingw}-libopenssl
BuildRequires:  %{mingw}-openssl
BuildRequires:  %{mingw}-libdb-devel
BuildRequires:  %{mingw}-libgcc
BuildRequires:  %{mingw}-libtool
BuildRequires:  %{mingw}-libgcrypt20
BuildRequires:  %{mingw}-cross-nsis
BuildRequires:  %{mingw}-gcc
BuildRequires:  %{mingw}-gcc-c++
BuildRequires:  %{mingw}-zlib-devel
BuildRequires:  %{mingw}-zlib
BuildRequires:  %{mingw}-libpng16-16
BuildRequires:  %{mingw}-libstdc++
BuildRequires:  %{mingw}-readline
BuildRequires:  %{mingw}-readline-devel
BuildRequires:  %{mingw}-lzo
BuildRequires:  %{mingw}-lzo-devel
BuildRequires:  %{mingw}-libfastlz
BuildRequires:  %{mingw}-libfastlz-devel
BuildRequires:  %{mingw}-libsqlite3-0
BuildRequires:  %{mingw}-libsqlite-devel
BuildRequires:  %{mingw}-gtest-devel
BuildRequires:  %{mingw}-libgtest0
BuildRequires:  %{mingw}-libjansson
BuildRequires:  %{mingw}-libjansson-devel

BuildRequires:  bc
BuildRequires:  less
BuildRequires:  procps
BuildRequires:  sed
BuildRequires:  vim
BuildRequires:  cmake

%description
Base package for Bareos Windows build.

%package release
Summary:        bareos
%description release
Bareos for Windows versions >= Windows Vista

%package debug
Summary:        bareos
%description debug
Bareos Debug for Windows versions >= Windows Vista


%prep
%setup -q -n bareos-%{version}

# unpack addons
for i in `ls %addonsdir`; do
   tar xvf %addonsdir/$i
done

for flavor in %flavors; do
   mkdir $flavor
done


%build

for flavor in %flavors; do

   #WINDOWS_BITS=$(echo %name | grep 64 >/dev/null 2>&1 && echo "64" || echo "32")
   WINDOWS_VERSION=$(echo $flavor | grep release >/dev/null && echo 0x600 || echo 0x500)
   pushd $flavor
   %{_mingw64_cmake} \
      -DCMAKE_INSTALL_BINDIR:PATH=%{_mingw64_bindir} \
      -Dsqlite3=yes \
      -Dpostgresql=yes \
      -Dtraymonitor=yes \
      -DWINDOWS_BITS=%WINDOWS_BITS \
      -DWINDOWS_VERSION=${WINDOWS_VERSION} \
      -Ddb_password=@db_password@ \
      -Ddb_port=@db_port@ \
      -Ddb_user=@db_user@ \
      -Ddir_password=@dir_password@ \
      -Dfd_password=@fd_password@ \
      -Dsd_password=@sd_password@ \
      -Dmon_dir_password=@mon_dir_password@ \
      -Dmon_fd_password=@mon_fd_password@ \
      -Dmon_sd_password=@mon_sd_password@ \
      -Dsd_password=@sd_password@ \
      -DVERSION_STRING=\"%version\" \
      ..

   make %{?jobs:-j%jobs} DESTDIR=%{buildroot}/${flavor}-%WINDOWS_BITS install

   popd

done


%install
#for flavor in %flavors; do
#   pushd $flavor
#   make DESTDIR=%{buildroot} install
#   popd
#done

%clean
rm -rf $RPM_BUILD_ROOT

%files


%files release
%defattr(-,root,root)
/release-%WINDOWS_BITS

%files debug
/debug-%WINDOWS_BITS

%changelog
