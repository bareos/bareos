# If name contains debug, enable debug during build
%define WIN_DEBUG %(echo %name | grep debug >/dev/null 2>&1 && echo "yes" || echo "no")

# If name contains prevista, build for windows < vista
%define WIN_VISTACOMPAT %(echo %name | grep prevista >/dev/null 2>&1 && echo "no" || echo "yes")

# Determine Windows Version (32/64) from name (mingw32-.../ming64-...)
%define WIN_VERSION %(echo %name | grep 64 >/dev/null 2>&1 && echo "64" || echo "32")

# Set what to build
%define BUILD_QTGUI yes

%define __strip %{_mingw64_strip}
%define __objdump %{_mingw64_objdump}
%define _use_internal_dependency_generator 0
#define __find_requires %{_mingw64_findrequires}
%define __find_provides %{_mingw64_findprovides}
#define __os_install_post #{_mingw64_debug_install_post} \
#                          #{_mingw64_install_post}


%define flavors "prevista postvista prevista-debug postvista-debug"

Name:           mingw64-winbareos
Version:        14.2.7
Release:        0
Summary:        bareos
License:        LGPLv2+
Group:          Development/Libraries
URL:            http://bareos.org
Source0:        bareos-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildArch:      noarch
#!BuildIgnore: post-build-checks

%define addonsdir /bareos-addons/
BuildRequires:  bareos-addons

%define SIGNCERT ia.p12
%define SIGNPWFILE signpassword


Source1:        fillup.sed
Patch1:         tray-monitor-conf.patch
Patch2:         tray-monitor-conf-fd-sd.patch

BuildRequires:  mingw64-filesystem
BuildRequires:  mingw64-cross-gcc
BuildRequires:  mingw64-cross-gcc-c++
BuildRequires:  mingw64-cross-binutils
BuildRequires:  mingw64-cross-pkg-config
BuildRequires:  mingw64-libqt4-devel
BuildRequires:  mingw64-libqt4
BuildRequires:  mingw64-pthreads-devel
BuildRequires:  mingw64-pthreads
BuildRequires:  mingw64-libopenssl-devel
BuildRequires:  mingw64-libopenssl
BuildRequires:  mingw64-openssl
BuildRequires:  mingw64-libdb-devel
BuildRequires:  mingw64-libgcc
BuildRequires:  mingw64-libtool

#BuildRequires:  mingw64-sed
BuildRequires:  mingw64-libgcrypt20
BuildRequires:  mingw64-cross-nsis
BuildRequires:  mingw64-gcc
BuildRequires:  mingw64-gcc-c++
BuildRequires:  mingw64-pthreads-devel
BuildRequires:  mingw64-zlib-devel
BuildRequires:  mingw64-zlib
BuildRequires:  mingw64-libpng16-16
BuildRequires:  mingw64-libstdc++
BuildRequires:  mingw64-readline
BuildRequires:  mingw64-readline-devel
BuildRequires:  mingw64-lzo
BuildRequires:  mingw64-lzo-devel
BuildRequires:  mingw64-libfastlz
BuildRequires:  mingw64-libfastlz-devel
BuildRequires:  mingw64-libsqlite3-0
BuildRequires:  mingw64-libsqlite-devel

BuildRequires:  sed
BuildRequires:  vim, procps, bc

BuildRequires:  osslsigncode
BuildRequires:  obs-name-resolution-settings
%description
bareos

%package devel
Summary:        bareos
Group:          Development/Libraries


%description devel
bareos


%package prevista
Summary:        bareos
%description prevista
bareos

%package prevista-debug
Summary:        bareos
%description prevista-debug
bareos


%package postvista
Summary:        bareos
%description postvista
bareos

%package postvista-debug
Summary:        bareos
%description postvista-debug
bareos






#{_mingw64_debug_package}

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

# unpack addons
for i in `ls %addonsdir`; do
   tar xvf %addonsdir/$i
done

CONTENT=`ls`

for flavor in `echo "%flavors"`; do
   mkdir $flavor
   for content in $CONTENT; do
      echo copying $content to $flavor
      cp -a $content $flavor
   done
done


%build

#cd src/win32/
#make WIN_DEBUG=%{WIN_DEBUG} BUILD_QTGUI=%{BUILD_QTGUI} WIN_VERSION=%{WIN_VERSION} WIN_VISTACOMPAT=%{WIN_VISTACOMPAT} %{?jobs:-j%jobs}


cd postvista/src/win32/
make WIN_DEBUG=no BUILD_QTGUI=%{BUILD_QTGUI} WIN_VERSION=%{WIN_VERSION} WIN_VISTACOMPAT=yes %{?jobs:-j%jobs}
cd -

cd postvista-debug/src/win32/
make WIN_DEBUG=yes BUILD_QTGUI=%{BUILD_QTGUI} WIN_VERSION=%{WIN_VERSION} WIN_VISTACOMPAT=yes %{?jobs:-j%jobs}
cd -

