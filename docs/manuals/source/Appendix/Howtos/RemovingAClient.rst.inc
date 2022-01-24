.. _section-RemoveClient:

Removing a Client
-----------------

Removing a client can mean several things in Bareos context. You can disable the client so no jobs that use this client will run anymore. You can also remove the client’s configuration so that no job can use the client anymore. However, you will still have the client in your catalog unless you explicitly remove it.

Disable a client
~~~~~~~~~~~~~~~~

It is possible to disable a client either by calling :bcommand:`disable client=<client-name>` or by setting :config:option:`dir/client/Enabled`\  to no. While the configuration change will persist across restarts, the command just disables the client temporarily.

When a client is disabled, the scheduler will not run any job for this client anymore, but you can still restore to it or manually trigger a job.

Remove configuration for client
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you want to remove the client permanently, you can also completely remove the client from your configuration. This will make sure that no job (neither backup nor restore) can use that client machine. After removing the Client Resource from the director configuration, you will also need to change or remove all Job and JobDefs Resources in the Director configuration that reference the removed client.

Removing the client configuration does not remove the backed up data or the file and job records in the catalog for the client. You can still restore the data that has been backed up from the client to a different client.

Remove catalog records related to the client
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Removing the client from the configuration will leave you with catalog records concerning the client. If you don’t want to restore any data backed up from the client anymore, you can call :bcommand:`purge jobs client=<client-name>`. This will remove all job and file records for the named client from the catalog. However, it will not remove the data from the volumes.



.. warning::

   After removing the job and file records you will be unable to restore the client's data.
   The :bcommand:`purge` command ignores retention policies, so please take careful.


As soon as all jobs for a client have been removed from the catalog that client record becomes orphaned. Orphaned client records usually stay in your database indefinitely, but if you want them removed for cosmetic reasons you can do so using "Check for orphaned Client records" in :command:`bareos-dbcheck`.

By default :command:`bareos-dbcheck` only lists the orphaned clients it finds. You need to enable the modify database flag to make it actually change the database.
