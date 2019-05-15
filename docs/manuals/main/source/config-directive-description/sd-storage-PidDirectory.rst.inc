This directive specifies a directory in which the Storage Daemon may put its process Id file files. The process Id file is used to shutdown Bareos and to prevent multiple copies of Bareos from running simultaneously. Standard shell expansion of the directory is done when the configuration file is read so that values such as $HOME will be properly expanded.

