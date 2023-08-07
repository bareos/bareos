.. _bpipe:

bpipe Plugin
~~~~~~~~~~~~

.. index::
   single: Plugin; bpipe

The bpipe plugin is a generic pipe program, that simply transmits the data from a specified program to Bareos for backup, and from Bareos to a specified program for restore. The purpose of the plugin is to provide an interface to any system program for backup and restore. That allows you, for example, to do database backups without a local dump. By using different command lines to bpipe, you can backup any kind of data (ASCII or binary) depending on the program called.

On Linux, the Bareos bpipe plugin is part of the **bareos-filedaemon** package and is therefore installed on any system running the filedaemon.

The bpipe plugin is so simple and flexible, you may call it the "Swiss Army Knife" of the current existing plugins for Bareos.

The bpipe plugin is specified in the Include section of your Job’s FileSet resource.

.. code-block:: bareosconfig
   :caption: bpipe fileset

   FileSet {
     Name = "MyFileSet"
     Include {
       Options {
         Signature = XXH128
         Compression = LZ4
       }
       Plugin = "bpipe"
                ":file=<filepath>"
                ":reader=<readprogram>"
                ":writer=<writeprogram>"
     }
   }

The syntax and semantics of the Plugin directive require the first part of the string up to the colon to be the name of the plugin. Everything after the first colon is ignored by the File daemon but is passed to the plugin. Thus the plugin writer may define the meaning of the rest of the string as he wishes. The full syntax of the plugin directive as interpreted by the bpipe plugin is:

Since Bareos :sinceVersion:`20: Multiline Strings` the plugin string can be spread over multiple lines using quotes as shown above.

.. code-block:: bareosconfig
   :caption: bpipe directive

   Plugin = "<plugin>:file=<filepath>:reader=<readprogram>:writer=<writeprogram>"

plugin
   is the name of the plugin with the trailing -fd.so stripped off, so in this case, we would put bpipe in the field.

filepath
   specifies the namespace, which for bpipe is the pseudo path and filename under which the backup will be saved. This pseudo path and filename will be seen by the user in the restore file tree. For example, if the value is :strong:`/MySQL/mydump.sql`, the data backed up by the plugin will be put under that "pseudo" path and filename. You must be careful to choose a naming convention that is unique to avoid a conflict with a path and filename that actually
   exists on your system.

readprogram
   for the bpipe plugin specifies the "reader" program that is called by the plugin during backup to read the data. bpipe will call this program by doing a popen on it.

writeprogram
   for the bpipe plugin specifies the "writer" program that is called by the plugin during restore to write the data back to the filesystem.
   To simply create a file, containing the data of the backup, following command can by used on a Unix system:

   .. code-block:: shell

      writer=sh -c 'cat >/var/tmp/bpipe.data'


Please note that the two items above describing the "reader" and "writer", these programs are "executed" by Bareos, which means there is no shell interpretation of any command line arguments you might use. If you want to use shell characters (redirection of input or output, ...), then we recommend that you put your command or commands in a shell script and execute the script. In addition if you backup a file with reader program, when running the writer program during the restore, Bareos will not
automatically create the path to the file. Either the path must exist, or you must explicitly do so with your command or in a shell script.

See the examples about :ref:`backup-postgresql` and :ref:`backup-mysql`.
