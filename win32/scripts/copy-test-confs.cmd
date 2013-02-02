COPY scripts\new-test-bareos-dir.conf bin\bareos-dir.conf
COPY scripts\test-bareos-sd.conf bin\bareos-sd.conf
COPY scripts\test-bareos-fd.conf bin\bareos-fd.conf
COPY scripts\test-console.conf bin\bconsole.conf

REM get proper SD tape definitions
COPY scripts\win32_tape_options bin\tape_options
