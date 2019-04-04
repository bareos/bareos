Normally, if you modify the FileSet Include or Exclude lists, the next backup will be forced to a Full so that Bareos can guarantee that any additions or deletions are properly saved.

We strongly recommend against setting this directive to yes, since doing so may cause you to have an incomplete set of backups.

If this directive is set to :strong:`yes`, any changes you make to the FileSet Include or Exclude lists, will not force a Full during subsequent backups.

