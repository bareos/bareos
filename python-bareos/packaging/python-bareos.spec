%global srcname bareos

#%%if 0%%{?rhel} || 0%%{?suse_version} == 1110 || 0%%{?suse_version} == 1315
%bcond_without python2
#%%else
#%%bcond_with python2
#%%endif

%if 0%{?with_python2}
%global PYVER 2
%define noarch 0
%if 0%{?rhel_version} >= 800 || 0%{?fedora} >= 31
%define pypkg python2
%else
%define pypkg python
%endif
%else
%global PYVER 3
%define noarch 1
%endif

%global pyXsuf %{PYVER}
%global pyXcmd python%{PYVER}

Name:           python-%{srcname}
Version:        0.4
Release:        1%{?dist}
Summary:        Backup Archiving REcovery Open Sourced - Python module
Group:          Productivity/Archiving/Backup
License:        AGPL-3.0
URL:            https://github.com/bareos/python-bareos/
Vendor:         The Bareos Team
#Source0:        http://pypi.python.org/packages/source/e/%%{srcname}/%%{srcname}-%%{version}.tar.gz
Source:         %{name}-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-root
%global debug_package %{nil}
%if %{with python2}
BuildRequires:  %{pypkg}-devel
BuildRequires:  %{pypkg}-setuptools
Requires:       %{pypkg}-dateutil
%endif
%if %{with python3}
BuildRequires:  python3-devel
BuildRequires:  python3-setuptools
Requires:       python3-dateutil
%endif
%if %noarch
BuildArch:      noarch
%endif
%{?python_provide:%python_provide python-%{srcname}}

%description
Bareos - Backup Archiving Recovery Open Sourced - Python module

This packages contains a python module to interact with a Bareos backup system.
It also includes some tools based on this module.

%if 0%{?opensuse_version} || 0%{?sle_version}
%debug_package
%endif

%define pyX_sitelib %(%{pyXcmd} -c "from distutils.sysconfig import get_python_lib; print(get_python_lib())")

%prep
%setup -q

%build
%{pyXcmd} setup.py build

%install
# Must do the python2 install first because the scripts in /usr/bin are
# overwritten with every setup.py install, and in general we want the
# python3 version to be the default.
%{pyXcmd} setup.py install --prefix=%{_prefix} --root=%{buildroot}

%check
# does not work, as it tries to download other packages from pip
#%%{__python2} setup.py test
#%%{pyXcmd} setup.py -q test

# Note that there is no %%files section for the unversioned python module if we are building for several python runtimes
%files
%defattr(-,root,root,-)
%doc README.rst
%{pyX_sitelib}/*
%{_bindir}/*

%changelog
