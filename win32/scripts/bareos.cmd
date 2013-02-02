@ECHO off
IF "%1" == "start" (
   net start bareos-dir
   net start bareos-sd
   net start bareos-fd
) ELSE IF "%1" == "stop" (
   net stop bareos-dir
   net stop bareos-sd
   net stop bareos-fd
) ELSE IF "%1" == "install" (
   bareos-dir /install -c %2\bareos-dir.conf
   bareos-sd /install -c %2\bareos-sd.conf
   bareos-fd /install -c %2\bareos-fd.conf
) ELSE IF "%1" == "uninstall" (
   bareos-dir /remove
   bareos-sd /remove
   bareos-fd /remove
)
