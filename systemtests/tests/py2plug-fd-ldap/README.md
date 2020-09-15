# Systemtest for the Filedaemon's Python LDAP Plugin

## What the test does
The test will set up a bunch of objects below a given basedn on your LDAP server inside "ou=backup".
These objects will be backed up and then restored to a different location "ou=restore".
Finally both subtrees will be compared.

Besides simple DNs we also test DNs with special characters (i.e. quotes, backslashes, newlines, etc.) and UTF-8 characters.

## Requirements to run
For this to work you will have to build Bareos with the Python 2 plugin for the Bareos FD.
The python code itself requires the python-ldap library.

To run a backup and a restore this test requires a working LDAP server.
If you have docker you can run the following command to set up a temporary server on your machine:
`docker run --rm -p 389:389 -p 636:636 --name my-openldap-container osixia/openldap:1.4.0`
This is the container the test was developed with.

You should configure the following CMake settings to point the test at your server:
* `SYSTEMTEST_LDAP_ADDRESS`
* `SYSTEMTEST_LDAP_BASEDN`
* `SYSTEMTEST_LDAP_BINDDN`
* `SYSTEMTEST_LDAP_PASSWORD`

Everything besides `SYSTEMTEST_LDAP_ADDRESS` defaults to the valies for the above container.

AFAICT this test will not work with Python 3.
