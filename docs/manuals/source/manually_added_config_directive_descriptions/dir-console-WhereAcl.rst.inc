This directive permits you to specify where a restricted console can restore files.
This ACL is only evaluated when changing the default restore path
specified in the restore job (typically :file:`/tmp/bareos-restores`).
If :strong:`*all*` is specified any path the user enters will be accepted.
Any other value specified
(there may be multiple :strong:`Where ACL`\  directives)
will restrict the user to use that path.
For example, on a Unix system, if you specify "/",
the file will be restored to the original location.
