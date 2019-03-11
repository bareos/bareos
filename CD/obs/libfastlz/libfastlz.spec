#
# spec file for package 
#
# Copyright (c) 2013 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#

Name:           libfastlz
Version:        0.1
Release:        0
License:        BSD
Summary:        zlib-like interface to fast block compression (LZ4 or FastLZ) libraries
Url:            https://github.com/exalead/fastlzlib
Group:          Development/Libraries/C and C++
Source:         %name-%version.tar.bz2
#PreReq:
#Provides:
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
BuildRequires: gcc
BuildRequires: make

BuildRequires: zlib-devel
BuildRequires: automake

%description
zlib-like interface to fast block compression (LZ4 or FastLZ) libraries


%package devel
Group:          development
Summary:        zlib-like interface to fast block compression (LZ4 or FastLZ) libraries devel package
Requires:       %name = %version
#BuildArch:      noarch
%description devel
zlib-like interface to fast block compression (LZ4 or FastLZ) libraries  - devel package

%prep
%setup -q

%build
%configure
make %{?_smp_mflags}

%install
find
#%%make_install
make  DESTDIR="%buildroot" install

mkdir -p $RPM_BUILD_ROOT/%_docdir

%if 0%{?suse_version}
mv $RPM_BUILD_ROOT/%_docdir/../%name $RPM_BUILD_ROOT/%_docdir
%endif

#rm  $RPM_BUILD_ROOT/%_libdir/*.a

%post

%postun

%files
%defattr(-,root,root)
%dir %_docdir/%name
%doc %_docdir/%name/*
%_bindir/*
%_libdir/*.so.*

%files devel
%defattr(-,root,root)
%_includedir/*
%_libdir/*.la
%_libdir/*.so
%_libdir/*.a

%changelog

