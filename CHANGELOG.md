All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and since Bareos version 20 this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Breaking changes
- Automatic downgrades to cleartext connections are now disabled by default. **Connecting to Bareos filedaemon < 18.2 is not possible anymore** - even if configured with TLS certificates. **Please update your clients**. Only if you absolutely cannot update a client you can configure `TLS Enable = no` explicitly. However, this is inherently insecure.
- The old catalog backup script `make_catalog_backup.pl` has been removed. Its replacement `make_catalog_backup` was already the default. If you didn't already update your BackupCatalog job, now is the time to do so.
- Support for Windows x86 (32Bit) has been removed.
- Support for Python 2 has been removed.

- The following **deprecated configuration options** have been **removed**, make sure to update your configuration accordingly:
   * Director daemon
     * `Pid Directory`
     * `Maximum Connections`
     
   * File daemon
     * `Pid Directory`
     * `Compatible`
     * `Maximum Connections`

   * Storage daemon
     * `Pid Directory`
     * `Compatible`
     * `Maximum Connections`

- The following configuration defaults have changed:
   * Director daemon
     * `Hard Links = no` (was `yes`) in FileSet->Include->Options
     * `TLS Require = yes` (was `no`)
   * Storage daemon
     * `Maximum Concurrent Jobs = 1` (was `0`) in Device
     * `Maximum Block Size = 1048576` (was `0`) in Device
     * `TLS Require = yes` (was `no`)
   * File daemon
     * `TLS Require = yes` (was `no`)

- The PostgreSQL plugin got a complete rewrite, the minimal changes are:
   * `module_name=bareos-fd-postgresql`
   * `db_host=`
   * `wal_archive_dir=`

