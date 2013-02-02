REM
REM Setup for using the Virtual Disk Changer (simulates tape changer)
REM
COPY scripts\bareos-dir-tape.conf bin\bareos-dir.conf
COPY scripts\bareos-sd-2disk.conf bin\bareos-sd.conf
COPY scripts\test-bareos-fd.conf bin\bareos-fd.conf
COPY scripts\test-console.conf bin\bconsole.conf
COPY bin\bareos-dir.conf tmp\1
sed -e "s;# Autochanger = yes;  Autochanger = yes;g" tmp\1 >bin\bareos-dir.conf
