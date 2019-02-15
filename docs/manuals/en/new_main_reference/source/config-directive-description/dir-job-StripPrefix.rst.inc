This directive applies only to a Restore job and specifies a prefix to remove from the directory name of all files being restored. This will use the :ref:`File Relocation <filerelocation>` feature.

Using ``Strip Prefix=/etc``, ``/etc/passwd`` will be restored to ``/passwd``

Under Windows, if you want to restore ``c:/files`` to ``d:/files``, you can use:



   .. literalinclude:: /_static/config/DirJobStripPrefix1.conf