cd prevista/src/win32/
make WIN_DEBUG=no BUILD_QTGUI=%{BUILD_QTGUI} WIN_VERSION=%{WIN_VERSION} WIN_VISTACOMPAT=no %{?jobs:-j%jobs}
cd -

cd prevista-debug/src/win32/
make WIN_DEBUG=yes BUILD_QTGUI=%{BUILD_QTGUI} WIN_VERSION=%{WIN_VERSION} WIN_VISTACOMPAT=no %{?jobs:-j%jobs}
cd -

%install
for flavor in `echo "%flavors"`; do

   mkdir -p $RPM_BUILD_ROOT%{_mingw64_bindir}/$flavor
   mkdir -p $RPM_BUILD_ROOT/etc/$flavor/%name

   pushd $flavor/src/win32

   cp qt-tray-monitor/bareos-tray-monitor.exe \
      qt-console/bat.exe \
      console/bconsole.exe \
      filed/bareos-fd.exe \
      stored/bareos-sd.exe \
      stored/btape.exe \
      stored/bls.exe \
      stored/bextract.exe \
      dird/bareos-dir.exe \
      dird/bareos-dbcheck.exe \
      tools/bsmtp.exe \
      stored/libbareossd*.dll \
      cats/libbareoscats*.dll \
      lib/libbareos.dll \
      findlib/libbareosfind.dll \
      lmdb/libbareoslmdb.dll \
      plugins/filed/bpipe-fd.dll \
      plugins/filed/mssqlvdi-fd.dll \
      plugins/stored/autoxflate-sd.dll \
      $RPM_BUILD_ROOT%{_mingw64_bindir}/$flavor

   for cfg in  ../qt-tray-monitor/tray-monitor.fd.conf.in \
      ../qt-tray-monitor/tray-monitor.fd-sd.conf.in \
      ../qt-tray-monitor/tray-monitor.fd-sd-dir.conf.in \
      ../qt-console/bat.conf.in \
      ../dird/bareos-dir.conf.in \
      ../filed/bareos-fd.conf.in \
      ../stored/bareos-sd.conf.in \
      ../console/bconsole.conf.in \
      ; do cp $cfg $RPM_BUILD_ROOT/etc/$flavor/%name/$(basename $cfg .in)
   done

   for cfg in $RPM_BUILD_ROOT/etc/$flavor/%name/*.conf;
   do
     sed -f %SOURCE1 $cfg -i ;
   done

   popd

   mkdir -p $RPM_BUILD_ROOT/etc/$flavor/%name/ddl
   for i in creates drops grants updates; do
      mkdir $RPM_BUILD_ROOT/etc/$flavor/%name/ddl/$i/
      cp -av src/cats/ddl/$i/postgres* $RPM_BUILD_ROOT/etc/$flavor/%name/ddl/$i/
   done

   for sql in $RPM_BUILD_ROOT/etc/$flavor//%name/ddl/*/*.sql;
   do
      sed -f %SOURCE1 $sql -i ;
   done

   # sign binary files
   pushd $RPM_BUILD_ROOT%{_mingw64_bindir}/$flavor
   for BINFILE in *; do
      mv $BINFILE $BINFILE.unsigned
      osslsigncode sign \
                   -pkcs12 ${OLDPWD}/%SIGNCERT \
                   -readpass ${OLDPWD}/%SIGNPWFILE \
                   -n "${DESCRIPTION}" \
                   -i http://www.bareos.com/ \
                   -t http://timestamp.comodoca.com/authenticode \
                   -in  $BINFILE.unsigned \
                   -out $BINFILE
      rm *.unsigned
   done
   popd
done

%clean
rm -rf $RPM_BUILD_ROOT

%files


%files prevista
%defattr(-,root,root)
/etc/prevista/%name/*.conf
/etc/prevista/%name/ddl/
%dir %{_mingw64_bindir}/prevista
%{_mingw64_bindir}/prevista/*.dll
%{_mingw64_bindir}/prevista/*.exe


%files postvista
%defattr(-,root,root)
/etc/postvista/%name/*.conf
/etc/postvista/%name/ddl/
%dir %{_mingw64_bindir}/postvista
%{_mingw64_bindir}/postvista/*.dll
%{_mingw64_bindir}/postvista/*.exe


%files prevista-debug
%defattr(-,root,root)
/etc/prevista-debug/%name/*.conf
/etc/prevista-debug/%name/ddl/
%dir %{_mingw64_bindir}/prevista-debug
%{_mingw64_bindir}/prevista-debug/*.dll
%{_mingw64_bindir}/prevista-debug/*.exe


%files postvista-debug
%defattr(-,root,root)
/etc/postvista-debug/%name/*.conf
/etc/postvista-debug/%name/ddl/
%dir %{_mingw64_bindir}/postvista-debug
%{_mingw64_bindir}/postvista-debug/*.dll
%{_mingw64_bindir}/postvista-debug/*.exe

%changelog
