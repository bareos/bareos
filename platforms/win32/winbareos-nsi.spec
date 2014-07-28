%define __strip %{_mingw64_strip}
%define __objdump %{_mingw64_objdump}
%define _use_internal_dependency_generator 0
%define __find_requires %{_mingw64_findrequires}
%define __find_provides %{_mingw64_findprovides}
%define __os_install_post %{_mingw64_debug_install_post} \
                          %{_mingw64_install_post}

# If versionstring contains debug, enable debug during build
%define WIN_DEBUG %(echo %version | grep debug >/dev/null 2>&1 && echo "yes" || echo "no")


#!BuildIgnore: post-build-checks
Name:           winbareos-nsi
Version:        14.2.0
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

BuildRequires:  mingw32-openssl
BuildRequires:  mingw64-openssl
BuildRequires:  mingw32-sed
BuildRequires:  mingw64-sed


BuildRequires:  sed
BuildRequires:  vim, procps, bc

BuildRequires:  mingw32-winbareos = %{version}
BuildRequires:  mingw64-winbareos = %{version}
BuildRequires:  mingw-debugsrc-devel = %{version}


BuildRequires:  mingw32-libgcc
BuildRequires:  mingw64-libgcc

BuildRequires:  mingw32-readline
BuildRequires:  mingw64-readline

BuildRequires:  mingw32-libstdc++
BuildRequires:  mingw64-libstdc++

BuildRequires:  mingw32-pthreads
BuildRequires:  mingw64-pthreads

BuildRequires:  mingw32-libqt4
BuildRequires:  mingw64-libqt4

BuildRequires:  mingw32-lzo
BuildRequires:  mingw64-lzo

BuildRequires:  mingw32-libfastlz
BuildRequires:  mingw64-libfastlz

Source1:         winbareos.nsi
Source2:         clientdialog.ini
Source3:         directordialog.ini
Source4:         storagedialog.ini
Source5:         KillProcWMI.dll
Source6:         bareos.ico
Source7:         AccessControl.dll
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
cp %SOURCE5 $RPM_BUILD_ROOT/nsisplugins  #  KillProcWMI
cp %SOURCE7 $RPM_BUILD_ROOT/nsisplugins  #  AccessControl

mkdir $RPM_BUILD_ROOT/release32
mkdir $RPM_BUILD_ROOT/release64

# copy the sql ddls over
cp -av /etc/mingw32-winbareos/ddl $RPM_BUILD_ROOT/release32
cp -av /etc/mingw64-winbareos/ddl $RPM_BUILD_ROOT/release64

# copy the sources over if we create debug package
%if %{WIN_DEBUG} == "yes"
cp -av /bareos-*debug*  $RPM_BUILD_ROOT/release32
cp -av /bareos-*debug*  $RPM_BUILD_ROOT/release64
%endif



for file in \
   bareos-fd.exe \
   bareos-sd.exe \
   bareos-dir.exe \
   dbcheck.exe \
   bconsole.exe \
   bsmtp.exe \
   btape.exe \
   bls.exe \
   bextract.exe \
   bareos-tray-monitor.exe \
   bat.exe \
   bpipe-fd.dll \
   mssqlvdi-fd.dll \
   autoxflate-sd.dll \
   libbareos.dll \
   libbareosfind.dll \
   libbareoslmdb.dll \
   libbareoscats*.dll \
   libbareossd*.dll \
   libcrypto-*.dll \
   libgcc_s_*-1.dll \
   libhistory6.dll \
   libreadline6.dll \
   libssl-*.dll \
   libstdc++-6.dll \
   libtermcap-0.dll \
   pthreadGCE2.dll \
   zlib1.dll \
   QtCore4.dll \
   QtGui4.dll \
   liblzo2-2.dll \
   libfastlz.dll \
   libpng15-15.dll \
   openssl.exe \
   sed.exe;\
do
   cp %{_mingw32_bindir}/$file $RPM_BUILD_ROOT/release32
   cp %{_mingw64_bindir}/$file $RPM_BUILD_ROOT/release64
done

for cfg in /etc/mingw32-winbareos/*.conf; do
   cp $cfg $RPM_BUILD_ROOT/release32
done

for cfg in /etc/mingw64-winbareos/*.conf; do
   cp $cfg $RPM_BUILD_ROOT/release64
done

cp %SOURCE1 %SOURCE2 %SOURCE3 %SOURCE4 %SOURCE5 %SOURCE6 %SOURCE7 %_sourcedir/LICENSE $RPM_BUILD_ROOT/release32
cp %SOURCE1 %SOURCE2 %SOURCE3 %SOURCE4 %SOURCE5 %SOURCE6 %SOURCE7 %_sourcedir/LICENSE $RPM_BUILD_ROOT/release64

makensis -DVERSION=%version -DPRODUCT_VERSION=%version-%release -DBIT_WIDTH=32 -DWIN_DEBUG=%{WIN_DEBUG} $RPM_BUILD_ROOT/release32/winbareos.nsi
makensis -DVERSION=%version -DPRODUCT_VERSION=%version-%release -DBIT_WIDTH=64 -DWIN_DEBUG=%{WIN_DEBUG} $RPM_BUILD_ROOT/release64/winbareos.nsi

%install
mkdir -p $RPM_BUILD_ROOT%{_mingw32_bindir}
mkdir -p $RPM_BUILD_ROOT%{_mingw64_bindir}

cp $RPM_BUILD_ROOT/release32/Bareos*.exe $RPM_BUILD_ROOT/winbareos-%version-32-bit-r%release.exe
cp $RPM_BUILD_ROOT/release64/Bareos*.exe $RPM_BUILD_ROOT/winbareos-%version-64-bit-r%release.exe

rm -R $RPM_BUILD_ROOT/release32
rm -R $RPM_BUILD_ROOT/release64
rm -R $RPM_BUILD_ROOT/nsisplugins
#rm -R $RPM_BUILD_ROOT/bareos-*

%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root)

/winbareos-%version-32-bit*.exe
/winbareos-%version-64-bit*.exe

#{_mingw32_bindir}
#{_mingw64_bindir}

%changelog
