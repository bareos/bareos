%define __strip %{_mingw64_strip}
%define __objdump %{_mingw64_objdump}
%define _use_internal_dependency_generator 0
%define __find_requires %{_mingw64_findrequires}
%define __find_provides %{_mingw64_findprovides}
%define __os_install_post %{_mingw64_debug_install_post} \
                          %{_mingw64_install_post}

%define addonsdir /bareos-addons/

# flavors:
#   If name contains debug, enable debug during build.
#   If name contains prevista, build for windows < vista.
%define flavors postvista postvista-debug

%define SIGNCERT %{_builddir}/ia.p12
%define SIGNPWFILE %{_builddir}/signpassword


#!BuildIgnore: post-build-checks
Name:           winbareos-nsi
Version:        0.0.0
Release:        0
Summary:        Bareos Windows NSI package
License:        LGPLv2+
Group:          Development/Libraries
URL:            http://www.bareos.org
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildArch:      noarch

BuildRequires:  bareos-addons
BuildRequires:  winbareos-nssm
BuildRequires:  winbareos-php

BuildRequires:  bc
BuildRequires:  less
BuildRequires:  procps
BuildRequires:  sed
BuildRequires:  vim

# Bareos sources
BuildRequires:  mingw-debugsrc-devel = %{version}

BuildRequires:  mingw64-cross-nsis

BuildRequires:  mingw32-filesystem
BuildRequires:  mingw64-filesystem

BuildRequires:  mingw32-openssl
BuildRequires:  mingw64-openssl

BuildRequires:  mingw32-libopenssl
BuildRequires:  mingw64-libopenssl

BuildRequires:  mingw32-sed
BuildRequires:  mingw64-sed

BuildRequires:  mingw32-libgcc
BuildRequires:  mingw64-libgcc

BuildRequires:  mingw32-readline
BuildRequires:  mingw64-readline

BuildRequires:  mingw32-libstdc++
BuildRequires:  mingw64-libstdc++

BuildRequires:  mingw32-libwinpthread1
BuildRequires:  mingw64-libwinpthread1

BuildRequires:  mingw32-libqt4
BuildRequires:  mingw64-libqt4

#BuildRequires:  mingw32-qt4-debug
#BuildRequires:  mingw64-qt4-debug

BuildRequires:  mingw32-lzo
BuildRequires:  mingw64-lzo

BuildRequires:  mingw32-libfastlz
BuildRequires:  mingw64-libfastlz

BuildRequires:  mingw32-sqlite
BuildRequires:  mingw64-sqlite

BuildRequires:  mingw32-libsqlite
BuildRequires:  mingw64-libsqlite

BuildRequires:  mingw32-cmocka
BuildRequires:  mingw64-cmocka

BuildRequires:  mingw32-libjansson
BuildRequires:  mingw64-libjansson

BuildRequires:  mingw32-winbareos-postvista = %{version}
BuildRequires:  mingw64-winbareos-postvista = %{version}

BuildRequires:  mingw32-winbareos-postvista-debug = %{version}
BuildRequires:  mingw64-winbareos-postvista-debug = %{version}

BuildRequires:  osslsigncode
BuildRequires:  obs-name-resolution-settings

BuildRequires:  bareos-webui

Source1:         winbareos.nsi
Source2:         clientdialog.ini
Source3:         directordialog.ini
Source4:         storagedialog.ini
Source6:         bareos.ico
Source9:         databasedialog.ini

%define NSISDLLS KillProcWMI.dll AccessControl.dll LogEx.dll

%description
Bareos Windows NSI installer packages for the different variants.


%package devel
Summary:        bareos
Group:          Development/Libraries

%description devel
bareos

#{_mingw32_debug_package}

%prep

# unpack addons
for i in `ls %addonsdir`; do
   tar xvf %addonsdir/$i
done


