
# Determine Windows Version (32/64) from name (mingw32-.../ming64-...)
%define WIN_VERSION %(echo %name | grep 64 >/dev/null 2>&1 && echo "64" || echo "32")

# Set what to build
%define BUILD_QTGUI yes

%define mingw mingw32
%define __strip %{_mingw32_strip}
%define __objdump %{_mingw32_objdump}
%define _use_internal_dependency_generator 0
#define __find_requires %%{_mingw32_findrequires}
%define __find_provides %{_mingw32_findprovides}
#define __os_install_post #{_mingw32_debug_install_post} \
#                          #{_mingw32_install_post}
%define bindir %{_mingw32_bindir}

# flavors:
#   If name contains debug, enable debug during build.
#   If name contains prevista, build for windows < vista.
%define flavors postvista postvista-debug
%define dirs_with_unittests lib findlib
%define bareos_configs bareos-dir.d/ bareos-fd.d/ bareos-sd.d/ tray-monitor.d/ bconsole.conf bat.conf

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
BuildRequires:  %{mingw}-libqt4-devel
BuildRequires:  %{mingw}-libqt4
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
BuildRequires:  %{mingw}-cmocka
BuildRequires:  %{mingw}-cmocka-devel
BuildRequires:  %{mingw}-libjansson
BuildRequires:  %{mingw}-libjansson-devel
#BuildRequires:  %%{mingw}-qt4-debug

BuildRequires:  bc
BuildRequires:  less
BuildRequires:  procps
BuildRequires:  sed
BuildRequires:  vim

%description
Base package for Bareos Windows build.

%package postvista
Summary:        bareos
%description postvista
Bareos for Windows versions >= Windows Vista

%package postvista-debug
Summary:        bareos
%description postvista-debug
Bareos Debug for Windows versions >= Windows Vista


%prep
%setup -q -n bareos-%{version}

# unpack addons
for i in `ls %addonsdir`; do
   tar xvf %addonsdir/$i
done

CONTENT=`ls`

for flavor in %flavors; do
   mkdir $flavor
   for content in $CONTENT; do
      echo copying $content to $flavor
      cp -a $content $flavor
   done
done


%build

for flavor in %flavors; do
   cd $flavor/src/win32/
   WIN_VISTACOMPAT=$(echo $flavor | grep postvista >/dev/null && echo yes || echo no)
   WIN_DEBUG=$(echo $flavor | grep debug >/dev/null && echo yes || echo no)
   make WIN_DEBUG=$WIN_DEBUG BUILD_QTGUI=%{BUILD_QTGUI} WIN_VERSION=%{WIN_VERSION} WIN_VISTACOMPAT=$WIN_VISTACOMPAT %{?jobs:-j%jobs}
   cd -

   for UNITTEST_DIR in %dirs_with_unittests; do
      cd $flavor/src/win32/${UNITTEST_DIR}/unittests
      make WIN_DEBUG=$WIN_DEBUG BUILD_QTGUI=%{BUILD_QTGUI} WIN_VERSION=%{WIN_VERSION} WIN_VISTACOMPAT=$WIN_VISTACOMPAT %{?jobs:-j%jobs}
      cd -
   done

   for cfg in $flavor/src/qt-console/bat.conf.in $flavor/src/console/bconsole.conf.in; do
     cp $cfg $flavor/src/defaultconfigs/
   done

#  rename all "*.conf.in" files to "*.conf"
   find $flavor/src/defaultconfigs/ -name "*.in" -exec sh -c 'name={}; newname=`echo $name | sed 's/.in$//'`; mv $name $newname' \;
done


%install
for flavor in %flavors; do

   mkdir -p $RPM_BUILD_ROOT%{bindir}/$flavor

   for UNITTEST_DIR in %dirs_with_unittests; do
      cp $flavor/src/win32/${UNITTEST_DIR}/unittests/*.exe $RPM_BUILD_ROOT%{bindir}/$flavor
   done

   cd $flavor/src/win32
   cp qt-tray-monitor/bareos-tray-monitor.exe \
      qt-console/bat.exe \
      console/bconsole.exe \
      filed/bareos-fd.exe \
      stored/bareos-sd.exe \
      stored/btape.exe \
      stored/bls.exe \
      stored/bextract.exe \
      stored/bscan.exe \
      dird/bareos-dir.exe \
      dird/bareos-dbcheck.exe \
      tools/bsmtp.exe \
      tools/bregex.exe \
      tools/bwild.exe \
      tests/grow.exe \
      tests/bregtest.exe \
      stored/libbareossd*.dll \
      cats/libbareoscats*.dll \
      lib/libbareos.dll \
      findlib/libbareosfind.dll \
      lmdb/libbareoslmdb.dll \
      plugins/filed/bpipe-fd.dll \
      plugins/filed/mssqlvdi-fd.dll \
      plugins/filed/python-fd.dll \
      plugins/stored/autoxflate-sd.dll \
      plugins/stored/python-sd.dll \
      plugins/dird/python-dir.dll \
      $RPM_BUILD_ROOT%{bindir}/$flavor
   cd -

   install -m 755 platforms/win32/bareos-config-deploy.bat $RPM_BUILD_ROOT/%{bindir}/$flavor

   mkdir -p $RPM_BUILD_ROOT%{bindir}/$flavor/Plugins
# identical for all flavors
   cp src/plugins/dird/*.py src/plugins/stored/*.py src/plugins/filed/*.py $RPM_BUILD_ROOT%{bindir}/$flavor/Plugins


   mkdir -p $RPM_BUILD_ROOT/etc/$flavor/%name/config/

   cd $flavor/src/defaultconfigs/
   for cfg in %{bareos_configs}; do
     cp -r $cfg $RPM_BUILD_ROOT/etc/$flavor/%name/config/$cfg
   done
   cd -

   mkdir -p $RPM_BUILD_ROOT/etc/$flavor/%name/ddl/

   for i in creates drops grants updates; do
      mkdir $RPM_BUILD_ROOT/etc/$flavor/%name/ddl/$i/
      cp -av src/cats/ddl/$i/postgres* $RPM_BUILD_ROOT/etc/$flavor/%name/ddl/$i/
   done

   for i in creates updates; do
      cp -av src/cats/ddl/$i/sqlite* $RPM_BUILD_ROOT/etc/$flavor/%name/ddl/$i/
   done

   for sql in $RPM_BUILD_ROOT/etc/$flavor//%name/ddl/*/*.sql; do
      sed -f platforms/win32/fillup.sed $sql -i
   done

   cp platforms/win32/fillup.sed $RPM_BUILD_ROOT/etc/$flavor/%name/
done

%clean
rm -rf $RPM_BUILD_ROOT

%files


%files postvista
%defattr(-,root,root)
/etc/postvista/%name/config/
/etc/postvista/%name/ddl/
/etc/postvista/%name/fillup.sed
%dir %{bindir}/postvista
%{bindir}/postvista/*.dll
%{bindir}/postvista/*.exe
%{bindir}/postvista/bareos-config-deploy.bat
%{bindir}/postvista/Plugins/

%files postvista-debug
%defattr(-,root,root)
/etc/postvista-debug/%name/config/
/etc/postvista-debug/%name/ddl/
/etc/postvista-debug/%name/fillup.sed
%dir %{bindir}/postvista-debug
%{bindir}/postvista-debug/*.dll
%{bindir}/postvista-debug/*.exe
%{bindir}/postvista-debug/bareos-config-deploy.bat
%{bindir}/postvista-debug/Plugins/

%changelog
