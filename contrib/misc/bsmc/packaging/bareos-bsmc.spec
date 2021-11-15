Name:           bareos-bsmc
Version:        0.1
Release:        1%{?dist}
Summary:        Bareos Simple Management CLI
Group:          Productivity/Archiving/Backup
License:        AGPL-3.0
URL:            https://github.com/bareos/bareos-bsmc/
Vendor:         The Bareos Team
Source:         %{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-root
BuildArch:      noarch
BuildRequires:  rsync
# for directory /etc/bareos/bareos-dir.d/
BuildRequires:  bareos-common
# required for restoring.
# Recommends would be enough, but nor supported by all distributions.
Requires:       bareos-filedaemon >= 15.2.1
# bsmcrmount
Requires:       python-bareos

%description
Bareos - Backup Archiving Recovery Open Sourced - bsmc 

bareos-bsmc is a CLI to Bareos, which lets you trigger tasks like backup or restore from command line.

%prep
%setup -q

%build

%install
mkdir -p %{buildroot}/bin %{buildroot}/etc 
rsync -av bin/.  %{buildroot}/bin/.
rsync -av etc/.  %{buildroot}/etc/.


%check

%files
%defattr(-,root,root,-)
%doc README.md
%config(noreplace) %attr(644,root,root) /etc/bareos/bsmc.conf
%config(noreplace) %attr(644,root,root) /etc/bareos/bareos-dir.d/*
/bin/*

%changelog
