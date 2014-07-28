# If versionstring contains debug, enable debug during build
%define WIN_DEBUG %(echo %version | grep debug >/dev/null 2>&1 && echo "yes" || echo "no")

# Determine Windows Version (32/64) from name (mingw32-.../ming64-...)
%define WIN_VERSION %(echo %name | grep 64 >/dev/null 2>&1 && echo "64" || echo "32")

# Set what to build
%define BUILD_QTGUI yes
%define WIN_VISTACOMPAT yes

%define __strip %{_mingw32_strip}
%define __objdump %{_mingw32_objdump}
%define _use_internal_dependency_generator 0
#define __find_requires %{_mingw32_findrequires}
%define __find_provides %{_mingw32_findprovides}
#define __os_install_post #{_mingw32_debug_install_post} \
#                          #{_mingw32_install_post}


Name:           mingw32-winbareos
Version:        14.2.0
Release:        0
Summary:        bareos
License:        LGPLv2+
Group:          Development/Libraries
URL:            http://bareos.org
Source0:        bareos-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildArch:      noarch
#!BuildIgnore: post-build-checks

Source1:       fillup.sed
Source2:       vss_headers.tar
Source3:       vdi_headers.tar
Source4:       pgsql-libpq.tar

Patch1:        tray-monitor-conf.patch
Patch2:        tray-monitor-conf-fd-sd.patch

BuildRequires:  mingw32-filesystem
BuildRequires:  mingw32-cross-gcc
BuildRequires:  mingw32-cross-gcc-c++
BuildRequires:  mingw32-cross-binutils
BuildRequires:  mingw32-cross-pkg-config
BuildRequires:  mingw32-libqt4-devel
BuildRequires:  mingw32-libqt4
BuildRequires:  mingw32-pthreads-devel
BuildRequires:  mingw32-pthreads
BuildRequires:  mingw32-libopenssl-devel
BuildRequires:  mingw32-libopenssl
BuildRequires:  mingw32-openssl
BuildRequires:  mingw32-libdb-devel
BuildRequires:  mingw32-libgcc
BuildRequires:  mingw32-libtool

#BuildRequires:  mingw32-sed
BuildRequires:  mingw32-libgcrypt
BuildRequires:  mingw32-cross-nsis
BuildRequires:  mingw32-gcc
BuildRequires:  mingw32-gcc-c++
BuildRequires:  mingw32-pthreads-devel
BuildRequires:  mingw32-zlib-devel
BuildRequires:  mingw32-zlib
BuildRequires:  mingw32-libpng
BuildRequires:  mingw32-libstdc++
BuildRequires:  mingw32-readline
BuildRequires:  mingw32-readline-devel
BuildRequires:  mingw32-lzo
BuildRequires:  mingw32-lzo-devel
BuildRequires:  mingw32-libfastlz
BuildRequires:  mingw32-libfastlz-devel
BuildRequires:  mingw32-libsqlite
BuildRequires:  mingw32-libsqlite-devel

BuildRequires:  sed
BuildRequires:  vim, procps, bc

%description
bareos

%package devel
Summary:        bareos
Group:          Development/Libraries


%description devel
bareos

#{_mingw32_debug_package}

%prep
#setup
%setup -q -n bareos-%{version}
cp src/qt-tray-monitor/tray-monitor.conf.in src/qt-tray-monitor/tray-monitor.conf.in.orig
cp src/qt-tray-monitor/tray-monitor.conf.in src/qt-tray-monitor/tray-monitor.fd-sd-dir.conf.in
%patch1 -p1
mv src/qt-tray-monitor/tray-monitor.fd-sd-dir.conf.in src/qt-tray-monitor/tray-monitor.fd.conf.in
cp src/qt-tray-monitor/tray-monitor.conf.in.orig src/qt-tray-monitor/tray-monitor.fd-sd-dir.conf.in
%patch2 -p1
mv src/qt-tray-monitor/tray-monitor.fd-sd-dir.conf.in src/qt-tray-monitor/tray-monitor.fd-sd.conf.in
cp src/qt-tray-monitor/tray-monitor.conf.in.orig src/qt-tray-monitor/tray-monitor.fd-sd-dir.conf.in

tar xvf %SOURCE2
tar xvf %SOURCE3
tar xvf %SOURCE4

%build

cd src/win32/
make WIN_DEBUG=%{WIN_DEBUG} BUILD_QTGUI=%{BUILD_QTGUI} WIN_VERSION=%{WIN_VERSION} WIN_VISTACOMPAT=%{WIN_VISTACOMPAT} %{?jobs:-j%jobs}

%install

mkdir -p $RPM_BUILD_ROOT%{_mingw32_bindir}

mkdir -p $RPM_BUILD_ROOT/etc/%name

pushd src/win32

cp qt-tray-monitor/bareos-tray-monitor.exe \
   qt-console/bat.exe \
   console/bconsole.exe \
   filed/bareos-fd.exe \
   stored/bareos-sd.exe \
   stored/btape.exe \
   stored/bls.exe \
   stored/bextract.exe \
   dird/bareos-dir.exe \
   dird/dbcheck.exe \
   tools/bsmtp.exe \
   stored/libbareossd*.dll \
   cats/libbareoscats*.dll \
   lib/libbareos.dll \
   findlib/libbareosfind.dll \
   lmdb/libbareoslmdb.dll \
   plugins/filed/bpipe-fd.dll \
   plugins/filed/mssqlvdi-fd.dll \
   plugins/stored/autoxflate-sd.dll \
   $RPM_BUILD_ROOT%{_mingw32_bindir}

for cfg in  ../qt-tray-monitor/tray-monitor.fd.conf.in \
   ../qt-tray-monitor/tray-monitor.fd-sd.conf.in \
   ../qt-tray-monitor/tray-monitor.fd-sd-dir.conf.in \
   ../qt-console/bat.conf.in \
   ../dird/bareos-dir.conf.in \
   ../filed/bareos-fd.conf.in \
   ../stored/bareos-sd.conf.in \
   ../console/bconsole.conf.in \
   ; do cp $cfg $RPM_BUILD_ROOT/etc/%name/$(basename $cfg .in)
done

for cfg in $RPM_BUILD_ROOT/etc/%name/*.conf;
do
  sed -f %SOURCE1 $cfg -i ;
done

popd

mkdir -p  $RPM_BUILD_ROOT/etc/%name/ddl
for i in creates drops grants updates; do
   mkdir $RPM_BUILD_ROOT/etc/%name/ddl/$i/
   cp -av src/cats/ddl/$i/postgres* $RPM_BUILD_ROOT/etc/%name/ddl/$i/
done

for sql in $RPM_BUILD_ROOT/etc/%name/ddl/*/*.sql;
do
   sed -f %SOURCE1 $sql -i ;
done



%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
/etc/%name/*.conf
/etc/%name/ddl/
%dir %{_mingw32_bindir}
%{_mingw32_bindir}/*.dll
%{_mingw32_bindir}/*.exe

%changelog
