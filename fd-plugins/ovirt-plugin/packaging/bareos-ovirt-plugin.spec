Name:		bareos-ovirt-plugin
Version:	0.1
Release:	1%{?dist}
Summary:	Bareos Ovirt Plugin

Group:		Productivity/Archiving/Backup
License:	GPL
URL:		http://eurotux.com
Source:	    bareos-ovirt-plugin.tar.gz

Requires:	bareos-filedaemon-python-plugin
Requires:   python-ovirt-engine-sdk4
Requires:   openssl
Requires:   python-pycurl
Requires:   python-lxml


%description
Ovirt backup plugin for Bareos uses the Ovirt API to take a snapshot of running VMs and
creates a full backup.
It also supports the restore to fresh new VM.

%prep
%setup -q -n bareos-ovirt-plugin


%build


%install
rm -rf $RPM_BUILD_ROOT

%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/bareos/plugins/

install BareosFdPluginOvirt.py $RPM_BUILD_ROOT%{_libdir}/bareos/plugins/
install bareos-fd-ovirt.py $RPM_BUILD_ROOT%{_libdir}/bareos/plugins/

%files
%defattr(-,root,root)
%dir %{_libdir}/bareos/
%{_libdir}/bareos/plugins/


%changelog
* Mon Oct 08 2018 Carlos Rodrigues <cmar@eurotux.com> 0.1-1
- initial version of bareos-ovirt-plugin