%build
for flavor in %{flavors}; do

   WIN_DEBUG=$(echo $flavor | grep debug >/dev/null && echo yes || echo no)

   mkdir -p $RPM_BUILD_ROOT/$flavor/nsisplugins
   for dll in %NSISDLLS; do
      cp %{_builddir}/$dll $RPM_BUILD_ROOT/$flavor/nsisplugins
   done

   for BITS in 32 64; do
      mkdir -p $RPM_BUILD_ROOT/$flavor/release${BITS}
      pushd    $RPM_BUILD_ROOT/$flavor/release${BITS}

      echo "The installer may contain the following software:" >> %_sourcedir/LICENSE
      echo "" >> %_sourcedir/LICENSE

      # nssm
      cp -a /usr/lib/windows/nssm/win${BITS}/nssm.exe .
      echo "" >> %_sourcedir/LICENSE
      echo "NSSM - the Non-Sucking Service Manager: https://nssm.cc/" >> %_sourcedir/LICENSE
      echo "##### LICENSE FILE OF NSSM START #####" >> %_sourcedir/LICENSE
      cat /usr/lib/windows/nssm/README.txt >> %_sourcedir/LICENSE
      echo "##### LICENSE FILE OF NSSM END #####" >> %_sourcedir/LICENSE
      echo "" >> %_sourcedir/LICENSE

      # bareos-webui
      cp -av /usr/share/bareos-webui bareos-webui  # copy bareos-webui
      pushd bareos-webui

      mkdir install
      cp /etc/bareos-webui/*.ini install
      cp -av /etc/bareos install
      mkdir -p tests/selenium
      cp /usr/share/doc/packages/bareos-webui/selenium/webui-selenium-test.py tests/selenium

      echo "" >> %_sourcedir/LICENSE
      echo "##### LICENSE FILE OF BAREOS_WEBUI START #####" >> %_sourcedir/LICENSE
      cat /usr/share/doc/packages/bareos-webui/LICENSE >> %_sourcedir/LICENSE # append bareos-webui license file to LICENSE
      echo "##### LICENSE FILE OF BAREOS_WEBUI END #####" >> %_sourcedir/LICENSE
      echo "" >> %_sourcedir/LICENSE


      # php
      cp -a /usr/lib/windows/php/ .
      cp php/php.ini .
      echo "" >> %_sourcedir/LICENSE
      echo "PHP: http://php.net/" >> %_sourcedir/LICENSE
      echo "##### LICENSE FILE OF PHP START #####" >> %_sourcedir/LICENSE
      cat php/license.txt >> %_sourcedir/LICENSE
      echo "##### LICENSE FILE OF PHP END #####" >> %_sourcedir/LICENSE
      echo "" >> %_sourcedir/LICENSE

      popd

      # copy the sql ddls over
      cp -av /etc/$flavor/mingw${BITS}-winbareos/ddl $RPM_BUILD_ROOT/$flavor/release${BITS}

      # copy the sources over if we create debug package
      cp -av /bareos*  $RPM_BUILD_ROOT/$flavor/release${BITS}
   done

   DESCRIPTION="Bareos - Backup Archiving Recovery Open Sourced"

   for file in %{_mingw32_bindir}/$flavor/*.exe %{_mingw32_bindir}/$flavor/*.dll; do
      basename=`basename $file`
      dest=$RPM_BUILD_ROOT/$flavor/release32/$basename
      osslsigncode  sign \
                    -pkcs12 %SIGNCERT \
                    -readpass %SIGNPWFILE \
                    -n "${DESCRIPTION}" \
                    -i http://www.bareos.com/ \
                    -t http://timestamp.comodoca.com/authenticode \
                    -in  $file \
                    -out $dest
      osslsigncode verify -in $dest
   done

   for file in %{_mingw64_bindir}/$flavor/*.exe %{_mingw64_bindir}/$flavor/*.dll; do
      basename=`basename $file`
      dest=$RPM_BUILD_ROOT/$flavor/release64/$basename
      osslsigncode  sign \
                    -pkcs12 %SIGNCERT \
                    -readpass %SIGNPWFILE \
                    -n "${DESCRIPTION}" \
                    -i http://www.bareos.com/ \
                    -t http://timestamp.comodoca.com/authenticode \
                    -in  $file \
                    -out $dest
      osslsigncode verify -in $dest
   done

   cp -av %{_mingw32_bindir}/$flavor/bareos-config-deploy.bat $RPM_BUILD_ROOT/$flavor/release32/
   cp -av %{_mingw64_bindir}/$flavor/bareos-config-deploy.bat $RPM_BUILD_ROOT/$flavor/release64/

# copy python plugins into Plugin directory
   cp -av %{_mingw32_bindir}/$flavor/Plugins $RPM_BUILD_ROOT/$flavor/release32/
   cp -av %{_mingw64_bindir}/$flavor/Plugins $RPM_BUILD_ROOT/$flavor/release64/

   for file in \
      libcrypto-*.dll \
      libcmocka.dll \
      libfastlz.dll \
      libgcc_s_*-1.dll \
      libhistory6.dll \
      libjansson-4.dll \
      liblzo2-2.dll \
      libpng*.dll \
      libreadline6.dll \
      libssl-*.dll \
      libstdc++-6.dll \
      libsqlite3-0.dll \
      libtermcap-0.dll \
      openssl.exe \
      libwinpthread-1.dll \
      QtCore4.dll \
      QtGui4.dll \
      sed.exe \
      sqlite3.exe \
      zlib1.dll \
   ; do
      cp %{_mingw32_bindir}/$file $RPM_BUILD_ROOT/$flavor/release32
      cp %{_mingw64_bindir}/$file $RPM_BUILD_ROOT/$flavor/release64
   done

   for BITS in 32 64; do

      cp -r /etc/$flavor/mingw${BITS}-winbareos/config/ $RPM_BUILD_ROOT/$flavor/release${BITS}

      cp /etc/$flavor/mingw${BITS}-winbareos/fillup.sed $RPM_BUILD_ROOT/$flavor/release${BITS}/config

      cp %SOURCE1 %SOURCE2 %SOURCE3 %SOURCE4 %SOURCE6 %SOURCE9 \
               %_sourcedir/LICENSE $RPM_BUILD_ROOT/$flavor/release${BITS}

      makensis -DVERSION=%version -DPRODUCT_VERSION=%version-%release -DBIT_WIDTH=${BITS} \
               -DWIN_DEBUG=${WIN_DEBUG} $RPM_BUILD_ROOT/$flavor/release${BITS}/winbareos.nsi
   done
done

%install

for flavor in %{flavors}; do
   mkdir -p $RPM_BUILD_ROOT%{_mingw32_bindir}
   mkdir -p $RPM_BUILD_ROOT%{_mingw64_bindir}

   FLAVOR=`echo "%name" | sed 's/winbareos-nsi-//g'`
   DESCRIPTION="Bareos installer version %version"
   URL="http://www.bareos.com"

   for BITS in 32 64; do
      cp $RPM_BUILD_ROOT/$flavor/release${BITS}/Bareos*.exe \
           $RPM_BUILD_ROOT/winbareos-%version-$flavor-${BITS}-bit-r%release-unsigned.exe

      osslsigncode  sign \
                    -pkcs12 %SIGNCERT \
                    -readpass %SIGNPWFILE \
                    -n "${DESCRIPTION}" \
                    -i http://www.bareos.com/ \
                    -t http://timestamp.comodoca.com/authenticode \
                    -in  $RPM_BUILD_ROOT/winbareos-%version-$flavor-${BITS}-bit-r%release-unsigned.exe \
                    -out $RPM_BUILD_ROOT/winbareos-%version-$flavor-${BITS}-bit-r%release.exe

      osslsigncode verify -in $RPM_BUILD_ROOT/winbareos-%version-$flavor-${BITS}-bit-r%release.exe

      rm $RPM_BUILD_ROOT/winbareos-%version-$flavor-${BITS}-bit-r%release-unsigned.exe

      rm -R $RPM_BUILD_ROOT/$flavor/release${BITS}

   done

   rm -R $RPM_BUILD_ROOT/$flavor/nsisplugins
done

%clean
#rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root)
/winbareos-*.exe

#{_mingw32_bindir}
#{_mingw64_bindir}

%changelog
