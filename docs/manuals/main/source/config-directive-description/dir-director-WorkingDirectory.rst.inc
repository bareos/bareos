This directive is optional and specifies a directory in which the Director may put its status files. This directory should be used only by Bareos but may be shared by other Bareos daemons. Standard shell expansion of the directory is done when the configuration file is read so that values such as :file:`$HOME`

will be properly expanded.

The working directory specified must already exist and be readable and writable by the Bareos daemon referencing it.

