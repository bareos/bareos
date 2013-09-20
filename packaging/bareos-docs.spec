#!BuildIgnore: post-build-checks
#
# spec file for package bareos-docs
#
# Copyright (c) 2013-2013  Bareos GmbH & Co. KG
#

Name:           bareos-docs
Version:        13.3.0
Release:        0
License:        GFDL-1.3
Summary:        Documentation
Url:            http://www.bareos.org
Group:          Productivity/Archiving/Backup
Source:         %{name}-%version.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
BuildArch:      noarch
BuildRequires:  texlive-latex
BuildRequires:  texlive-tex4ht ImageMagick
%if ( 0%{?rhel_version} || 0%{?fedora_version} )
BuildRequires:  texlive-collection-latexrecommended texlive-moreverb texlive-ifmtarg texlive-xifthen
%endif


%description
Bareos - Backup Archiving Recovery Open Sourced.
Bareos is a set of computer programs that permit you (or the system
administrator) to manage backup, recovery, and verification of computer
data across a network of computers of different kinds. In technical terms,
it is a network client/server based backup program. Bareos is relatively
easy to use and efficient, while offering many advanced storage management
features that make it easy to find and recover lost or damaged files.

This package contains the Bareos documentation.


%prep
%setup -q

%build
cd manuals/en/main
make

find

%install



%files
%defattr(-,root,root)
%doc manuals/en/main/main.dvi
%doc manuals/en/main/main.pdf
%doc manuals/en/main/html
%changelog
