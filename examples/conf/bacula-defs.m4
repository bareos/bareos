
define(`CLIENT',`
Job {
  Name = "$1"
  JobDefs = "$3"
  Client = "$1"
  FileSet = "$1"
  Write Bootstrap = "/var/bacula/working/$1.bsr"
}

Client {
  Name = "$1"
  Address = "$2"
  FDPort = 9102
  Catalog = MyCatalog
  Password = "ilF0PZoICjQ60R3E3dks08Rq36KK8cDGJUAaW"          # password for FileDaemon
  File Retention = 30 days            # 30 days
  Job Retention = 6 months            # six months
  AutoPrune = yes                     # Prune expired Jobs/Files
}
')


define(`STORAGE', `
Storage {
  Name = "$1"
  Address = "$3"
  SDPort = 9103
  Password = "KLUwcp1ZTeIc0x265UPrpWW28t7d7cRXmhOqyHxRr"       
  Device = "$1"                # must be same as Device in Storage daemon
  Media Type = "$2"            # must be same as MediaType in Storage daemon
}')


define(`POOL', `
Pool {
  Name = "$1"
  Pool Type = Backup
  Recycle = yes                       # Bacula can automatically recycle Volumes
  AutoPrune = yes                     # Prune expired volumes
  Volume Retention = 365 days         # one year
  Accept Any Volume = yes             # write on any volume in the pool
  Cleaning Prefix = "CLN"
}')

