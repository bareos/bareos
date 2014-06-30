# If versionstring contains debug, enable debug during build
%define WIN_DEBUG %(echo %version | grep debug >/dev/null 2>&1 && echo "yes" || echo "no")

# Determine Windows Version (32/64) from name (mingw32-.../ming64-...)
%define WIN_VERSION %(echo %name | grep 64 >/dev/null 2>&1 && echo "64" || echo "32")

# Set what to build
%define BUILD_QTGUI yes
%define WIN_VISTACOMPAT yes

%define __strip %{_mingw64_strip}
%define __objdump %{_mingw64_objdump}
%define _use_internal_dependency_generator 0
%define __find_requires %{_mingw64_findrequires}
%define __find_provides %{_mingw64_findprovides}
#define __os_install_post #{_mingw64_debug_install_post} \
#                          #{_mingw64_install_post}

Name:           mingw64-winbareos
Version:        13.2.3
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

Patch1:        tray-monitor-conf.patch

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
BuildRequires:  mingw64-libgcrypt
BuildRequires:  mingw64-cross-nsis
BuildRequires:  mingw64-gcc
BuildRequires:  mingw64-gcc-c++
BuildRequires:  mingw64-pthreads-devel
BuildRequires:  mingw64-zlib-devel
BuildRequires:  mingw64-zlib
BuildRequires:  mingw64-libpng
BuildRequires:  mingw64-libstdc++
BuildRequires:  mingw64-readline
BuildRequires:  mingw64-readline-devel
BuildRequires:  mingw64-lzo
BuildRequires:  mingw64-lzo-devel
BuildRequires:  mingw64-libfastlz
BuildRequires:  mingw64-libfastlz-devel

BuildRequires:  sed
BuildRequires:  vim, procps, bc

%description
bareos

%package devel
Summary:        bareos
Group:          Development/Libraries


%description devel
bareos

#{_mingw64_debug_package}

%prep
#setup
%setup -q -n bareos-%{version}
%patch1 -p1

tar xvf %SOURCE2
tar xvf %SOURCE3

%build

cd src/win32/
make WIN_DEBUG=%{WIN_DEBUG} BUILD_QTGUI=%{BUILD_QTGUI} WIN_VERSION=%{WIN_VERSION} WIN_VISTACOMPAT=%{WIN_VISTACOMPAT} %{?jobs:-j%jobs}

%install

mkdir -p $RPM_BUILD_ROOT%{_mingw64_bindir}

mkdir -p $RPM_BUILD_ROOT/etc/%name

pushd src/win32

cp qt-tray-monitor/bareos-tray-monitor.exe \
   qt-console/bat.exe console/bconsole.exe \
   filed/bareos-fd.exe lib/libbareos.dll \
   findlib/libbareosfind.dll \
   plugins/filed/bpipe-fd.dll \
   plugins/filed/mssqlvdi-fd.dll \
   $RPM_BUILD_ROOT%{_mingw64_bindir}

for cfg in  ../qt-tray-monitor/tray-monitor.conf.in \
   ../qt-console/bat.conf.in \
   ../filed/bareos-fd.conf.in \
   ../console/bconsole.conf.in \
   ; do cp $cfg $RPM_BUILD_ROOT/etc/%name/$(basename $cfg .in)
done

for cfg in $RPM_BUILD_ROOT/etc/%name//*.conf;
do
  sed -f %SOURCE1 $cfg -i ;
done

%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root)
/etc/%name/*.conf
%dir %{_mingw64_bindir}
%{_mingw64_bindir}/*.dll
%{_mingw64_bindir}/*.exe

%changelog
