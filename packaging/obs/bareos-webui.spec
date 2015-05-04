
Name:          bareos-webui
Provides:      bareos-console-web
Version:       14.2.1
Release:       0%{?dist}
Summary:       Bareos Web User Interface

Group:         Productivity/Archiving/Backup
License:       AGPL-3.0+
URL:           https://github.com/bareos/bareos-webui
Source:        %{name}-%{version}.tar.gz
BuildRoot:     %{_tmppath}/%{name}-%{version}-build
BuildArch:     noarch

BuildRequires: autoconf automake bareos-common

Requires: php >= 5.3.3
Requires: php-ZendFramework2 >= 2.2.0
#Requires: php-pdo
#Requires: php-json
#Requires: php-pcre
#Requires: php-gd
#Requires: php-xml
# * PHP PDO Extension
# * PHP intl Extension
# * PHP PHAR Extension
# * PHP DATE Extension
# * PHP OpenSSL Extension

%if 0%{?suse_version}
BuildRequires: apache2
# /usr/sbin/apxs2
BuildRequires: apache2-devel
BuildRequires: mod_php_any
#define _apache_conf_dir #(/usr/sbin/apxs2 -q SYSCONFDIR)
%define _apache_conf_dir /etc/apache2/conf.d/
%define daemon_user  wwwrun
%define daemon_group www
Requires:   apache
Requires:   cron
Requires:   mod_php_any
Recommends: php-pgsql php-mysql php-sqlite
Suggests:   postgresql-server mysql sqlite3
%else
#if 0#{?fedora} || 0#{?rhel_version} || 0#{?centos_version}
BuildRequires: httpd
# apxs2
BuildRequires: httpd-devel
%define _apache_conf_dir /etc/httpd/conf.d/
%define daemon_user  apache
%define daemon_group apache
Requires:   cronie
Requires:   httpd
Requires:   mod_php
Requires:   php-pgsql php-mysql
# not available?
#php-sqlite
%endif

#define serverroot #(/usr/sbin/apxs2 -q datadir 2>/dev/null || /usr/sbin/apxs2 -q PREFIX)/htdocs/

%description
Bareos Web User Interface.
Supports status overview about Jobs.

%prep
%setup -q

%build
#autoreconf -fvi
%configure
make

%install
# makeinstall macro does not work on RedHat
#makeinstall
make DESTDIR=%{buildroot} install

%post
a2enmod setenv &> /dev/null || true
a2enmod rewrite &> /dev/null || true
a2enmod php5 &> /dev/null || true

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc README.md LICENSE AGPL-3.0.txt AUTHORS
%doc doc/
%{_datadir}/%{name}/
#attr(-, #daemon_user, #daemon_group) #{_datadir}/#{name}/data
%dir /etc/bareos-webui
%config(noreplace) /etc/bareos-webui/directors.ini
%config(noreplace) %attr(644,root,root) /etc/bareos/bareos-dir.d/bareos-webui.conf
%config(noreplace) %{_apache_conf_dir}/bareos-webui.conf

