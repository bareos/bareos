#
# spec file for package mingw-debugsrc-devel
#
# Copyright (c) 2014 Bareos GmbH & Co. KG
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.bareos.org
#
#!BuildIgnore: post-build-checks

Name:           mingw-debugsrc-devel
Version:        14.3.0
Release:        0
Summary:        bareos
License:        LGPLv2+
Group:          Development/Libraries
URL:            http://bareos.org
Source0:        bareos-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
BuildArch:      noarch

%description

%prep
%setup -q -n bareos-%{version}

%build

%install
cp  -av ../bareos-* $RPM_BUILD_ROOT/

%post

%postun

%files
%defattr(-,root,root)
/bareos-*

%changelog