### Added
- file checksums: add new signature algorithm xxh128 [PR #1359]
- stored: buffer fd messages [PR #1539]
- filed: parallel compression/reading/sending [PR #1533]
- cats: postgresql introduce pl/sql lstat_decode() function [PR #1466]
- bareos-fd-plugin: new python PostgreSQL [PR #1541]
- tls: add tls v1.3 configuration option [PR #1577]
- webui: add machine-readable copyright file [PR #1419]
- webui: add config resource graph to analytics (experimental) [PR #1412]
- build: add Debian 12 [PR #1477]
- build: add Fedora 38 [PR #1563]
- devtools: add `pr-tool` to automate PR review and merge [PR #935]

### Changed
- VMware Plugin: introduce pyVmomi 8.x compatibility [PR #1352]
- VMware Plugin: Backup and Restore of VMs using multiple datastores [PR #1473]
- VMware Plugin: improve snapshot cleanup [PR #1484]
- webui: enable sorting on version column [PR #1365]
- webui: create internal api module [PR #1447]
- webui: make restore merge options configurable [PR #1445]
- webui: update German translation [PR #1437]
- webui: update localization [PR #1414]
- webui: improve localization process [PR #1429]
- webui: re-add show update status for clients [PR #1371]
- webui: upgrade bootstrap to version 3.4.1 [PR #1550]
- tray-monitor: build against Qt6 when present [PR #1011]
- dird: skip disabled clients in status command [PR #1367]
- ua_restore: Add additional client info for restore report [PR #1374]
- ua_restore: enable restore from archive [PR #1372]
- dird: enable default options in `fileset` config when no options are explicitly specified [PR #1357]
- dird: cats: adapt `purge` command to delete jobs with specific jobstatus and/or from specific pool [PR #1349]
- dird: allow to disable TLS-PSK downgrades [PR #1398]
- dird: keep copy and migration control/administrative jobs [PR #1421]
- dird: deprecate client `Autoprune`, `JobRetention`, and `FileRetention` [PR #1425]
- dird: cats: abort purge when there are no eligible jobids [PR #1512]
- dird: show current and allowed console connections [PR #1487]
- dird: add prev and new jobid variables [PR #1499]
- dird: `list jobs`: add `level` keyword and accept a list of job levels [PR #1548]
- consolidate: make virtualfull jobs spawned by consolidate job inherit same priority and max concurrent jobs [PR #1530]
- database: improve subscription view [PR #1542]
- update `Maximum Concurrent Jobs` default value to 1 [PR #1426] 
- stored: change default block size to 1 MiB [PR #1396]
- stored: add AccessMode SD->Device directive to reserve devices exclusively for reading or writing [PR #1464]
- stored: automatically increment tape block size in case the buffer is too small [PR #1496]
- vss: remove dependency on live system during backup [PR #1452]
- bsmtp: make mailhost and port message info a debug message [PR #1507]
- improve default configuration [PR #1508]
- ua_restore: change restore argument handling [PR #1516]
- plugins: switch python-ldap plugin to  python3 [PR #1522]
- python-fd: allow fd plugins to (re-)set since-time and disable/re-enable timestamp/accurate checks [PR #1505]
- python-bareos: add missing `dirname` variable [PR #1460]
- python-bareos: add timeout setting when connecting to the director. [PR #1583]
- python: adapt for new Python module versions [PR #1546]
- Python Plugins: Avoid pop(0) performance impact [PR #1351]
- config: deprecate `LabelType` and `CheckLabels` [PR #1521]
- daemons: set CLI11 error exit code to `41` and bareos config parsing error exit code to `42` [PR #1515]
- require TLS by default [PR #1529]
- database: media table: use bigint instead of integer [PR #1579]
- configuration: deprecate base jobs [PR #1580]
- Enable automatic cpu-dependent dispatch for xxHash [PR #1601]
- packaging: systemd unit: set a limit of restart [PR #1450]
- Allow to use the third-party libraries of the OS instead of the bundled ones [PR #1441]
- scripts: config-lib improve get_local_hostname fallback [PR #1402]
- cleanup: remove some unused functions and shrink number of includes in bareos.h [PR #1433]
- rename gettext macro [PR #1588]
- build: replace sprintf by snprintf due to upgraded MacOS compiler, change linking of googletest [PR #1361]
- introduce php-cs-fixer and apply PSR-12 guidelines [PR #1403]
- berrno_test.cc: accept both 271E and 273E [PR #1407]
- Sanitizers: add ASAN options to avoid crashes [PR #1410]
- packaging: cleanup SUSE webui dependencies [PR #1493]
- build: adapt matrix and pkglist for changes to CI [PR #1490]
- build: switch from FreeBSD 13.1 to 13.2 [PR #1524]
- build: switch to FreeBSD 12.4 [PR #1440]
- FreeBSD: build cleanup [PR #1336]
- testfind: reuse filedaemon logic [PR #1234]
- core: make resource/configuration locking safer [PR #1325]
- systemtests: `rename virtualfull` -> `virtualfull-basic` ,`bareos` -> `bareos-basic`, `bconsole` -> `bconsole-basic` [PR #1339]
- improvements to pr-tool [PR #1389]
- pr-tool: handling POEditor commits and optional github ci tests [PR #1434]
- pr-tool: Add options to be used in CI runs [PR #1488]
- bareos-check-sources: ignore bootstrap*.css [PR #1556]
- utils: add a thread-safe single-producer/single-consumer queue [PR #1504]
- devtools: Update python dependencies [PR #1531]
- scripts: force cd / for all PostgreSQL scripts [PR #1608]
- dird: remove optimize_for_size/optimize_for_speed [PR #1613]
- Improve python plugin configuration [PR #1619]
- restore: add fileregex parameter [PR #1618]
- macOS: fix linking problem for macOS >= 14 [PR #1620]
- build: introduce Fedora 39 [PR #1615]
- filedaemon: remove ovirt plugin [PR #1626]
- vadp-dumper: fix multithreaded backup/restore issues [PR #1623]

### Removed
- cats: remove remains of deprecated databases [PR #1377]
- dird: filed: stored: remove deprecated `maximumconnections` option from configuration [PR #1340]
- filed: stored: remove deprecated `compatible` option from configuration  [PR #1341]
- daemons: remove deprecated `Pid Directory` config option [PR #1426] 
- core: remove deprecated make_catalog_backup.pl [PR #1378]
- build: remove python2 support [PR #1520]
- build: disable building for Windows 32-bit [PR #1582]
- build: remove openSUSE 15.3 and Univention 5.0 from test matrix [PR #1469]
- cats: remove dynamic catalog backends [PR #1392]
- remove no longer used pkglists [PR #1335]
- Remove unnecessary loader/module split in Python modules for the FD [PR #1602]
- webui: remove some development leftovers and artefacts [PR #1422]

### Fixed
- VMware Plugin: Fix backup and recreating VMs with PCI passthrough for GPU [PR #1565]
- VMware Plugin: Fix transformer issues [PR #1532]
- VMware Plugin: fix restore to different vmname [PR #1390]
- vmware_cbt_tool.py: pyVmomi 8.x compatibility [PR #1386]
- stored: fix support for non-tape block-addressed devices [PR #1554]
- stored: remove warning for maximum block size for tapes [PR #1375]
- stored: fix crashes of storage tools when autoxflate plugin is loaded [PR #1348]
- storage daemon: fix crash on volume swap [PR #1356]
- status storage: fix wrong data displayed about waiting jobs [PR #1476]
- stored: fix blocksize warning [PR #1503]
- stored: fix incoherent meta data when concurrently writing to the same volume [PR #1495]
- daemons: update network handling when IP protocols unavailable [PR #1454]
- autoxflate: fix autoxflate on replication [PR #1576]
- core: fix schema public with PostgreSQL 15 [PR #1449]
- cats: fix issue where `startfile` field gets wrongly updated [PR #1346]
- cats: fix for integer overflow issue when using `offset` in `llist` [PR #1547]
- catreq.cc: suppress missing batch con warning [PR #1578]
- postgresql plugin: read config parameters from cluster [PR #1599]
- cats: fix creates, grants and drops postgresql [PR #1502]
- Improve handling of catalog requests that try to reduce VolFiles, VolBlocks and VolBytes [PR #1431]
- dird: fix expected file count error during bsr build  [PR #1511]
- tools: fix tools not starting up on windows  [PR #1549]
- bsmtp: fix and update code, and change CLI parsing to CLI11 [PR #1316]
- restore: fix failed restores showing `Restore ok with warning` [PR #1387]
- json generation: Fix some leaks and an integer overflow [PR #1130]
- bsmtp bls bextract: fixes for command line parsing [PR #1455]
- checkpoints: fix performance drop on big volume restores [PR #1345]
- Fix problem with reoccuring files in always incremental [PR #1395]
- Consolidate: fix for consolidate job's client name not being correctly shown [PR #1474]
- filed: fix possible data-loss when excluding hardlinks [PR #1506]
- filed: avoid reading from ephemeral buffer [PR #1373]
- filed: fix off-by-one error when resizing acl buffer [PR #1479]
- core: fixes for gcc-13 support [PR #1424]
- build: fix for gcc 13.1.1 [PR #1459]
- Fix gcc warnings in ndmjob program [PR #1343]
- packaging: debian fix dependencies [PR #1573]
- github actions: update/fix publish to PyPI workflows [PR #1572]
- github actions: PyPi: install setuptools [PR #1604]
- scripts: config-lib improve setup_sd_user [PR #1448]
- dird: fix `purge oldest volume` [PR #1629]

### Documentation
- add explanation about binary version numbers [PR #1354]
- docs: improve bareos-webui documentation [PR #1366]
- docs: catalog maintenance improvements [PR #1379]
- doc: improve plugin, webui, virtualfull chapters [PR #1401]
- docs: move and update localization documentation [PR #1415]
- check-sources: update check-sources ignores [PR #1439]
- docs: replace GZIP compression by LZ4 and MD5/SHA1 signature by XXH128 [PR #1453]
- docs: Windows add Antivirus section [PR #1468]
- doc: renew tutorial, suggest passive client, restructure heartbeat interval [PR #1543]
- docs: improvements for droplet, jobdefs, concurrents jobs for restore [PR #1611]

[PR #935]: https://github.com/bareos/bareos/pull/935
[PR #1011]: https://github.com/bareos/bareos/pull/1011
[PR #1130]: https://github.com/bareos/bareos/pull/1130
[PR #1234]: https://github.com/bareos/bareos/pull/1234
[PR #1316]: https://github.com/bareos/bareos/pull/1316
[PR #1325]: https://github.com/bareos/bareos/pull/1325
[PR #1335]: https://github.com/bareos/bareos/pull/1335
[PR #1336]: https://github.com/bareos/bareos/pull/1336
[PR #1339]: https://github.com/bareos/bareos/pull/1339
[PR #1340]: https://github.com/bareos/bareos/pull/1340
[PR #1341]: https://github.com/bareos/bareos/pull/1341
[PR #1343]: https://github.com/bareos/bareos/pull/1343
[PR #1345]: https://github.com/bareos/bareos/pull/1345
[PR #1346]: https://github.com/bareos/bareos/pull/1346
[PR #1348]: https://github.com/bareos/bareos/pull/1348
[PR #1349]: https://github.com/bareos/bareos/pull/1349
[PR #1351]: https://github.com/bareos/bareos/pull/1351
[PR #1352]: https://github.com/bareos/bareos/pull/1352
[PR #1354]: https://github.com/bareos/bareos/pull/1354
[PR #1356]: https://github.com/bareos/bareos/pull/1356
[PR #1357]: https://github.com/bareos/bareos/pull/1357
[PR #1359]: https://github.com/bareos/bareos/pull/1359
[PR #1361]: https://github.com/bareos/bareos/pull/1361
[PR #1365]: https://github.com/bareos/bareos/pull/1365
[PR #1366]: https://github.com/bareos/bareos/pull/1366
[PR #1367]: https://github.com/bareos/bareos/pull/1367
[PR #1371]: https://github.com/bareos/bareos/pull/1371
[PR #1372]: https://github.com/bareos/bareos/pull/1372
[PR #1373]: https://github.com/bareos/bareos/pull/1373
[PR #1374]: https://github.com/bareos/bareos/pull/1374
[PR #1375]: https://github.com/bareos/bareos/pull/1375
[PR #1377]: https://github.com/bareos/bareos/pull/1377
[PR #1378]: https://github.com/bareos/bareos/pull/1378
[PR #1379]: https://github.com/bareos/bareos/pull/1379
[PR #1386]: https://github.com/bareos/bareos/pull/1386
[PR #1387]: https://github.com/bareos/bareos/pull/1387
[PR #1389]: https://github.com/bareos/bareos/pull/1389
[PR #1390]: https://github.com/bareos/bareos/pull/1390
[PR #1392]: https://github.com/bareos/bareos/pull/1392
[PR #1395]: https://github.com/bareos/bareos/pull/1395
[PR #1396]: https://github.com/bareos/bareos/pull/1396
[PR #1398]: https://github.com/bareos/bareos/pull/1398
[PR #1401]: https://github.com/bareos/bareos/pull/1401
[PR #1402]: https://github.com/bareos/bareos/pull/1402
[PR #1403]: https://github.com/bareos/bareos/pull/1403
[PR #1407]: https://github.com/bareos/bareos/pull/1407
[PR #1410]: https://github.com/bareos/bareos/pull/1410
[PR #1412]: https://github.com/bareos/bareos/pull/1412
[PR #1414]: https://github.com/bareos/bareos/pull/1414
[PR #1415]: https://github.com/bareos/bareos/pull/1415
[PR #1419]: https://github.com/bareos/bareos/pull/1419
[PR #1421]: https://github.com/bareos/bareos/pull/1421
[PR #1422]: https://github.com/bareos/bareos/pull/1422
[PR #1424]: https://github.com/bareos/bareos/pull/1424
[PR #1425]: https://github.com/bareos/bareos/pull/1425
[PR #1426]: https://github.com/bareos/bareos/pull/1426
[PR #1429]: https://github.com/bareos/bareos/pull/1429
[PR #1431]: https://github.com/bareos/bareos/pull/1431
[PR #1433]: https://github.com/bareos/bareos/pull/1433
[PR #1434]: https://github.com/bareos/bareos/pull/1434
[PR #1437]: https://github.com/bareos/bareos/pull/1437
[PR #1439]: https://github.com/bareos/bareos/pull/1439
[PR #1440]: https://github.com/bareos/bareos/pull/1440
[PR #1441]: https://github.com/bareos/bareos/pull/1441
[PR #1445]: https://github.com/bareos/bareos/pull/1445
[PR #1447]: https://github.com/bareos/bareos/pull/1447
[PR #1448]: https://github.com/bareos/bareos/pull/1448
[PR #1449]: https://github.com/bareos/bareos/pull/1449
[PR #1450]: https://github.com/bareos/bareos/pull/1450
[PR #1452]: https://github.com/bareos/bareos/pull/1452
[PR #1453]: https://github.com/bareos/bareos/pull/1453
[PR #1454]: https://github.com/bareos/bareos/pull/1454
[PR #1455]: https://github.com/bareos/bareos/pull/1455
[PR #1459]: https://github.com/bareos/bareos/pull/1459
[PR #1460]: https://github.com/bareos/bareos/pull/1460
[PR #1464]: https://github.com/bareos/bareos/pull/1464
[PR #1466]: https://github.com/bareos/bareos/pull/1466
[PR #1468]: https://github.com/bareos/bareos/pull/1468
[PR #1469]: https://github.com/bareos/bareos/pull/1469
[PR #1473]: https://github.com/bareos/bareos/pull/1473
[PR #1474]: https://github.com/bareos/bareos/pull/1474
[PR #1476]: https://github.com/bareos/bareos/pull/1476
[PR #1477]: https://github.com/bareos/bareos/pull/1477
[PR #1479]: https://github.com/bareos/bareos/pull/1479
[PR #1484]: https://github.com/bareos/bareos/pull/1484
[PR #1487]: https://github.com/bareos/bareos/pull/1487
[PR #1488]: https://github.com/bareos/bareos/pull/1488
[PR #1490]: https://github.com/bareos/bareos/pull/1490
[PR #1493]: https://github.com/bareos/bareos/pull/1493
[PR #1495]: https://github.com/bareos/bareos/pull/1495
[PR #1496]: https://github.com/bareos/bareos/pull/1496
[PR #1499]: https://github.com/bareos/bareos/pull/1499
[PR #1502]: https://github.com/bareos/bareos/pull/1502
[PR #1503]: https://github.com/bareos/bareos/pull/1503
[PR #1504]: https://github.com/bareos/bareos/pull/1504
[PR #1505]: https://github.com/bareos/bareos/pull/1505
[PR #1506]: https://github.com/bareos/bareos/pull/1506
[PR #1507]: https://github.com/bareos/bareos/pull/1507
[PR #1508]: https://github.com/bareos/bareos/pull/1508
[PR #1511]: https://github.com/bareos/bareos/pull/1511
[PR #1512]: https://github.com/bareos/bareos/pull/1512
[PR #1515]: https://github.com/bareos/bareos/pull/1515
[PR #1516]: https://github.com/bareos/bareos/pull/1516
[PR #1520]: https://github.com/bareos/bareos/pull/1520
[PR #1521]: https://github.com/bareos/bareos/pull/1521
[PR #1522]: https://github.com/bareos/bareos/pull/1522
[PR #1524]: https://github.com/bareos/bareos/pull/1524
[PR #1529]: https://github.com/bareos/bareos/pull/1529
[PR #1530]: https://github.com/bareos/bareos/pull/1530
[PR #1531]: https://github.com/bareos/bareos/pull/1531
[PR #1532]: https://github.com/bareos/bareos/pull/1532
[PR #1533]: https://github.com/bareos/bareos/pull/1533
[PR #1539]: https://github.com/bareos/bareos/pull/1539
[PR #1541]: https://github.com/bareos/bareos/pull/1541
[PR #1542]: https://github.com/bareos/bareos/pull/1542
[PR #1543]: https://github.com/bareos/bareos/pull/1543
[PR #1546]: https://github.com/bareos/bareos/pull/1546
[PR #1547]: https://github.com/bareos/bareos/pull/1547
[PR #1548]: https://github.com/bareos/bareos/pull/1548
[PR #1549]: https://github.com/bareos/bareos/pull/1549
[PR #1550]: https://github.com/bareos/bareos/pull/1550
[PR #1554]: https://github.com/bareos/bareos/pull/1554
[PR #1556]: https://github.com/bareos/bareos/pull/1556
[PR #1563]: https://github.com/bareos/bareos/pull/1563
[PR #1565]: https://github.com/bareos/bareos/pull/1565
[PR #1572]: https://github.com/bareos/bareos/pull/1572
[PR #1573]: https://github.com/bareos/bareos/pull/1573
[PR #1576]: https://github.com/bareos/bareos/pull/1576
[PR #1577]: https://github.com/bareos/bareos/pull/1577
[PR #1578]: https://github.com/bareos/bareos/pull/1578
[PR #1579]: https://github.com/bareos/bareos/pull/1579
[PR #1580]: https://github.com/bareos/bareos/pull/1580
[PR #1582]: https://github.com/bareos/bareos/pull/1582
[PR #1583]: https://github.com/bareos/bareos/pull/1583
[PR #1588]: https://github.com/bareos/bareos/pull/1588
[PR #1599]: https://github.com/bareos/bareos/pull/1599
[PR #1601]: https://github.com/bareos/bareos/pull/1601
[PR #1602]: https://github.com/bareos/bareos/pull/1602
[PR #1604]: https://github.com/bareos/bareos/pull/1604
[PR #1608]: https://github.com/bareos/bareos/pull/1608
[PR #1611]: https://github.com/bareos/bareos/pull/1611
[PR #1613]: https://github.com/bareos/bareos/pull/1613
[PR #1615]: https://github.com/bareos/bareos/pull/1615
[PR #1618]: https://github.com/bareos/bareos/pull/1618
[PR #1619]: https://github.com/bareos/bareos/pull/1619
[PR #1620]: https://github.com/bareos/bareos/pull/1620
[PR #1623]: https://github.com/bareos/bareos/pull/1623
[PR #1626]: https://github.com/bareos/bareos/pull/1626
[PR #1629]: https://github.com/bareos/bareos/pull/1629
[unreleased]: https://github.com/bareos/bareos/tree/master
