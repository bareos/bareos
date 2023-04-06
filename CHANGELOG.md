All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and since Bareos version 20 this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- VMware Plugin: introduce pyVmomi 8.x compatibility [PR #1352]
- devtools: add `pr-tool` to automate PR review and merge [PR #935]
- build: replace sprintf by snprintf due to upgraded MacOS compiler, change linking of googletest [PR #1361]
- storage daemon: fix crash on volume swap [PR #1356]
- core: make resource/configuration locking safer [PR #1325]
- json generation: Fix some leaks and an integer overflow [PR #1130]
- tray-monitor: build against Qt6 when present [PR #1011]
- systemtests: `rename virtualfull` -> `virtualfull-basic` ,`bareos` -> `bareos-basic`, `bconsole` -> `bconsole-basic` [PR #1339]
- stored: fix crashes of storage tools when autoxflate plugin is loaded [PR #1348]
- webui: enable sorting on version column [PR #1365]
- dird: skip disabled clients in status command [PR #1367]
- bsmtp: fix and update code, and change CLI parsing to CLI11 [PR #1316]
- ua_restore: Add additional client info for restore report [PR #1374]
- restore: fix failed restores showing Restore ok with warning [PR #1387]
- FreeBSD: build cleanup [PR #1336]
- improvements to pr-tool [PR #1389]
- file checksums: add new signature algorithm xxh128 [PR #1359]
- stored: remove warning for maximum block size for tapes [PR #1375]
- ua_restore: enable restore from archive [PR #1372]
- testfind: reuse filedaemon logic [PR #1234]
- dird: enable default options in `fileset` config when no options are explicitly specified [PR #1357]
- introduce php-cs-fixer and apply PSR-12 guidelines [PR #1403]
- berrno_test.cc: accept both 271E and 273E [PR #1407]
- Sanitizers: add ASAN options to avoid crashes [PR #1410]
- dird: cats: adapt `purge` command to delete jobs with specific jobstatus and/or from specific pool [PR #1349]
- filed: stored: remove deprecated `compatible` option from configuration  [PR #1341]
- webui: remove some development leftovers and artefacts [PR #1422]
- dird: allow to disable TLS-PSK downgrades [PR #1398]
- core: fixes for gcc-13 support [PR #1424]
- webui: update localization [PR #1414]
- webui: improve localization process [PR #1429]
- webui: add machine-readable copyright file [PR #1419]
- webui: add config resource graph to analytics (experimental) [PR #1412]
- dird: keep copy and migration control/administrative jobs [PR #1421]

### Removed
- remove no longer used pkglists [PR #1335]
- core: remove deprecated make_catalog_backup.pl [PR #1378]
- cats: remove remains of deprecated databases [PR #1377]
- dird: filed: stored: remove deprecated `maxconnections` option from configuration [PR #1340]
- cats: remove dynamic catalog backends [PR #1392]

### Changed
- cats: fix issue where `startfile` field gets wrongly updated [PR #1346]
- Python Plugins: Avoid pop(0) performance impact [PR #1351]

### Fixed
- Fix gcc warnings in ndmjob program [PR #1343]
- filed: avoid reading from ephemeral buffer [PR #1373]
- checkpoints: fix performance drop on big volume restores [PR #1345]
- VMware Plugin: fix restore to different vmname [PR #1390]
- vmware_cbt_tool.py: pyVmomi 8.x compatibility [PR #1386]
- Fix problem with reoccuring files in always incremental [PR #1395]

### Documentation
- add explanation about binary version numbers [PR #1354]
- docs: improve bareos-webui documentation [PR #1366]
- docs: catalog maintenance improvements [PR #1379]
- doc: improve plugin, webui, virtualfull chapters [PR #1401]
- docs: move and update localization documentation [PR #1415]

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
[PR #1398]: https://github.com/bareos/bareos/pull/1398
[PR #1401]: https://github.com/bareos/bareos/pull/1401
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
[PR #1429]: https://github.com/bareos/bareos/pull/1429
[unreleased]: https://github.com/bareos/bareos/tree/master
