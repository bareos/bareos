
Name:          bareos-console-web
Provides:      barrossa
Version:       0.1
Release:       0%{?dist}
Summary:       Web interface of a Bareos backup system

Group:      Productivity/Archiving/Backup
License:    AGPL-3.0+
#URL:        http://webacula.sourceforge.net/
Source:     %{name}_%{version}.tar.gz
BuildRoot:  %{_tmppath}/%{name}-%{version}-build
BuildArch:  noarch

BuildRequires: autoconf automake
BuildRequires: sudo

Requires: bareos-bconsole bareos-common
Requires: php >= 5.3.3
Requires: php-ZendFramework >= 2.2.0
Requires: php-pdo
#Requires: php-json
#Requires: php-pcre
#Requires: php-gd
#Requires: php-xml
# * PHP PDO Extension
# * PHP intl Extension
# * PHP PHAR Extension
# * PHP DATE Extension
# * PHP OpenSSL Extension
Requires: sudo

%if 0%{?suse_version}
BuildRequires: apache2
# /usr/sbin/apxs2
BuildRequires: apache2-devel
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
Barbossa - web interface of a Bareos backup system.
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
# if command a2enmod exists, 
# use it to enable Apache rewrite module
LOG=/var/log/bareos-console-web-install.log
echo "`date`: BEGIN bareos-console-web init" >> $LOG
which a2enmod >/dev/null && a2enmod rewrite >> $LOG 2>&1
export WEBACULA_INTERACTIVE="no"
# if /usr/sbin/webacula-config init >> $LOG 2>&1; then
#     echo "SUCCESS: webacula-config init, see $LOG"
# else
#     echo "FAILED: webacula-config init, see $LOG"
# fi

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%doc AUTHORS.txt CONTRIBUTE.md README.md LICENSE.txt
%doc doc/
%{_datadir}/%{name}/
#attr(-, #daemon_user, #daemon_group) #{_datadir}/#{name}/data
#{_sysconfdir}/#{name}
%config(noreplace) %{_apache_conf_dir}/bareos-console-web.conf
#config(noreplace) #{_sysconfdir}/#{name}/config.ini
#/usr/sbin/webacula-config

# /etc/sudoers.d/ should not belong to this package,
# but is does currently not exist on most distributions
dir /etc/sudoers.d/
# sudo requires permissions 440 and config files without any "."
%attr(440,root,root) %config(noreplace) /etc/sudoers.d/bareos-console-web-bconsole
