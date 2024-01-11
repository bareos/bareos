#
# python-bareos spec file.
#

# based on
# https://docs.fedoraproject.org/en-US/packaging-guidelines/Python_Appendix/
#

%global srcname bareos

%define python3_build_requires python-rpm-macros python3-devel python3-setuptools


Name:           python-%{srcname}
Version:        0
Release:        1%{?dist}
License:        AGPL-3.0
Summary:        Backup Archiving REcovery Open Sourced - Python module
Url:            https://github.com/bareos/python-bareos/
Group:          Productivity/Archiving/Backup
Vendor:         The Bareos Team
Source:         %{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-root

BuildArch:      noarch

%global _description %{expand:
Bareos - Backup Archiving Recovery Open Sourced - Python module

This packages contains a python module to interact with a Bareos backup system.
It also includes some tools based on this module.}

%description %_description

%package -n python3-%{srcname}
Summary:        %{summary}
BuildRequires:  %{python3_build_requires}
%{?python_provide:%python_provide python3-%{srcname}}
%if 0%{?fedora} || 0%{?rhel} >= 8 || 0%{?suse_version} || 0%{?sle_version}
# Recommends is not supported on RHEL <= 7.
Recommends:     python3-configargparse
%endif

%description -n python3-%{srcname} %_description


%prep
%setup -q


%build
%py3_build


%install
%py3_install


%check
# This does not work,
# as "test" tries to download other packages from pip.
#%%{__python3} setup.py test

%files -n python3-%{srcname}
%defattr(-,root,root,-)
%doc README.rst
%{python3_sitelib}/%{srcname}/
%{python3_sitelib}/python_%{srcname}-*.egg-info/
%{_bindir}/*


%changelog
