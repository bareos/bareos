COPY scripts\win32-bareos-dir-tape.conf bin\bareos-dir.conf
COPY scripts\win32-bareos-sd-tape.conf bin\bareos-sd.conf
COPY scripts\win32-bareos-fd.conf bin\bareos-fd.conf
COPY scripts\test-console.conf bin\bconsole.conf

REM get proper SD tape definitions
COPY scripts\win32_tape_options bin\tape_options
