This directive applies only to a Restore job and specifies a prefix to the directory name of all files being restored. This permits files to be restored in a different location from which they were saved. If Where is not specified or is set to backslash (/), the files will be restored to their original location. By default, we have set Where in the example configuration files to be /tmp/bareos-restores. This is to prevent accidental overwriting of your files.



   .. warning::

      To use Where on NDMP backups, please read :ref:`section-ndmp-where`
