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


%define python2_build_requires python-rpm-macros python2-devel python2-setuptools
%define python3_build_requires python-rpm-macros python3-devel python3-setuptools

%if 0%{?rhel} > 0 && 0%{?rhel} <= 6
%define skip_python3 1
%define python2_build_requires python-rpm-macros python2-rpm-macros python-devel python-setuptools
%endif


%define python2_extra_package 1

%if 0%{?rhel} > 0 && 0%{?rhel} <= 7
%define python2_extra_package 0
%endif

%if 0%{?sle_version} <= 120000
%define python2_extra_package 0
%endif


Name:           python-%{srcname}
Version:        0
Release:        1%{?dist}
License:        AGPL-3.0
Summary:        Backup Archiving REcovery Open Sourced - Python module
Url:            https://github.com/bareos/python-bareos/
Group:          Productivity/Archiving/Backup
Vendor:         The Bareos Team
Source:         %{name}-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-root

BuildArch:      noarch

%global _description %{expand:
Bareos - Backup Archiving Recovery Open Sourced - Python module

This packages contains a python module to interact with a Bareos backup system.
It also includes some tools based on this module.}

%description %_description


%if ! 0%{?skip_python2}

%if 0%{?python2_extra_package}

%package -n python2-%{srcname}
Summary:        %{summary}
BuildRequires:  %{python2_build_requires}
%{?python_provide:%python_provide python2-%{srcname}}

%description -n python2-%{srcname} %_description

%endif
# python2_extra_package

%endif
# skip_python2


%if ! 0%{?skip_python3}

%package -n python3-%{srcname}
Summary:        %{summary}
BuildRequires:  %{python3_build_requires}
%{?python_provide:%python_provide python3-%{srcname}}

%description -n python3-%{srcname} %_description

%endif
# skip_python3

%prep
#%%autosetup -n %%{srcname}-%%{version}
%setup -q

%build
%if ! 0%{?skip_python2}
%py2_build
%endif

%if ! 0%{?skip_python3}
%py3_build
%endif


%install
# Must do the python2 install first because the scripts in /usr/bin are
# overwritten with every setup.py install, and in general we want the
# python3 version to be the default.
%if ! 0%{?skip_python2}
%py2_install
%endif

%if ! 0%{?skip_python3}
%py3_install
%endif


%check
# This does not work,
# as "test" tries to download other packages from pip.
#%%{__python2} setup.py test
#%%{__python3} setup.py test


%if ! 0%{?skip_python2}
%if ! 0%{?python2_extra_package}
%files
%else
%files -n python2-%{srcname}
%endif
# python2_extra_package
%defattr(-,root,root,-)
%doc README.rst
%{python2_sitelib}/%{srcname}/
%{python2_sitelib}/python_%{srcname}-*.egg-info/
# Include binaries only if no python3 package will be build.
%if 0%{?skip_python3}
%{_bindir}/*
%endif
%endif
# skip_python2


%if ! 0%{?skip_python3}
%files -n python3-%{srcname}
%defattr(-,root,root,-)
%doc README.rst
%{python3_sitelib}/%{srcname}/
%{python3_sitelib}/python_%{srcname}-*.egg-info/
%{_bindir}/*
%endif
# skip_python3


%changelog
