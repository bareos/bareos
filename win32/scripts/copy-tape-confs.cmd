copy scripts\bareos-dir-tape.conf bin\bareos-dir.conf
copy scripts\bareos-sd-tape.conf bin\bareos-sd.conf
copy scripts\test-bareos-fd.conf bin\bareos-fd.conf
copy scripts\test-console.conf bin\bconsole.conf

REM get proper SD tape definitions
copy scripts\win32_tape_options bin\tape_options
