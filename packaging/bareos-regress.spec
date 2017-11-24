
Name:          bareos-regress
Version:       0.0.0
Release:       0%{?dist}
Summary:       Bareos Regression Testing
Group:         Productivity/Archiving/Backup
License:       AGPL-3.0+
URL:           http://www.bareos-webui.org/
Vendor:        The Bareos Team
Source:        %{name}-%{version}.tar.gz
BuildRoot:     %{_tmppath}/%{name}-%{version}-build
BuildArch:     noarch

BuildRequires: make rsync sed
BuildRequires: bareos-database-sqlite3
BuildRequires: bareos-regress-config

Requires:   acl
Requires:   attr
Requires:   cmake
Requires:   bareos
Requires:   bareos-database-sqlite3
Requires:   bareos-regress-config
Requires:   bareos-storage-fifo
Requires:   bareos-storage-tape
Requires:   bareos-tools
Requires:   make
Requires:   sed

%description
Bareos - Backup Archiving Recovery Open Sourced. \
Bareos is a set of computer programs that permit you (or the system \
administrator) to manage backup, recovery, and verification of computer \
data across a network of computers of different kinds. In technical terms, \
it is a network client/server based backup program. Bareos is relatively \
easy to use and efficient, while offering many advanced storage management \
features that make it easy to find and recover lost or damaged files. \
Bareos source code has been released under the AGPL version 3 license.

This package contains Bareos regression tests.
Don't use them on productive systems.


%prep
%setup

%build
ln -s /etc/bareos/bareos-regress.conf config
tmp="$PWD/tmp" make sed

%install
make DESTDIR=%{buildroot} install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc README* LICENSE
%defattr(-,bareos,bareos,-)
/var/lib/bareos/bareos-regress/

%post
if ! [ -e /var/lib/bareos/bareos-regress/config ]; then
  ln -s /etc/bareos/bareos-regress.conf /var/lib/bareos/bareos-regress/config
fi
su - bareos -s /bin/sh -c "cd /var/lib/bareos/bareos-regress; make sed" || true

%changelog
