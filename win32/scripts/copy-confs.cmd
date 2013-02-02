COPY scripts\bareos-dir.conf bin\bareos-dir.conf
COPY scripts\bareos-sd.conf bin\bareos-sd.conf
COPY scripts\bareos-fd.conf bin\bareos-fd.conf
COPY scripts\bconsole.conf bin\bconsole.conf

REM get proper SD tape definitions
COPY scripts\win32_tape_options bin\tape_options
