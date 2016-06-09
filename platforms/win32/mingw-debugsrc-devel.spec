#
# spec file for package mingw-debugsrc-devel
#
# Copyright (c) 2014-2015 Bareos GmbH & Co. KG
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
Version:        15.2.4
Release:        0
Summary:        bareos
License:        LGPLv2+
Group:          Development/Libraries
URL:            http://bareos.org
Source0:        bareos-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
BuildArch:      noarch
Provides:       %name = %version
Provides:       %name-postvista = %version
Provides:       %name-postvista-debug = %version
#Provides:       %name-prevista = %version
#Provides:       %name-prevista-debug = %version

%description

%prep
%setup -q -n bareos-%{version}

%build

%install
cp  -av ../bareos-%{version} $RPM_BUILD_ROOT/

%post

%postun

%files
%defattr(-,root,root)
/bareos-*



%changelog
