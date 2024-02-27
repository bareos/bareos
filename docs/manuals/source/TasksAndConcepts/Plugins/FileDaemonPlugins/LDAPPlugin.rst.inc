LDAP Plugin
~~~~~~~~~~~

.. index::
   single: Plugin; ldap

This plugin is intended to backup (and restore) the contents of a LDAP server.
It uses normal LDAP operation for this. The package **bareos-filedaemon-ldap-python-plugin** 
(:sinceVersion:`15.2.0: LDAP Plugin`) contains an example configuration file,
that must be adapted to your environment.

Please note that the plugin was tested against an OpenLDAP server. Other LDAP servers
may behave differently and there might be problems when backing up or restoring objects.
Most notably, it will not be possible to restore objects to an Active Directory server.

On restore, if the object to be restored already exists on the LDAP server, it will
be deleted first, then restored from the backup. This could cause problems if your
LDAP server uses referential integrity (e.g. if a user object is
restored, the LDAP server might remove the user from all groups when it is
being deleted and recreated during the restore process).
