The writebootstrap directive specifies a file name where Bareos will write a bootstrap file for each Backup job run. This directive applies only to Backup Jobs. If the Backup job is a Full save, Bareos will erase any current contents of the specified file before writing the bootstrap records. If the Job is an Incremental or Differential save, Bareos will append the current bootstrap record to the end of the file.

Using this feature, permits you to constantly have a bootstrap file that can recover the current state of your system. Normally, the file specified should be a mounted drive on another machine, so that if your hard disk is lost, you will immediately have a bootstrap record available. Alternatively, you should copy the bootstrap file to another machine after it is updated. Note, it is a good idea to write a separate bootstrap file for each Job backed up including the job that backs up your
catalog database.

If the bootstrap-file-specification begins with a vertical bar (|), Bareos will use the specification as the name of a program to which it will pipe the bootstrap record. It could for example be a shell script that emails you the bootstrap record.

Before opening the file or executing the specified command, Bareos performs :ref:`character substitution <character substitution>` like in RunScript directive. To automatically manage your bootstrap files, you can use this in your JobDefs resources: 

.. literalinclude:: /include/config/DirJobWriteBootstrap1.conf



For more details on using this file, please see chapter :ref:`BootstrapChapter`.

