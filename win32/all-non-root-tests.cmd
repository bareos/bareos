REM
REM Run all tests
REM
ECHO.
ECHO. >>test.out
ECHO Start non-root disk tests
ECHO Start non-root disk tests >>test.out
CALL tests\auto-label-test
CALL tests\backup-bareos-test
CALL tests\bextract-test
REM CALL tests\big-vol-test
CALL tests\bscan-test
CALL tests\bsr-opt-test
CALL tests\compressed-test
REM CALL tests\compress-encrypt-test
CALL tests\concurrent-jobs-test
CALL tests\data-encrypt-test
REM CALL tests\encrypt-bug-test
REM CALL tests\fifo-test
REM CALL tests\backup-to-null
REM CALL tests\regexwhere-test
REM The following two can uses *lots* of disk space
REM   so they are normally turned off, but if you have
REM   at least two GB free, you can run them
REM CALL tests\sparse-encrypt-test
REM CALL tests\gigaslam-sparse-test
CALL tests\differential-test
CALL tests\four-concurrent-jobs-test
CALL tests\four-jobs-test
CALL tests\incremental-test
CALL tests\query-test
CALL tests\recycle-test
CALL tests\restore2-by-file-test
CALL tests\restore-by-file-test
CALL tests\restore-disk-seek-test
REM CALL tests\runscript-test
CALL tests\six-vol-test
CALL tests\span-vol-test
REM CALL tests\maxvol-test
REM CALL tests\maxvol2-test
CALL tests\sparse-compressed-test
CALL tests\sparse-test
CALL tests\two-jobs-test
CALL tests\two-vol-test
CALL tests\verify-vol-test
REM CALL tests\weird-files2-test
REM CALL tests\weird-files-test
CALL tests\migration-job-test
CALL tests\migration-jobspan-test
CALL tests\migration-volume-test
CALL tests\migration-time-test
REM CALL tests\hardlink-test
REM CALL tests\tls-test
ECHO End non-root disk tests
ECHO End non-root disk tests >>test.out

REM
REM The following are Virtual Disk Autochanger tests
ECHO.
ECHO. >>test.out
ECHO Start non-root virtual disk autochanger tests
ECHO Start non-root virtual disk autochanger tests >>test.out
REM CALL tests\three-pool-recycle-test
CALL tests\two-pool-test
REM CALL tests\fast-two-pool-test
CALL tests\two-volume-test
CALL tests\incremental-2disk
CALL tests\2drive-incremental-2disk
CALL tests\scratch-pool-test
CALL tests\2drive-concurrent-test.cmd
REM CALL tests\incremental-2media
REM CALL tests\three-pool-test
ECHO End non-root virtual disk autochanger tests
ECHO End non-root virtual disk autochanger tests >>test.out
