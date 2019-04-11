This directive applies only to a Restore job and specifies what happens when Bareos wants to restore a file or directory that already exists. You have the following options for replace-option:

always
   :index:`\ <single: always>` when the file to be restored already exists, it is deleted and then replaced by the copy that was backed up. This is the default value.

ifnewer
   :index:`\ <single: ifnewer>` if the backed up file (on tape) is newer than the existing file, the existing file is deleted and replaced by the back up.

ifolder
   :index:`\ <single: ifolder>` if the backed up file (on tape) is older than the existing file, the existing file is deleted and replaced by the back up.

never
   :index:`\ <single: never>` if the backed up file already exists, Bareos skips restoring this file.

