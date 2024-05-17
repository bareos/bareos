.. _section-UseMailxAsBsmtpReplacement:

Use Heirloom mailx as :command:`bsmtp` replacement
--------------------------------------------------

.. index::
   single: howto; bsmtp replacement; mailx
   pair: Mailcommand; mailx
   pair: Operatorcommand; mailx
   single: bareos-bsmtp
   see: bsmtp; bareos-bsmtp

While Bareos :command:`bsmtp` is useful to transmit message when a local or trusted :strong:`MTA` is
available. It lacks some feature like the availability to do :command:`smtp auth` or :command:`TLS`.

This section will show you how to configure Bareos to use the command heimdal :command:`mailx`.

.. warning::

   :command:`mailx` never returns an error code even if the mail is not delivered!

   Compared to :command:`bsmtp` usage with pipe mode in `WriteBootStrap`: :strong:`the job will
   then never fail!`

   You may be warned in job log when you use ``RunScript``, but ``WriteBootStrap`` will silently
   fail.


.. note::

   See section below how to detect errors.


Prepare configuration for mailx
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

As :command:`mailx` by default reads its default configuration from file :command:`~/.mailrc`,
we will create this :command:`.mailrc` file in the :config:option:`dir/director/WorkingDirectory`.
You have to set the mode to :command:`0600` and set the owner to userid :strong:`bareos`.

.. code-block:: shell-session
   :caption: .mailrc content sample

   root@host:~# cat << _EOT_ > /var/lib/bareos/.mailrc
   set smtp=smtp://smtp.maildomain.tld:587
   set smtp-use-starttls
   # set ssl-verify=ignore
   set from="Bareos <bareos@maildomain.tld>"
   set smtp-auth-user=smtp-username>
   set smtp-auth-password=smtp-password
   set smtp-auth=login
   # set sendcharsets=UTF-8
   # set ttycharset=UTF-8
   # set encoding=8bit
   _EOT_

   root@host:~# chown bareos:bareos /var/lib/bareos/.mailrc
   root@host:~# chmod 0600 /var/lib/bareos/.mailrc

.. note::

   To test this configuration as user :strong:`bareos` see example below.

.. code-block:: shell-session
   :caption: Run :command:`mailx` command as bareos user

   # Start the shell as bareos user
   root@host:~# su -s /bin/bash - bareos

   # run the command as user bareos:
   bareos@host:~# mailx -s "Bareos test" dest@maildomain.tld <<< "
   This is a simple test to check that default ~/.mailrc file is read by mailx
   .
   "


This file allows to use sensitive data without leaking them on command line, but you can also set
most options on the command line or in Bareos configuration.


.. note::

   Please check the fine manual of :command:`mailx` to learn more about the parameters that may
   be relevant in your infrastructure.


We need now to replace :command:`bsmtp` by :command:`mailx` in different configuration files:

.. code-block:: bareosconfig
   :caption: MailCommand in bareos-dir.d/messages/Daemon.conf

   Messages {
      Name = Daemon
      Description = "Message delivery for daemon messages (no job)."
      mailcommand = "/usr/bin/mailx -s \"Bareos daemon message\" %r"
      mail = bareos@maildomain.tld = all, !skipped, !audit
      console = all, !skipped, !saved, !audit
      append = "/usr/local/var/log/bareos/bareos.log" = all, !skipped, !audit
      append = "/usr/local/var/log/bareos/bareos-audit.log" = audit
   }


.. code-block:: bareosconfig
   :caption: MailCommand and OperatorCommand in bareos-dir.d/messages/Standard.conf

   Messages {
      Name = Standard
      Description = "Reasonable message delivery -- send most everything to email address and to the console."
      operatorcommand = "/usr/bin/mailx -s \"Bareos: Intervention needed for %j\" %r"
      mailcommand = "/usr/bin/mailx -s \"Bareos: %t %e of %c %l\" %r"
      operator = bareos@maildomain.tld = mount
      mail = bareos@maildomain.tld = all, !skipped, !saved, !audit
      console = all, !skipped, !saved, !audit
      append = "/var/log/bareos/bareos.log" = all, !skipped, !saved, !audit
      catalog = all, !skipped, !saved, !audit
   }



Additional usage
~~~~~~~~~~~~~~~~

Like :command:`bsmtp`, :command:`mailx` can receive data by pipe `|`. If you are using this feature
in your configuration like in the default :command:`BackupCatalog` you can setup the following:

.. code-block:: bareosconfig
   :caption: :command:`mailx` for WriteBootStrap in bareos-dir.d/job/BackupCatalog.conf

   Job {
      Name = "BackupCatalog"
      Description = "Backup the catalog database (after the nightly save)"
      JobDefs = "DefaultJob"
      Level = Full
      FileSet="Catalog"
      Schedule = "WeeklyCycleAfterBackup"

      # This creates an ASCII copy of the catalog
      # Arguments to make_catalog_backup are:
      #  make_catalog_backup <catalog-name>
      RunBeforeJob = "/usr/lib/bareos/scripts/make_catalog_backup MyCatalog"

      # This deletes the copy of the catalog
      RunAfterJob  = "/usr/lib/bareos/scripts/delete_catalog_backup MyCatalog"

      # This sends the bootstrap via mail for disaster recovery.
      # Should be sent to another system, please change recipient accordingly
      Write Bootstrap = "|/usr/bin/mailx -s \"Bootstrap for Job %j\" bareos@maildomain.tld"
      Priority = 11                   # run after main backup
   }



Detecting errors
~~~~~~~~~~~~~~~~

There are several ways to check if errors occurred during mail transport with :command:`mailx`.

dead.letter presence
^^^^^^^^^^^^^^^^^^^^

In case of delivery error, mails in error are kept in working directory file
:command:`/var/lib/bareos/dead.letter`.

You may want to monitor the existence of this file. Read and forward its content and clear it after.


mailx error in joblog
^^^^^^^^^^^^^^^^^^^^^

When using :command:`mailx` with ``RunScript`` you may see errors like the following example:

.. code-block:: none
   :caption: example of mailx error during RunScript execution

   bareos-dir JobId 16113: shell command: run AfterJob "/usr/bin/mailx
   -s "Bareos: bootstrap for Job catalog.2024-04-30_16.28.40_24"
   -a /var/lib/bareos/storage/bsr/bareos-fd.catalog.bsr
   bareos@localhost.localdomain"
   bareos-dir JobId 16113: AfterJob: smtp-server: 535 5.7.8 Error: authentication failed:
   (reason unavailable)
   bareos-dir JobId 16113: AfterJob: . . . message not sent.



:command:`mailx` error with systemd bareos-dir unit
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

On :command:`systemd` managed hosts, the |dir| service will log errors that occur.

.. code-block:: none
   :caption: example journalctl output of bareos-dir unit

   journalctl -b -u bareos-dir
   bareos-dir[30027]: Message delivery ERROR: Mail prog: smtp-server: 535 5.7.8
                                              Error: authentication failed: (reason unavailable)
   bareos-dir[30027]: Message delivery ERROR: Mail prog: . . . message not sent.
