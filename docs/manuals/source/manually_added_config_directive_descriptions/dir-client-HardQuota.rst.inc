The amount of data determined by the Hard Quota directive sets the hard limit of backup space that cannot be exceeded. This is the maximum amount this client can back up before any backup job will be aborted.

If the Hard Quota is exceeded, the running job is terminated:



   .. literalinclude:: /include/config/DirClientHardQuota1.conf
      :language: bareosconfig


