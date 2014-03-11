%define __strip %{_mingw64_strip}
%define __objdump %{_mingw64_objdump}
%define _use_internal_dependency_generator 0
%define __find_requires %{_mingw64_findrequires}
%define __find_provides %{_mingw64_findprovides}
%define __os_install_post %{_mingw64_debug_install_post} \
                          %{_mingw64_install_post}

#!BuildIgnore: post-build-checks
Name:           winbareos-nsi
Version:        13.2.3
Release:        0
Summary:        bareos
License:        LGPLv2+
Group:          Development/Libraries
URL:            http://bareos.org
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildArch:      noarch
BuildRequires:  mingw32-filesystem
BuildRequires:  mingw64-filesystem
BuildRequires:  mingw64-cross-nsis

BuildRequires: mingw32-openssl
#mingw32-libopenssl mingw32-libgcc mingw32-openssl mingw32-libstdc++ mingw32-pthreads mingw32-libqt4 mingw32-libqt4-win32 mingw32-zlib mingw32-libpng
BuildRequires: mingw64-openssl
BuildRequires: mingw32-sed
BuildRequires: mingw64-sed


BuildRequires:  sed
BuildRequires:  vim, procps, bc

BuildRequires:  mingw32-winbareos  = %{version}
BuildRequires:  mingw64-winbareos  = %{version}

Source1:         winbareos.nsi
Source2:         clientdialog.ini
Source3:         directordialog.ini
Source4:         KillProcWMI.dll
Source5:         bareos.ico
Source6:         AccessControl.dll
%description
bareos

%package devel
Summary:        bareos
Group:          Development/Libraries


%description devel
bareos

#{_mingw32_debug_package}

%prep


%build

mkdir -p $RPM_BUILD_ROOT/nsisplugins
cp %SOURCE4 $RPM_BUILD_ROOT/nsisplugins  #  KillProcWMI
cp %SOURCE6 $RPM_BUILD_ROOT/nsisplugins  #  AccessControl

mkdir  $RPM_BUILD_ROOT/release32
mkdir  $RPM_BUILD_ROOT/release64

for file in \
      bareos-tray-monitor.exe bat.exe bareos-fd.exe bconsole.exe \
      bpipe-fd.dll mssqlvdi-fd.dll libbareos.dll libbareosfind.dll \
      libcrypto-*.dll libgcc_s_*-1.dll libhistory6.dll \
      libreadline6.dll libssl-*.dll libstdc++-6.dll \
      libtermcap-0.dll pthreadGCE2.dll zlib1.dll \
      QtCore4.dll QtGui4.dll liblzo2-2.dll libfastlz.dll \
      libpng15-15.dll openssl.exe sed.exe; do
      cp  %{_mingw32_bindir}/$file $RPM_BUILD_ROOT/release32
      cp  %{_mingw64_bindir}/$file $RPM_BUILD_ROOT/release64
done

for cfg in /etc/mingw32-winbareos/*.conf; do
      cp $cfg $RPM_BUILD_ROOT/release32
done

for cfg in /etc/mingw64-winbareos/*.conf; do
      cp $cfg $RPM_BUILD_ROOT/release64
done

cp %SOURCE1 %SOURCE2 %SOURCE3 %SOURCE4 %SOURCE5 %SOURCE6 %_sourcedir/LICENSE $RPM_BUILD_ROOT/release32
cp %SOURCE1 %SOURCE2 %SOURCE3 %SOURCE4 %SOURCE5 %SOURCE6 %_sourcedir/LICENSE $RPM_BUILD_ROOT/release64

makensis -DPRODUCT_VERSION=%version-%release -DBIT_WIDTH=32 $RPM_BUILD_ROOT/release32/winbareos.nsi
makensis -DPRODUCT_VERSION=%version-%release -DBIT_WIDTH=64 $RPM_BUILD_ROOT/release64/winbareos.nsi

%install
mkdir -p $RPM_BUILD_ROOT%{_mingw32_bindir}
mkdir -p $RPM_BUILD_ROOT%{_mingw64_bindir}

cp $RPM_BUILD_ROOT/release32/Bareos*.exe $RPM_BUILD_ROOT/winbareos-%version-32-bit-r%release.exe
cp $RPM_BUILD_ROOT/release64/Bareos*.exe $RPM_BUILD_ROOT/winbareos-%version-64-bit-r%release.exe

rm -R $RPM_BUILD_ROOT/release32
rm -R $RPM_BUILD_ROOT/release64
rm -R $RPM_BUILD_ROOT/nsisplugins

%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root)

/winbareos-%version-32-bit*.exe
/winbareos-%version-64-bit*.exe

#{_mingw32_bindir}
#{_mingw64_bindir}

%changelog
