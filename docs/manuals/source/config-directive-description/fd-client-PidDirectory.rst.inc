This directive specifies a directory in which the File Daemon may put its process Id file files. The process Id file is used to shutdown Bareos and to prevent multiple copies of Bareos from running simultaneously.

The Bareos file daemon uses a platform specific default value, that is defined at compile time. Typically on Linux systems, it is set to :file:`/var/lib/bareos/` or :file:`/var/run/`.

