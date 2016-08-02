
Name:          bareos-webui
#Provides:
Version:       15.2.1
Release:       0%{?dist}
Summary:       Bareos Web User Interface

Group:         Productivity/Archiving/Backup
License:       AGPL-3.0+
URL:           http://www.bareos-webui.org/
Vendor:        The Bareos Team
Source:        %{name}-%{version}.tar.gz
BuildRoot:     %{_tmppath}/%{name}-%{version}-build
BuildArch:     noarch

BuildRequires: autoconf automake bareos-common

Requires: php >= 5.3.3
Requires: php-bz2
Requires: php-ctype
Requires: php-curl
Requires: php-date
Requires: php-dom
Requires: php-fileinfo
Requires: php-filter
Requires: php-gettext
Requires: php-gd
Requires: php-hash
Requires: php-iconv
Requires: php-intl
Requires: php-json
%if 0%{?suse_version}
%else
Requires: php-libxml
%endif
Requires: php-mbstring
Requires: php-mysql
Requires: php-openssl
Requires: php-pcre
Requires: php-pdo
#Requires: php-pecl
Requires: php-pgsql
Requires: php-reflection
Requires: php-session
Requires: php-simplexml
Requires: php-spl
Requires: php-xml
Requires: php-xmlreader
Requires: php-xmlwriter
Requires: php-zip

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
#Recommends: php-pgsql php-mysql php-sqlite
#Suggests:   postgresql-server mysql sqlite3
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
%endif

#define serverroot #(/usr/sbin/apxs2 -q datadir 2>/dev/null || /usr/sbin/apxs2 -q PREFIX)/htdocs/

%description
Bareos - Backup Archiving Recovery Open Sourced. \
Bareos is a set of computer programs that permit you (or the system \
administrator) to manage backup, recovery, and verification of computer \
data across a network of computers of different kinds. In technical terms, \
it is a network client/server based backup program. Bareos is relatively \
easy to use and efficient, while offering many advanced storage management \
features that make it easy to find and recover lost or damaged files. \
Bareos source code has been released under the AGPL version 3 license.

This package contains the webui (Bareos Web User Interface).

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

# write version to version file
echo %version | grep -o  '[0-9]\{1,3\}\.[0-9]\{1,3\}\.[0-9]\{1,3\}' > %{buildroot}/%_datadir/%name/version.txt

# With the introduction of config subdirectories (bareos-16.2)
# some config files have been renamed (or even splitted into multiple files).
# However, bareos is still able to work with the old config files,
# but rpm renames them to *.rpmsave.
# To keep the bareos working after updating to bareos-16.2,
# we implement a workaroung:
#   * post: if the old config exists, make a copy of it.
#   * (rpm exchanges files on disk)
#   * posttrans:
#       if the old config file don't exists but we have created a backup before,
#       restore the old config file.
#       Remove our backup, if it exists.

%define post_backup_file() \
if [ -f  %1 ]; then \
      cp -a %1 %1.rpmupdate.%{version}.keep; \
fi; \
%nil

%define posttrans_restore_file() \
if [ ! -e %1 -a -e %1.rpmupdate.%{version}.keep ]; then \
   mv %1.rpmupdate.%{version}.keep %1; \
fi; \
if [ -e %1.rpmupdate.%{version}.keep ]; then \
   rm %1.rpmupdate.%{version}.keep; \
fi; \
%nil

%post
%post_backup_file /etc/bareos/bareos-dir.d/webui-consoles.conf
%posttrans_restore_file /etc/bareos/bareos-dir.d/webui-consoles.conf
%post_backup_file /etc/bareos/bareos-dir.d/webui-profiles.conf
%posttrans_restore_file /etc/bareos/bareos-dir.d/webui-profiles.conf
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
%config(noreplace) /etc/bareos-webui/configuration.ini
%config(noreplace) %attr(644,root,root) /etc/bareos/bareos-dir.d/console/admin.conf.example
%config(noreplace) %attr(644,root,root) /etc/bareos/bareos-dir.d/profile/webui-admin.conf
%config(noreplace) %{_apache_conf_dir}/bareos-webui.conf

