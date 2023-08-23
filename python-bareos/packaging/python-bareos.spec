#
# python-bareos spec file.
#

# based on
# https://docs.fedoraproject.org/en-US/packaging-guidelines/Python_Appendix/
# specifically on
# https://pagure.io/packaging-committee/blob/ae14fdb50cc6665a94bc32f7d984906ce1eece45/f/guidelines/modules/ROOT/pages/Python_Appendix.adoc
#

#
# For current distribution, create
# python2-bareos and python3-bareos packages.
#
# CentOS 6 supports only Python2.
#
# CentOS <= 7 and SLES <= 12,
# the Python2 package is namend python-bareos (instead of python2-bareos).
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
