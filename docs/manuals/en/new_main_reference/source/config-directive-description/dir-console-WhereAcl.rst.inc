This directive permits you to specify where a restricted console can restore files. If this directive is not specified, only the default restore location is permitted (normally :file:`/tmp/bareos-restores`. If :strong:`*all*` is specified any path the user enters will be accepted. Any other value specified (there may be multiple :strong:`Where ACL`\  directives) will restrict the user to use that path. For example, on a Unix system, if you specify
"/", the file will be restored to the original location.

