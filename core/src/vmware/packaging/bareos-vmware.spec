#
# spec file for package  bareos-vmware-plugin
#
# Copyright (c) 2015-2018 Bareos GmbH & Co KG

# bareos-vadp-dumper must be main project,
# as the package with debug info gets the name of the main package.
Name:           bareos-vadp-dumper
Version:        0.0.0
Release:        0
License:        AGPL-3.0
URL:            http://www.bareos.org/
Vendor:         The Bareos Team
%define         sourcename bareos-vmware-%{version}
Source:         %{sourcename}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

BuildRequires:  bareos-vmware-vix-disklib-devel
BuildRequires:  cmake
BuildRequires:  gcc-c++
%if 0%{?suse_version}
BuildRequires:  libjansson-devel
%else
BuildRequires:  jansson-devel
%endif




#%%package -n     bareos-vmware-vix-disklib
#Summary:        VMware vix disklib distributable libraries
#Group:          Productivity/Archiving/Backup
#Url:            https://www.vmware.com/support/developer/vddk/
#License:        VMware
#AutoReqProv:    no
#%%description -n bareos-vmware-vix-disklib
#The Virtual Disk Development Kit (VDDK) is a collection of C/C++ libraries,
#code samples, utilities, and documentation to help you create and access
#VMware virtual disk storage. The VDDK is useful in conjunction with the
#vSphere API for writing backup and recovery software, or similar applications.
#This package only contains the distributable code to comply with
#https://communities.vmware.com/viewwebdoc.jspa?documentID=DOC-10141




#PreReq:
#Provides:




%prep
%setup -q -n %{sourcename}

%build
CFLAGS="${CFLAGS:-%optflags}" ; export CFLAGS ;
CXXFLAGS="${CXXFLAGS:-%optflags}" ; export CXXFLAGS ;

cmake -DCMAKE_INSTALL_PREFIX:PATH=/usr -DCMAKE_VERBOSE_MAKEFILE=ON .

%__make CFLAGS="$RPM_OPT_FLAGS" CXXFLAGS="$RPM_OPT_FLAGS" %{?_smp_mflags};


%install
%make_install




#%%files -n bareos-vmware-vix-disklib
#/usr/lib/vmware-vix-disklib/lib64


%changelog

