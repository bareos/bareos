Base configuration for all new (2016) regression tests.
The function copy_configs first copies the resources of this directory to the target directory,
followed by test specific additional or overwriting configuration.

Settings Summary
================

  * Director
    * "bareos-dir"

  * Storage
    * "File1"
      * Address = @hostname@
      * Device = "File1"
      * MediaType = "File1"
      * Tls Cn = "bareos-sd1.bareos.org"

  * Client
    * "bareos-fd"
    * Tls Cn = "client1.bareos.org"

  * JobDefs
    * "DefaultJob"

  * Job
    * "backup-bareos-fd"
      * Client = "bareos-fd"
      * JobDefs = "DefaultJob"
    * "RestoreFiles"
      * Where = "/tmp/bareos-restores"

  * FileSet
    * "FS_TESTJOB"
      * File=<@tmpdir@/file-list

  * Pool
    * "Scratch"
    * "Default"
      * MaximumVolumes = 20
      * MaximumVolumeBytes = 100 m

  * Messages
    * "Standard"

