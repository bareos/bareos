Normally, if you modify
:config:option:`dir/FileSet/Include/File` or
:config:option:`dir/FileSet/Exclude/File`
of the FileSet Include or Exclude lists,
the next backup will be forced to a full
so that Bareos can guarantee that any additions or deletions are properly saved.

We strongly recommend against setting this directive to yes, since doing so may cause you to have an incomplete set of backups.

If this directive is set to :strong:`yes`, any changes you make to the FileSet Include or Exclude lists, will not force a Full during subsequent backups.
