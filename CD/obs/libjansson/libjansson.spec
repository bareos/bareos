#
# spec file for package libjansson
#
# Copyright (c) 2014 SUSE LINUX Products GmbH, Nuernberg, Germany.
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


Name:           libjansson
%define lname   libjansson4
Summary:        C library for encoding, decoding and manipulating JSON data
License:        MIT
Group:          Development/Libraries/C and C++
Version:        2.7
Release:        0
Url:            http://digip.org/jansson/

#Git-Clone:     git://github.com/akheron/jansson
Source:         http://www.digip.org/jansson/releases/jansson-%version.tar.bz2
Source1:        baselibs.conf
Source2:        http://www.digip.org/jansson/releases/jansson-%version.tar.bz2.asc
Source3:        %name.keyring
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
BuildRequires:  pkgconfig

%description
Jansson is a C library for encoding, decoding and manipulating JSON data.
It features:
 * Simple and intuitive API and data model
 * Comprehensive documentation
 * No dependencies on other libraries
 * Full Unicode support (UTF-8)
 * Extensive test suite

%package -n %lname
Summary:        C library for encoding, decoding and manipulating JSON data
Group:          Development/Libraries/C and C++

%description -n %lname
Jansson is a C library for encoding, decoding and manipulating JSON data.
It features:
 * Simple and intuitive API and data model
 * Comprehensive documentation
 * No dependencies on other libraries
 * Full Unicode support (UTF-8)
 * Extensive test suite

%package devel
Summary:        Development files for libjansson
Group:          Development/Libraries/C and C++
Requires:       %lname = %version
Provides:       jansson-devel = %version

%description devel
Jansson is a C library for encoding, decoding and manipulating JSON data.
It features:
 * Simple and intuitive API and data model
 * Comprehensive documentation
 * No dependencies on other libraries
 * Full Unicode support (UTF-8)
 * Extensive test suite

%prep
%setup -q -n jansson-%{version}

%build
%configure --disable-static
make %{?_smp_mflags}

%install
%make_install
rm -f "%buildroot/%_libdir"/*.la;

%post -n %lname -p /sbin/ldconfig

%postun -n %lname -p /sbin/ldconfig

%files -n %lname
%defattr(-,root,root)
%{_libdir}/libjansson.so.4*

%files devel
%defattr(-,root,root)
%{_includedir}/jansson.h
%{_includedir}/jansson_config.h
%{_libdir}/libjansson.so
%{_libdir}/pkgconfig/jansson.pc

%changelog
