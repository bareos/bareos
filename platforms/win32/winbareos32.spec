%define __strip %{_mingw32_strip}
%define __objdump %{_mingw32_objdump}
%define _use_internal_dependency_generator 0
%define __find_requires %{_mingw32_findrequires}
%define __find_provides %{_mingw32_findprovides}
#define __os_install_post #{_mingw32_debug_install_post} \
#                          #{_mingw32_install_post}


Name:           mingw32-winbareos
Version:        12.4.5
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

Patch1:        tray-monitor-conf.patch

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
%patch1 -p1

tar xvf %SOURCE2

%build

cd src/win32/


make BUILD_QTGUI=yes   %{?jobs:-j%jobs}
#make  %{?jobs:-j%jobs}

#make BUILD_QTGUI=yes WIN_VERSION=64  %{?jobs:-j%jobs}


%install

mkdir -p $RPM_BUILD_ROOT%{_mingw32_bindir}

mkdir -p $RPM_BUILD_ROOT/etc/%name

pushd src/win32

cp qt-tray-monitor/bareos-tray-monitor.exe \
   qt-console/bat.exe console/bconsole.exe \
   filed/bareos-fd.exe lib/libbareos.dll \
   findlib/libbareosfind.dll \
   plugins/fd/bpipe-fd.dll $RPM_BUILD_ROOT%{_mingw32_bindir}

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
%dir %{_mingw32_bindir}
%{_mingw32_bindir}/*.dll
%{_mingw32_bindir}/*.exe

%changelog
