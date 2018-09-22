
#!BuildIgnore: post-build-checks
Name:           winbareos-opsi
Version:        0
Release:        0
Summary:        Bareos Windows Packages for OPSI
License:        AGPL-3.0
Group:          Productivity/Archiving/Backup
URL:            http://www.bareos.org
Requires:       opsi-depotserver
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildArch:      noarch
BuildRequires:  sed
BuildRequires:  opsi-utils
BuildRequires:  winbareos-nsi = %{version}

Source0:        bareos-%{version}.tar.gz

%description
Bareos - Backup Archiving Recovery Open Sourced.
Bareos is a set of computer programs that permit you (or the system
administrator) to manage backup, recovery, and verification of computer
data across a network of computers of different kinds. In technical terms,
it is a network client/server based backup program. Bareos is relatively
easy to use and efficient, while offering many advanced storage management
features that make it easy to find and recover lost or damaged files.
Bareos source code has been released under the AGPL version 3 license.

This package contains the Bareos Windows client packages
for OPSI (http://www.opsi.org), the Windows system management solution.
Using OPSI, the package can be distributed to Windows systems.


%define opsidest /var/lib/opsi/repository


%prep
%setup -n bareos-%{version}

%build
cd platforms

# OPSI ProductVersion is at most 32 characters long
VERSION32C=$(sed -r -e 's/(.{1,32}).*/\1/' -e 's/\.*$//' <<< %{version})

# set version and release for OPSI
sed -i -e "s/^version: \$PackageVersion/version: %{release}/i" \
       -e "s/^version: \$ProductVersion/version: $VERSION32C/i" opsi/OPSI/control

WINBAREOS32=`ls -1 /winbareos*-release-32-bit-*.exe`
WINBAREOS64=`ls -1 /winbareos*-release-64-bit-*.exe`
if [ -r "$WINBAREOS32" ] && [ -r "$WINBAREOS64" ]; then
    mkdir -p opsi/CLIENT_DATA/data
    cp -a $WINBAREOS32 $WINBAREOS64 opsi/CLIENT_DATA/data

    WINBAREOS32b='data\\'`basename $WINBAREOS32`
    WINBAREOS64b='data\\'`basename $WINBAREOS64`
    sed -i -e's/^Set $ProductExe32$ .*/Set $ProductExe32$ = "'$WINBAREOS32b'"/' \
           -e's/^Set $ProductExe64$ .*/Set $ProductExe64$ = "'$WINBAREOS64b'"/' opsi/CLIENT_DATA/setup3264.ins

    opsi-makeproductfile -m -z opsi
fi
test -r winbareos*.opsi


%install
mkdir -p $RPM_BUILD_ROOT%{opsidest}
cp -a platforms/winbareos*.opsi* $RPM_BUILD_ROOT%{opsidest}


%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root)
%{opsidest}/winbareos*.opsi*

%changelog
