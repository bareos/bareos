REM
REM Setup for migration tests
REM
COPY scripts\bareos-dir-migration.conf bin\bareos-dir.conf
COPY scripts\bareos-sd-migration.conf bin\bareos-sd.conf
COPY scripts\test-bareos-fd.conf bin\bareos-fd.conf
COPY scripts\test-console.conf bin\bconsole.conf
