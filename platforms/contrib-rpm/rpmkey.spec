# rpm public key package
# Copyright (C) 2006 Free Software Foundation Europe e.V.
#
#

# replace the string yournam with your name
# first initial and lastname, ie. sbarninger
%define pubkeyname yourname

# replace below with your name and email address
Packager: Your Name <your-email@site.org>

Summary: The %{pubkeyname} rpm public key
Name: rpmkey-%{pubkeyname}
Version: 0.1
Release: 1
License: GPL v2
Group: System/Packages
Source0: %{pubkeyname}.asc
BuildArch: noarch
BuildRoot: %{_tmppath}/%{name}-root

%define gpgkeypath /etc/bacula/pubkeys

%description
The %{pubkeyname} rpm public key. If you trust %{pubkeyname} component
and you want to import this key to the RPM database, then install this 
RPM. After installing this package you may import the key into your rpm
database, as root:

rpm --import %{gpgkeypath}/%{pubkeyname}.asc

%prep
%setup -c -T a1

%build

%install
mkdir -p %{buildroot}%{gpgkeypath}
cp -a %{SOURCE0} %{buildroot}%{gpgkeypath}/

%files
%defattr(-, root, root)
%{gpgkeypath}/%{pubkeyname}.asc


%changelog
* Sat Aug 19 2006 D. Scott Barninger <barninger@fairfieldcomputers.com
- change key directory to /etc/bacula/pubkeys
* Sun Jul 16 2006 D. Scott Barninger <barninger@fairfieldcomputers.com
- initial spec file
