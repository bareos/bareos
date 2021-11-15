Bareos PAM Integration
======================

PAM, the *Pluggable Authentication Modules* used by Linux
provide dynamic authentication support for applications and services in a Linux system.

PAM authentication is included since Bareos >= 18.2, see https://docs.bareos.org/master/TasksAndConcepts/PAM.html#configuration

However, this supports only the authentication mechanism.
That means, the user must be known in the backend system used by PAM  (:file:`/etc/passwd`, LDAP or ...)
**and** the user has to exist in the Bareos Director.

The PAM implementation of Bareos is only used for authentication of console connections.
Console access is only provided by the Bareos Director.

PAM Configuration
-----------------

By default, PAM configuration files resides in the directory :file:`/etc/pam.d/`.

Authentication using PAM is requested by a service name.
The Bareos Director uses the service name **bareos**.
The corresponding configuration file is :file:`/etc/pam.d/bareos`.
If this file does not exist, PAM uses the fallback file :file:`/etc/pam.d/other`.

Often PAM is offered by system services, meaning the calling process has *root* priviliges.
In contrast, the Bareos Director on Linux runs as user *bareos*,
therefore by default it might not offer all required functionality.

Known Limitations of PAM Modules
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:pam_unix:
    When authenticating with pam_unix, it tries to read system files,
    including the file :file:`/etc/shadow`.
    By default, the user *bareos* do not have the permission to read this file.
    If this functionality is required, adapt the priviliges accordingly
    (e.g. add the user bareos to the group owning the file).

:pam_ldap:
    When using pam_ldap make sure
    your configuration does not require the rootbinddn and :file:`/etc/pam_ldap.secret` settings.
    Instead use the binddn/bindpw settings (if required).

Another limitation is, that some PAM modules do not ask for a login name.
They only ask for the password.
As result, the bconsole command (without the -p parameter)
will only ask for a password, but not the login name.
As the user is unknown, the authentication fails.

One method to circumvent this
is to provide the PAM credentials to the bconsole by an extra credentials file.
This credentials file is adressed by the bconsole -p parameter.

Testing PAM Authentication
~~~~~~~~~~~~~~~~~~~~~~~~~~

If you have configured the PAM settings for Bareos (:file:`/etc/pam.d/bareos`),
you can test it outside of Bareos.

Make sure, the program **pamtester** (package: pamtester on Debian) is installed.

In this example, we will test, if the user USER_TO_TEST can be successfully authenticated by PAM.

::

   # switch to user bareos, to run with the same priviliges as bareos-dir
   su - bareos -s /bin/bash

   # use pamtester to try authentication by the PAM service bareos
   pamtester bareos USER_TO_TEST authenticate


Pamtester will ask for a password.
After providing this,
it will print if the user can be authenticated successfully (output: "pamtester: successfully authenticated") or not.

Testing PAM Authentication of the Bareos Director
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After PAM has been successfully tested using pamtester,
it can be tested using the bareos-dir.

Configure the Bareos Director as described by https://docs.bareos.org/master/TasksAndConcepts/PAM.html#configuration.

Create a bconsole configuration file, name it :file:`bconsole-pam.conf`.

Test to connect via bconsole to the bareos-dir::

   $ bconsole -c bconsole-pam.conf
   Connecting to Director localhost:9101
    Encryption: ECDHE-PSK-CHACHA20-POLY1305
   Login:USER_TO_TEST
   Passwort: ********
   1000 OK: bareos-dir Version: 19.1.2 (01 February 2019)
   You are logged in as: USER_TO_TEST

   Enter a period to cancel a command.
   *

After successfully testing with bconsole, the Bareos WebUI can be tested.

Reuse your existing PamConsole or create an additional one::

   Console {
     Name = "pam-webui"
     Password = "secret"
     UsePamAuthentication = yes
     TLS Enable = no
   }

As PHP does not yet support TLS-PSK, the setting ``TLS Enable = no`` is required.
Therefore it is advised to run the Bareos Director and Bareos WebUI on the same host.

Configure the ``pam_console_name`` and the ``pam_console_password`` in :file:`/etc/bareos-webui/directors.ini`
as defined in the Console Resource, see above.

You may want to add an additional Bareos Director section like this or add the
parameters to an already existing one if heading for PAM usage only.

::

   [localhost-dir-pam]
   enabled              = "yes"
   diraddress           = "localhost"
   dirport	        = 9101
   tls_verify_peer      = false
   server_can_do_tls    = false
   server_requires_tls  = false
   client_can_do_tls    = false
   client_requires_tls  = false
   pam_console_name     = "pam-webui"
   pam_console_password = "secret"

PAM users require a dedicated User Resource, see https://docs.bareos.org/master/Configuration/Director.html#user-resource .

A User Resource for a user named `alice` in the file :file:`/etc/bareos/bareos-dir.d/user/alice.conf` could
look like folllowing::

   User {
      Name = "alice"
      Profile = "webui-admin"
   }

Now you should be able to login using PAM user `alice` for example.


Auto Create Bareos Users
~~~~~~~~~~~~~~~~~~~~~~~~

Until now, only PAM users that are already configured in the Bareos Director can login.

The PAM script ``pam_exec_add_bareos_user.py`` can circumvent this.

It can be integrated into the Bareos PAM configuration by ``pam_exec`` .

This version of the script requires at least Bareos >= 19.2.4.

Installation
^^^^^^^^^^^^

* Verify that ``pam_exec`` is installed. On Debian it is part of the PAM base package **libpam-modules**.
* Install ``python-bareos``.
* Copy ``pam_exec_add_bareos_user.py`` to :file:`/usr/local/bin/`.

Create a Bareos console for user pam-adduser:

::

   Console {
     Name       = "pam-adduser"
     Password   = "secret"
     CommandACL = ".api", ".profiles", ".users", "configure", "version"
     TlsEnable  = no
   }


Add a pam_exec line to the PAM configuration file :file:`/etc/pam.d/bareos`.
This example uses pam_ldap to authenticate.

::

   auth     requisite           pam_ldap.so
   auth     [default=ignore]    pam_exec.so /usr/local/bin/pam_exec_add_bareos_user.py --name pam-adduser --password secret --profile webui-admin

Make sure, an unsuccessful authentication ends before pam_exec.so.
In this example, this is done by the *requisite* keyword (when not successful, stop executing the PAM stack).

Using this, a user who successfully authenticates against LDAP, will be created as Bareos user with ACLs as defined in profile *webui-admin*.
