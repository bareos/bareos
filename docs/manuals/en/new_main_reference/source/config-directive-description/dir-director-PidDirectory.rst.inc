This directive is optional and specifies a directory in which the Director may put its process Id file. The process Id file is used to shutdown Bareos and to prevent multiple copies of Bareos from running simultaneously. Standard shell expansion of the Directory is done when the configuration file is read so that values such as $HOME will be properly expanded.

The PID directory specified must already exist and be readable and writable by the Bareos daemon referencing it.

Typically on Linux systems, you will set this to: :file:`/var/run`. If you are not installing Bareos in the system directories, you can use the Working Directory as defined above.

