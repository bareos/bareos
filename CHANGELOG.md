All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- add openssl 3 ulc [PR #1683]
- Add backport tool [PR #1639]

### Changed
- github actions: PyPi: install setuptools [PR #1589]
- restore: add fileregex parameter [PR #1587]
- scripts: force cd / for all PostgreSQL scripts [PR #1607]
- Improve python plugin configuration [PR #1605]
- macOS: fix linking problem for macOS >= 14 [PR #1592]
- dird: remove optimize_for_size/optimize_for_speed [PR #1612]
- build: introduce Fedora 39 [PR #1614]
- libcloud: modularize systemtest [PR #1609]
- filedaemon: remove ovirt plugin [PR #1617]
- vadp-dumper: fix multithreaded backup/restore issues [PR #1593]
- VMware Plugin: Run bareos_vadp_dumper with optimized parameters to increase backup performance [PR #1630]
- pkglists: update SUSE to have vmware packages [PR #1632]
- backup report: show negative compression [PR #1598]
- core: add build patch for `sprintf` in macos builds [PR #1636]
- python-bareos: use socket.create_connection() to allow AF_INET6 [PR #1646]
- Improve FreeBSD build [PR #1538]
- core: sql_* add leading space to sql construct [PR #1656]
- plugins: postgresql fix missing pg_backup_stop() call [PR #1655]
- filed: fix vss during client initiated connections [PR #1665]
- bareos-config: fix output of deploy_config [PR #1672]
- Disable automated package-tests for SLES 12 [PR #1671]
- Make BareosDirPluginPrometheusExporter.py work with python3 [PR #1647]
- Improve FreeBSD dependencies [PR #1670]
- python-bareos: integrate usage of config files [PR #1678]
- cmake: cleanup [PR #1661]
- bnet-server-tcp: split socket creation from listening for unittests [PR #1649]
- webui: Backup Unit Report fixes [PR #1696]
- windows: fix calculation of "job_metadata.xml" object size [PR #1695]
- stored: fix storage daemon crash if passive client is unreachable, create better session keys [PR #1688]
- bareos-triggerjob: fix parameter handling [PR #1708]
- fvec: add mmap based vector  [PR #1662]
- core: fix various data races (connection_pool/heartbeat_thread) [PR #1685]
- filed: skip stripped top level directories [PR #1686]
- jcr: fix some compiler warnings [PR #1648]
- build: Fix debugsource RPM package generation [PR #1713]
- Bugfix: Clean up error handling in LDAP plugin, fix dependencies [PR #1717]
- crypto_wrap: replace aes wrap with openssl aes wrap algorithm [PR #1718]
- dbcheck: fix dbcheck crash if password is not set in catalog resource [PR #1710]
- Require python3 explicit [PR #1719]
- cmake: put generated files into CMAKE_BINARY_DIR [PR #1707]
- increase warning level on C/C++ compiler [PR #1689]
- VMware Plugin: Backup and Restore NVRAM [PR #1727]
- doc: add backtick around *.?* description  [PR #1752]
- PR template: remove backport hints [PR #1762]
- python-bareos: use TLS-PSK from core ssl module (available since Python >= 3.13) [PR #1756]
- [percona-xtrabackup] prevent High memory usage for no reason (IO_CLOSE) [PR #1724]
- docs: improve handling of ResourceItem descriptions [PR #1761]
- pr-tool: give hint about commit headline length limits [PR #1763]
- stored: fix some sd error messages; add additional check during restore; split up always-incremental-consolidate test [PR #1722]
- Generate LICENSE.txt from LICENSE.template [PR #1753]
- Allow cross-building for Windows on newer compiler [PR #1772]

### Removed
- plugins: remove old deprecated postgres plugin [PR #1606]
- Remove EOL platforms [PR #1684]

### Documentation
- docs: improvements for droplet, jobdefs [PR #1581]
- docs: fix Pool explanation for migration jobs [PR #1728]
- github: introduce template for issues [PR #1716]

### Fixed
- dird: fix `purge oldest volume` [PR #1628]
- Fix continuation on colons in plugin baseclass [PR #1637]
- plugins: fix cancel handling crash [PR #1595]
- Fix bareos_tasks plugin for pgsql [PR #1659]
- core: Fix compile errors on GCC 14 [PR #1687]
- stored: fix authentication race condition / deadlock [PR #1732]
- Fix warning about missing delcandidates table in director [PR #1721]
- stored: fix not counting files correctly in mac jobs when autoxflate is enabled [PR #1745]
- cats: fixes BigSqlQuery header fetching [PR #1746]

[PR #1538]: https://github.com/bareos/bareos/pull/1538
[PR #1581]: https://github.com/bareos/bareos/pull/1581
[PR #1587]: https://github.com/bareos/bareos/pull/1587
[PR #1589]: https://github.com/bareos/bareos/pull/1589
[PR #1592]: https://github.com/bareos/bareos/pull/1592
[PR #1593]: https://github.com/bareos/bareos/pull/1593
[PR #1595]: https://github.com/bareos/bareos/pull/1595
[PR #1598]: https://github.com/bareos/bareos/pull/1598
[PR #1605]: https://github.com/bareos/bareos/pull/1605
[PR #1606]: https://github.com/bareos/bareos/pull/1606
[PR #1607]: https://github.com/bareos/bareos/pull/1607
[PR #1609]: https://github.com/bareos/bareos/pull/1609
[PR #1612]: https://github.com/bareos/bareos/pull/1612
[PR #1614]: https://github.com/bareos/bareos/pull/1614
[PR #1617]: https://github.com/bareos/bareos/pull/1617
[PR #1628]: https://github.com/bareos/bareos/pull/1628
[PR #1630]: https://github.com/bareos/bareos/pull/1630
[PR #1632]: https://github.com/bareos/bareos/pull/1632
[PR #1636]: https://github.com/bareos/bareos/pull/1636
[PR #1637]: https://github.com/bareos/bareos/pull/1637
[PR #1639]: https://github.com/bareos/bareos/pull/1639
[PR #1646]: https://github.com/bareos/bareos/pull/1646
[PR #1647]: https://github.com/bareos/bareos/pull/1647
[PR #1648]: https://github.com/bareos/bareos/pull/1648
[PR #1649]: https://github.com/bareos/bareos/pull/1649
[PR #1655]: https://github.com/bareos/bareos/pull/1655
[PR #1656]: https://github.com/bareos/bareos/pull/1656
[PR #1659]: https://github.com/bareos/bareos/pull/1659
[PR #1661]: https://github.com/bareos/bareos/pull/1661
[PR #1662]: https://github.com/bareos/bareos/pull/1662
[PR #1665]: https://github.com/bareos/bareos/pull/1665
[PR #1670]: https://github.com/bareos/bareos/pull/1670
[PR #1671]: https://github.com/bareos/bareos/pull/1671
[PR #1672]: https://github.com/bareos/bareos/pull/1672
[PR #1678]: https://github.com/bareos/bareos/pull/1678
[PR #1683]: https://github.com/bareos/bareos/pull/1683
[PR #1684]: https://github.com/bareos/bareos/pull/1684
[PR #1685]: https://github.com/bareos/bareos/pull/1685
[PR #1686]: https://github.com/bareos/bareos/pull/1686
[PR #1687]: https://github.com/bareos/bareos/pull/1687
[PR #1688]: https://github.com/bareos/bareos/pull/1688
[PR #1689]: https://github.com/bareos/bareos/pull/1689
[PR #1695]: https://github.com/bareos/bareos/pull/1695
[PR #1696]: https://github.com/bareos/bareos/pull/1696
[PR #1707]: https://github.com/bareos/bareos/pull/1707
[PR #1708]: https://github.com/bareos/bareos/pull/1708
[PR #1710]: https://github.com/bareos/bareos/pull/1710
[PR #1713]: https://github.com/bareos/bareos/pull/1713
[PR #1716]: https://github.com/bareos/bareos/pull/1716
[PR #1717]: https://github.com/bareos/bareos/pull/1717
[PR #1718]: https://github.com/bareos/bareos/pull/1718
[PR #1719]: https://github.com/bareos/bareos/pull/1719
[PR #1721]: https://github.com/bareos/bareos/pull/1721
[PR #1722]: https://github.com/bareos/bareos/pull/1722
[PR #1724]: https://github.com/bareos/bareos/pull/1724
[PR #1727]: https://github.com/bareos/bareos/pull/1727
[PR #1728]: https://github.com/bareos/bareos/pull/1728
[PR #1732]: https://github.com/bareos/bareos/pull/1732
[PR #1745]: https://github.com/bareos/bareos/pull/1745
[PR #1746]: https://github.com/bareos/bareos/pull/1746
[PR #1752]: https://github.com/bareos/bareos/pull/1752
[PR #1753]: https://github.com/bareos/bareos/pull/1753
[PR #1756]: https://github.com/bareos/bareos/pull/1756
[PR #1761]: https://github.com/bareos/bareos/pull/1761
[PR #1762]: https://github.com/bareos/bareos/pull/1762
[PR #1763]: https://github.com/bareos/bareos/pull/1763
[PR #1772]: https://github.com/bareos/bareos/pull/1772
[unreleased]: https://github.com/bareos/bareos/tree/master
