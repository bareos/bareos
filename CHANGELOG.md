All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- add openssl 3 ulc [PR #1683]
- Add backport tool [PR #1639]
- Add FreeBSD 13.3 and 14.0 [PR #1765]

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
- cats: fix subscription view missing mariabackup plugin [PR #1767]
- contrib: add reschedule_job_as_full.sh [PR #1786]
- replace https://bugs.bareos.org by  https://github.com/bareos/bareos/issues/ [PR #1808]
- tools: remove perl in mtx changer [PR #1740]
- bsmtp: add fixes to be more rfc compliant [PR #1795]
- pr-tool: update LICENSE.txt [PR #1773]
- tools: add dedup estimation tool `bdedupestimate` [PR #1654]
- Build ULC and EL_9 for aarch64 [PR #1821]
- webui: fix and improve update check [PR #1809]
- python-bareos: fix description [PR #1840]
- contrib: fix reschedule_job_as_full.sh [PR #1853]
- pull-request-template: add milestone check to reviewers list [PR #1868]
- contrib: add pure python statefile parser [PR #1789]
- Use only MinGW VSS [PR #1847]
- VMware Plugin: Adapt to Python 3.12 [PR #1850]
- freebsd: fix build issues with ports tree 2024Q3 [PR #1883]
- Improve PythonFdWrapper class [PR #1846]
- windows: build natively with msvc compiler [PR #1744]
- alist: fix race condition while iterating [PR #1859]
- build: enable building for Fedora40 and Ubuntu 24.04 [PR #1815]
- ULC: build the universal linux client also for rpm based OS. [PR #1824]
- consolidate: disable vfull duplicate job check [PR #1739]
- dird: disallow running always incremental virtual full jobs with empty jobid list [PR #1738]
- msgchan: fix deadlock [PR #1858]
- VMware Plugin: Add option restore_allow_disks_mismatch [PR #1876]
- matrix remove obsolete SUSE [PR #1888]
- cats: scripts add option --no-psqlrc to psql [PR #1900]
- filed: fix python plugin crash on python <3.10 [PR #1889]
- vadp-dumper: fix out of bounds read [PR #1908]
- webui: fixing selenium tests [PR #1885]
- plugins: adjust plugin info formatting [PR #1919]
- python-bareos: fix backslash usage in regex [PR #1917]
- Libcloud Accurate File Backup [PR #1903]
- cats: add missing database locks [PR #1787]
- webui: properly set focus to input box after each command [PR #1936]
- bscrypto: fix and update code, and move CLI parsing to cli11, add systemtests [PR #1734]
- stored: reserve/acquire a device on first incoming write data [PR #1715]
- docs: add ADR infrastructure and first ADRs [PR #1945]
- nsi installers: remove postgresql backend [PR #1914]
- stored: add dedupable backend [PR #1663]
- systemtests: fix plugin postgresql testrunner-* [PR #1947]
- webui: fix handling deleted clients in restore browser [PR #1933]
- console.cc: forbid @exec etc. as privileged user [PR #1950]
- github actions python-bareos: add workflow_dispatch [PR #1966]
- FreeBSD: fix sed inplace usage, use bin/sh as shebang for script, pkg make director dependent of database-postgresql [PR #1961]
- dir: fix DbLocker usage [PR #1953]
- Disable writing PRE_LABEL label-type to support WORM media [PR #1958]
- debian: fix dedupable backend names [PR #1977]
- external packages: use CPM packages instead of third-party directory [PR #1963]
- scripts: remove unmaintained bareos-explorer [PR #1990]
- build: add support for Fedora 41 [PR #1996]
- filed: detect integer overflow during backup [PR #1952]
- dedupable device: fix example files [PR #2029]
- bareosfd_test.py: allow a delta of 1.1 [PR #2031]

### Removed
- plugins: remove old deprecated postgres plugin [PR #1606]
- Remove EOL platforms [PR #1684]
- core: fix some overlooked issues in msvc merge [PR #1912]
- dird: remove BaseJob directives [PR #1969]

### Documentation
- docs: improvements for droplet, jobdefs [PR #1581]
- docs: fix Pool explanation for migration jobs [PR #1728]
- github: introduce template for issues [PR #1716]
- docs: improve pluginAPI docs [PR #1805]
- Fix mistake in VolumeManagement.rst [PR #1829]
- docs: improve Restoring a Bareos Server section [PR #1878]
- Update ReaR documentation [PR #1871]
- docs: improve debuginfo install description (fix issue #1943) [PR #1980]
- docs: add debian howto start daemons instructions [PR #1998]
- docs: move bareos-devel to github discussion [PR #1989]
- docs: fixed typo in the filedaemon service name [PR #2027]

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
- Fix issue #1780 libpng icc profil [PR #1788]
- Fix missing DLL in windows packaging [PR #1807]
- VMware Plugin: Adapt to older urllib3 versions [PR #1810]
- fix various memory leaks [PR #1723]
- fix sql error on bad virtualfull; detect parsing errors with strtod [PR #1725]
- windows: fix some crashes, change handling of invalid paths; lex: add better error detection; accurate: fix out of bounds writes [PR #1793]
- create_bareos_database: fix `db_name` not being double quoted [PR #1865]
- fix warnings on FreeBSD 13.3 compiler [PR #1881]
- dir: fix crash on purge with job without client [PR #1857]
- fix runtime status [PR #1872]
- Fix multiple ACL handling bugs [PR #1875]
- fix #1775 plugin: fd mariabackup add support mariadb 11+ [PR #1835]
- deb control files: depend on python3-bareos [PR #1956]
- fix include-ordering on FreeBSD that could cause build issues [PR #1972]
- stored: fix volume size mismatch [PR #1992]
- add Honor No Dump Flag to config output [PR #1994]
- JSON API: make sure, strings are valid UTF8 [PR #1922]
- dird: fix mark command not accepting full windows paths [PR #1938]

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
[PR #1654]: https://github.com/bareos/bareos/pull/1654
[PR #1655]: https://github.com/bareos/bareos/pull/1655
[PR #1656]: https://github.com/bareos/bareos/pull/1656
[PR #1659]: https://github.com/bareos/bareos/pull/1659
[PR #1661]: https://github.com/bareos/bareos/pull/1661
[PR #1662]: https://github.com/bareos/bareos/pull/1662
[PR #1663]: https://github.com/bareos/bareos/pull/1663
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
[PR #1715]: https://github.com/bareos/bareos/pull/1715
[PR #1716]: https://github.com/bareos/bareos/pull/1716
[PR #1717]: https://github.com/bareos/bareos/pull/1717
[PR #1718]: https://github.com/bareos/bareos/pull/1718
[PR #1719]: https://github.com/bareos/bareos/pull/1719
[PR #1721]: https://github.com/bareos/bareos/pull/1721
[PR #1722]: https://github.com/bareos/bareos/pull/1722
[PR #1723]: https://github.com/bareos/bareos/pull/1723
[PR #1724]: https://github.com/bareos/bareos/pull/1724
[PR #1725]: https://github.com/bareos/bareos/pull/1725
[PR #1727]: https://github.com/bareos/bareos/pull/1727
[PR #1728]: https://github.com/bareos/bareos/pull/1728
[PR #1732]: https://github.com/bareos/bareos/pull/1732
[PR #1734]: https://github.com/bareos/bareos/pull/1734
[PR #1738]: https://github.com/bareos/bareos/pull/1738
[PR #1739]: https://github.com/bareos/bareos/pull/1739
[PR #1740]: https://github.com/bareos/bareos/pull/1740
[PR #1744]: https://github.com/bareos/bareos/pull/1744
[PR #1745]: https://github.com/bareos/bareos/pull/1745
[PR #1746]: https://github.com/bareos/bareos/pull/1746
[PR #1752]: https://github.com/bareos/bareos/pull/1752
[PR #1753]: https://github.com/bareos/bareos/pull/1753
[PR #1756]: https://github.com/bareos/bareos/pull/1756
[PR #1761]: https://github.com/bareos/bareos/pull/1761
[PR #1762]: https://github.com/bareos/bareos/pull/1762
[PR #1763]: https://github.com/bareos/bareos/pull/1763
[PR #1765]: https://github.com/bareos/bareos/pull/1765
[PR #1767]: https://github.com/bareos/bareos/pull/1767
[PR #1772]: https://github.com/bareos/bareos/pull/1772
[PR #1773]: https://github.com/bareos/bareos/pull/1773
[PR #1786]: https://github.com/bareos/bareos/pull/1786
[PR #1787]: https://github.com/bareos/bareos/pull/1787
[PR #1788]: https://github.com/bareos/bareos/pull/1788
[PR #1789]: https://github.com/bareos/bareos/pull/1789
[PR #1793]: https://github.com/bareos/bareos/pull/1793
[PR #1795]: https://github.com/bareos/bareos/pull/1795
[PR #1805]: https://github.com/bareos/bareos/pull/1805
[PR #1807]: https://github.com/bareos/bareos/pull/1807
[PR #1808]: https://github.com/bareos/bareos/pull/1808
[PR #1809]: https://github.com/bareos/bareos/pull/1809
[PR #1810]: https://github.com/bareos/bareos/pull/1810
[PR #1815]: https://github.com/bareos/bareos/pull/1815
[PR #1821]: https://github.com/bareos/bareos/pull/1821
[PR #1824]: https://github.com/bareos/bareos/pull/1824
[PR #1829]: https://github.com/bareos/bareos/pull/1829
[PR #1835]: https://github.com/bareos/bareos/pull/1835
[PR #1840]: https://github.com/bareos/bareos/pull/1840
[PR #1846]: https://github.com/bareos/bareos/pull/1846
[PR #1847]: https://github.com/bareos/bareos/pull/1847
[PR #1850]: https://github.com/bareos/bareos/pull/1850
[PR #1853]: https://github.com/bareos/bareos/pull/1853
[PR #1857]: https://github.com/bareos/bareos/pull/1857
[PR #1858]: https://github.com/bareos/bareos/pull/1858
[PR #1859]: https://github.com/bareos/bareos/pull/1859
[PR #1865]: https://github.com/bareos/bareos/pull/1865
[PR #1868]: https://github.com/bareos/bareos/pull/1868
[PR #1871]: https://github.com/bareos/bareos/pull/1871
[PR #1872]: https://github.com/bareos/bareos/pull/1872
[PR #1875]: https://github.com/bareos/bareos/pull/1875
[PR #1876]: https://github.com/bareos/bareos/pull/1876
[PR #1878]: https://github.com/bareos/bareos/pull/1878
[PR #1881]: https://github.com/bareos/bareos/pull/1881
[PR #1883]: https://github.com/bareos/bareos/pull/1883
[PR #1885]: https://github.com/bareos/bareos/pull/1885
[PR #1888]: https://github.com/bareos/bareos/pull/1888
[PR #1889]: https://github.com/bareos/bareos/pull/1889
[PR #1900]: https://github.com/bareos/bareos/pull/1900
[PR #1903]: https://github.com/bareos/bareos/pull/1903
[PR #1908]: https://github.com/bareos/bareos/pull/1908
[PR #1912]: https://github.com/bareos/bareos/pull/1912
[PR #1914]: https://github.com/bareos/bareos/pull/1914
[PR #1917]: https://github.com/bareos/bareos/pull/1917
[PR #1919]: https://github.com/bareos/bareos/pull/1919
[PR #1922]: https://github.com/bareos/bareos/pull/1922
[PR #1933]: https://github.com/bareos/bareos/pull/1933
[PR #1936]: https://github.com/bareos/bareos/pull/1936
[PR #1938]: https://github.com/bareos/bareos/pull/1938
[PR #1945]: https://github.com/bareos/bareos/pull/1945
[PR #1947]: https://github.com/bareos/bareos/pull/1947
[PR #1950]: https://github.com/bareos/bareos/pull/1950
[PR #1952]: https://github.com/bareos/bareos/pull/1952
[PR #1953]: https://github.com/bareos/bareos/pull/1953
[PR #1956]: https://github.com/bareos/bareos/pull/1956
[PR #1958]: https://github.com/bareos/bareos/pull/1958
[PR #1961]: https://github.com/bareos/bareos/pull/1961
[PR #1963]: https://github.com/bareos/bareos/pull/1963
[PR #1966]: https://github.com/bareos/bareos/pull/1966
[PR #1969]: https://github.com/bareos/bareos/pull/1969
[PR #1972]: https://github.com/bareos/bareos/pull/1972
[PR #1977]: https://github.com/bareos/bareos/pull/1977
[PR #1980]: https://github.com/bareos/bareos/pull/1980
[PR #1989]: https://github.com/bareos/bareos/pull/1989
[PR #1990]: https://github.com/bareos/bareos/pull/1990
[PR #1992]: https://github.com/bareos/bareos/pull/1992
[PR #1994]: https://github.com/bareos/bareos/pull/1994
[PR #1996]: https://github.com/bareos/bareos/pull/1996
[PR #1998]: https://github.com/bareos/bareos/pull/1998
[PR #2027]: https://github.com/bareos/bareos/pull/2027
[PR #2029]: https://github.com/bareos/bareos/pull/2029
[PR #2031]: https://github.com/bareos/bareos/pull/2031
[unreleased]: https://github.com/bareos/bareos/tree/master
