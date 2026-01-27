All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- table-formatter.js: Fix icon selection logic [PR #2481]
- systemtests: tune for execution on Windows [PR #2502]
- barri: remove indicators dependency [PR #2516]
- barri: remove unnecessary quotes in version text [PR #2512]

### Fixed
- VMware Plugin: Fix NVRAM backup when datacenter is not in root folder [PR #2475]
- rpm: don't fail, if install log file isn't writable [PR #2477]
- systemtests: autochanger add testrunner-restore-non-present-volume [PR #2504]
- core/filed: fix spurious wakeups race condition in filed. [PR #2513]

### Documentation
- docs: large improvement on several chapters [PR #2507]

## [25.0.1] - 2025-12-04

### Changed
- dirconfig: add subscription setting with comment [PR #2472]
- webui: make default job list period configurable [PR #2471]

### Fixed
- hyper-v: fix restores with portable data [PR #2474]

## [25.0.0] - 2025-12-01

### Breaking changes
- Bareos 25 disables SSL on the PostgreSQL connection, as we have observed strange issues with SSL enabled.[Issue #1965]
- Bareos 25 and later will not modify or extend existing configuration directories on package updates [PR #2338]
  Note: on FreeBSD unmodified configuration changes from Bareos <= 24 will get removed and   replaced by the new default configuration of the current package.
- Glusterfs FD plugin and SD backend is deprecated and will get removed in 26.

#### Automatic Disk Changer configuration with Storage -> Device `Count`
Setting up a virtual disk changer to backup as many parallel jobs into the same disk directory is now as easy as never before:

If the `Count` directive is specified with a value > 1,
duplicated devices will be created starting from serial-number 0000 up to `Count`.
The 0000 device is automatically assigned `Autoselect=No`. 
Additionally, an autochanger resource is created with the name of the device the `Count` directive is specified for.
The duplicated devices will be assigned to this autochanger unless they are used in another autochanger already.

##### Working Example
For a working example refer to the default configuration being deployed with Bareos 25 in the [Director Configuration](https://github.com/bareos/bareos/blob/bareos-25/core/src/defaultconfigs/bareos-sd.d/device/FileStorage.conf.in#L11) and in the [Storage configuration](https://github.com/bareos/bareos/blob/bareos-25/core/src/defaultconfigs/bareos-dir.d/storage/File.conf.in#L7)

##### Migration of a manually configured disk changer to the new automatic functionality

If you want to migrate from your manually configured disk autochanger to simply using the `Count` directive:
1. remove the disk autochanger resource
2. have a storage device with a specified `Count` directive (with value >1)
3. wherever you used the disk autochanger name before, use the name of the device from step 2.


### Added
- plugins: introduce windows disaster recovery  imager (barri) [PR #2285]
- plugins: add proxmox backup [PR #2372]
- plugins: introduce hyper-v backup [PR #2284]
- webui: add summary subscription report [PR #2340]
- set environment vars in bpipe fd plugin [PR #2205]
- add media_vault tool to contrib [PR #2357]
- build: add support for el10 [PR #2106]
- build: add Fedora 42 [PR #2263]
- build: add Debian 13 [PR #2290]
- Introduce build of Fedora 43 [PR #2407]

### Changed
- Sync EvpDigest between OpenSSL <1.1 and 1.1+ [PR #2086]
- winbareos-native.nsi: do not package python3 plugins [PR #2076]
- logrotate: add mtx-changer debug log config [PR #2039]
- cmake: add cmake check whether tirpc is installed [PR #2109]
- bconsole: require only one password in the configuration [PR #2116]
- openssl: unify ssl error logging [PR #2078]
- Inherit RunScript elements between JobDef resources [PR #2097]
- stored: list all devices if device is invalid/missing [PR #2122]
- Refactor some Xattr and Acl internals [PR #1893]
- systemtests: fail if daemons have config warnings [PR #2144]
- contrib: check_chunk.py improve README.md instructions [PR #2147]
- VMware Plugin: Fix CBT query handling [PR #2152]
- webui: use TemporaryDir as userdatadir in Selenium test [PR #2194]
- ndmp: remove ndmp backup level limit [PR #2188]
- reduce test-matrix [PR #2192]
- bareos-fd: add option for grpc fallback [PR #2104]
- Make tests optional [PR #2180]
- windows: single-config/single-output directory [PR #2211]
- webui: use WEB_UI path in apache bareos-webui.conf file [PR #2201]
- config: update directives and error out on bconsole config warnings [PR #2217]
- config: add virtual file changer example + documentation [PR #2090]
- various cmake improvements [PR #2176]
- packaging: use GCC 14 for RHEL 8/9 [PR #2275]
- setgid on configdirs [PR #2270]
- Replace Bsnprintf() implementation and add format attributes [PR #1697]
- cmake: disable lto when linking gtests [PR #2286]
- VMware Plugin: Fix for virtual USB devices [PR #2213]
- bareos-check-sources: add shell_format plugin [PR #2267]
- Build FreeBSD for major versions 14 / 13 (instead of minor releases) [PR #2117]
- matrix: test the Debian ULC packages for OpenSSL 1.1 on Debian 11 instead of Debian 10 [PR #2321]
- ndmp-bareos: Introduce incremental loop, restore 2nd file explicitly [PR #2269]
- systemtest: always incremental add one job with one removed file [PR #2329]
- systemtest: speed up always-incremental with sleep 0.25 [PR #2358]
- storage: improve tapealert plugin [PR #2370]
- systemtests: speed up testing [PR #2376]
- regex library: from legacy implementation to pcre2 [PR #2376]
- webui: migrate from ZF2 to Laminas [PR #2385]
- don't alter configuration on ULC package updates [PR #2399]
- stored: implicitly create autochanger from device with count > 1 [PR #2198]
- defaultconfig: introduce automatic diskchanger [PR #2427]
- webui: use patched laminas packages [PR #2439]
- jobreport: show FD Bytes Read [PR #2443]
- macos: use macos-15 runner [PR #2442]
- Improve status subscription [PR #2444]
- Refactor libcloud plugin to work with current Python versions [PR #2428]
- stored: add new volume status 'Unlabeled' [PR #2207]
- postgresql: require non-ssl connection [PR #2272]
- detect OS during runtime [PR #2452]

### Deprecated
- config: deprecate file daemon as alias for client in FD config [PR #2187]
- glusterfs: deprecate plugin and backend [PR #2416]

### Fixed
- plugins: Fix typo in postgresql plugin [PR #2066]
- python-bareos: Add missing dh-python build dep [PR #2130]
- debian: Add missing build dependencies [PR #2128]
- added build-dep to libutfcpp-dev in debian-like environments [PR #2056]
- increase accepted warnings for windows [PR #2191]
- stored: fix sd volume limit [PR #2264]
- python: fix traceback generation [PR #2303]
- cats: fix missing locks [PR #2331]
- correct backend init command in PyPI instructions (fixes #2380) [PR #2381]
- debian: Do not make dbconfig sql files executable [PR #2404]
- plugin: change prefix @hyper-v@ -> @HYPER-V [PR #2431]
- cats: fix version.map.in [PR #2064]
- webui: fix for PHP < 7.3 [PR #2067]
- cmake: fix MARIADB_MYSQL_INSTALL_DB_SCRIPT usage [PR #2040]
- Fix building ndmjob program [PR #2079]
- bareos-config-libs: double quote dbconfig values [PR #2111]
- freebsd: automate creation of pkg-plist.database-postgresql [PR #2102]
- plugin: postgresql use integer for savepkt.object_index [PR #2132]
- bconsole: enable app icon on windows [PR #2105]
- windows: fix readlink buffer size issue [PR #2153]
- Fix btape fill-test problem [PR #2018]
- cmake: add cmake_parse_arguments policy CMP0174 [PR #2169]
- VMware plugin: fix check_mac_address() for vm.config not present [PR #2059]
- scheduler: 'last' keyword doesn't allow job to be visible in status dir [PR #2120]
- fix autodeflate messages and refactor setup method [PR #2121]
- stored: fix crash when using jit reservation with no matching device; fix reservation error [PR #2141]
- Fix 32bit compilation [PR #2175]
- config: fix issues with config directive aliases [PR #2159]
- cleanup: fix multi-config tests [PR #2202]
- mssqlvdi-fd: add support for filestream backups [PR #2072]
- cmake: fix not including BareosTargetTools in systemtests, needed for get_target_output_dir [PR #2232]
- fix MaximumConcurrentJobs in docs and defaultsconfig [PR #2225]
- cli-test: fix test on freebsd [PR #2241]
- core: refactor config parser; fix ktls configuration; fix crashes/ub [PR #2222]
- contrib add support for mariadb 11+ [PR #2215]
- mariabackup: reset self.mycnf to string type [PR #2252]
- dird: fix bugs in DateTime [PR #2260]
- config: fix Director -> Director resource [PR #2259]
- mtx-changer: make mandatory test mt-st versus cpio-mt [PR #2256]
- packaging: set all `*.conf.examples` as %config(noreplace) [PR #2268]
- disable bareosfd-python3-module-test on FreeBSD [PR #2278]
- fix problems with msvc 19.44 [PR #2287]
- plugins: fix error_string construction [PR #2273]
- dplcompat: fix unsupported size-suffixes on chunksize [PR #2240]
- gRPC: add fixes to enable building on SUSE [PR #2250]
- fix admin job issues [PR #2283]
- truncate: fix return status bug [PR #2300]
- systemtests: add config-default test [PR #2332]
- VMware Plugin: Adapt to pyVmomi 9 [PR #2341]
- VMWare Plugin: Fix VirtualSerialPort, NVRAM timeouts configurable [PR #2344]
- fix next-pool overrides by job [PR #2279]
- stored: fix race condition [PR #2359]
- dird: fix nextvol crash [PR #2335]
- grpc: fix missing some early events [PR #2333]
- grpc: fix extra / for directories [PR #2373]
- packaging: add setcap/getcap requirements [PR #2347]
- scsicrypto: don't expose kek in job log [PR #2386]
- RPM: fix restarting bareos-fd on upgrades from bareos <= 24 [PR #2395]
- dird: fix mark crash [PR #2401]
- Fix droplet build [PR #2412]
- Fix some spelling issues [PR #2403]
- debian: remove dbc_debug to avoid username/password leakage [PR #2417]
- uniformized variables [PR #2391]
- plugin: postgresql do testing with wal compress on [PR #2408]
- cmake: fix traymonitor startup on windows [PR #2423]
- packaging: bareos-contrib-tool depend on python3-bareos [PR #2432]
- packaging: make contrib-tools use defaultsconfig [PR #2445]
- debugedit: fix messing with mtime [PR #2429]
- ndmp native: fix problem with new 'unlabeled' media status [PR #2456]

### Documentation
- docs: fix grpc-fd plugin call [PR #2068]
- docs: add devtools documentation [PR #2149]
- docs rear: update to rear >= 2.8 [PR #2220]
- docs: Python plugin dependencies installation [PR #2315]
- docs: update disaster recovery chapter [PR #2310]
- Update dir-director-MaximumConcurrentJobs.rst.inc with a warning about MaximumConcurrentJobs modification on running Directors [PR #2394]
- doc: don't alter configuration on package updates [PR #2392]
- docs: introduce 3rd party plugin (Yuzuy Qumulo) [PR #2418]
- Add historic data to CHANGELOG.md [PR #2434]
- doc: storage backend add note about static build [PR #2350]
- doc: add views & functions to developer catalog service chapter [PR #2328]

### Added
- added build-dep to libutfcpp-dev in debian-like environments [PR #2056]
- add media_vault tool to contrib [PR #2357]

### Changed
- plugins: Fix typo in postgresql plugin [PR #2066]
- Sync EvpDigest between OpenSSL <1.1 and 1.1+ [PR #2086]
- winbareos-native.nsi: do not package python3 plugins [PR #2076]
- logrotate: add mtx-changer debug log config [PR #2039]
- cmake: add cmake check whether tirpc is installed [PR #2109]
- bconsole: require only one password in the configuration [PR #2116]
- openssl: unify ssl error logging [PR #2078]
- Inherit RunScript elements between JobDef resources [PR #2097]
- python-bareos: Add missing dh-python build dep [PR #2130]
- stored: list all devices if device is invalid/missing [PR #2122]
- Refactor some Xattr and Acl internals [PR #1893]
- debian: Add missing build dependencies [PR #2128]
- systemtests: fail if daemons have config warnings [PR #2144]
- contrib: check_chunk.py improve README.md instructions [PR #2147]
- build: add support for el10 [PR #2106]
- VMware Plugin: Fix CBT query handling [PR #2152]
- increase accepted warnings for windows [PR #2191]
- webui: use TemporaryDir as userdatadir in Selenium test [PR #2194]
- ndmp: remove ndmp backup level limit [PR #2188]
- reduce test-matrix [PR #2192]
- bareos-fd: add option for grpc fallback [PR #2104]
- Make tests optional [PR #2180]
- windows: single-config/single-output directory [PR #2211]
- webui: use WEB_UI path in apache bareos-webui.conf file [PR #2201]
- config: update directives and error out on bconsole config warnings [PR #2217]
- config: add virtual file changer example + documentation [PR #2090]
- stored: fix sd volume limit [PR #2264]
- various cmake improvements [PR #2176]
- packaging: use GCC 14 for RHEL 8/9 [PR #2275]
- setgid on configdirs [PR #2270]
- Replace Bsnprintf() implementation and add format attributes [PR #1697]
- cmake: disable lto when linking gtests [PR #2286]
- VMware Plugin: Fix for virtual USB devices [PR #2213]
- build: add Fedora 42 [PR #2263]
- python: fix traceback generation [PR #2303]
- build: add Debian 13 [PR #2290]
- bareos-check-sources: add shell_format plugin [PR #2267]
- Build FreeBSD for major versions 14 / 13 (instead of minor releases) [PR #2117]
- matrix: test the Debian ULC packages for OpenSSL 1.1 on Debian 11 instead of Debian 10 [PR #2321]
- ndmp-bareos: Introduce incremental loop, restore 2nd file explicitly [PR #2269]
- systemtest: always incremental add one job with one removed file [PR #2329]
- webui: add summary subscription report [PR #2340]
- set environment vars in bpipe fd plugin [PR #2205]
- systemtest: speed up always-incremental with sleep 0.25 [PR #2358]
- cats: fix missing locks [PR #2331]
- storage: improve tapealert plugin [PR #2370]
- correct backend init command in PyPI instructions (fixes #2380) [PR #2381]
- debian: Do not make dbconfig sql files executable [PR #2404]
- systemtests: speed up testing [PR #2376]
- webui: migrate from ZF2 to Laminas [PR #2385]
- don't alter configuration on ULC package updates [PR #2399]
- stored: implicitly create autochanger from device with count > 1 [PR #2198]
- plugins: introduce hyper-v backup [PR #2284]
- defaultconfig: introduce automatic diskchanger [PR #2427]
- plugin: change prefix @hyper-v@ -> @HYPER-V [PR #2431]
- plugins: introduce windows disaster recovery  imager (barri) [PR #2285]
- webui: use patched laminas packages [PR #2439]
- plugins: add proxmox backup [PR #2372]
- Introduce build of Fedora 43 [PR #2407]
- jobreport: show FD Bytes Read [PR #2443]
- macos: use macos-15 runner [PR #2442]
- Improve status subscription [PR #2444]
- Refactor libcloud plugin to work with current Python versions [PR #2428]

## [24.0.0] - 2024-12-16

### Breaking changes
- Support for base jobs is phased out. You can still restore jobs that use a base job, but you cannot create new backups using a base job anymore. Bareos 25 might remove restore-support for this, too.
- When running `bconsole` as superuser, critical local commands `@input` `@output` `@tee` and `@exec` are disabled
- Bareos will no longer write PRE_LABEL labels on empty media, but instead write normal labels.

### Added
- Add dedupable backend [PR #1663]
- Add droplet compatible storage backend [PR #1790]
- Add just in time reservation:  reserve/acquire a device on first incoming write data [PR #1715]
- Add new grpc plugin allowing to run python plugins in own process [PR #1993]
- Add support for WORM media [PR #1958]
- Add openssl 3 ulc [PR #1683]

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
- pr-tool: add backport tool [PR #1639]
- Build FreeBSD 13.3 and 14.0 [PR #1765]
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
- docs: add ADR infrastructure and first ADRs [PR #1945]
- nsi installers: remove postgresql backend [PR #1914]
- systemtests: fix plugin postgresql testrunner-* [PR #1947]
- webui: fix handling deleted clients in restore browser [PR #1933]
- console.cc: forbid @exec etc. as privileged user [PR #1950]
- github actions python-bareos: add workflow_dispatch [PR #1966]
- FreeBSD: fix sed inplace usage, use bin/sh as shebang for script, pkg make director dependent of database-postgresql [PR #1961]
- dir: fix DbLocker usage [PR #1953]
- debian: fix dedupable backend names [PR #1977]
- external packages: use CPM packages instead of third-party directory [PR #1963]
- scripts: remove unmaintained bareos-explorer [PR #1990]
- build: add support for Fedora 41 [PR #1996]
- filed: detect integer overflow during backup [PR #1952]
- dedupable device: fix example files [PR #2029]
- bareosfd_test.py: allow a delta of 1.1 [PR #2031]
- mssql TearDownVdiDevice(): enhance error reporting [PR #2008]
- macos: update build environment [PR #2035]
- NDMP_NATIVE: enable full restore, eject tape before unload, enable update of NDMP environment [PR #1862]
- vmware: upgrade vmware vix disklib to version 8.0.3
- plugins: Fix typo in postgresql plugin [PR #2071]

### Removed
- plugins: remove old deprecated postgres plugin [PR #1606]
- Remove EOL platforms [PR #1684]
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
- docs: add worm support paragraph [PR #2037]
- docs: fix grpc-fd plugin call [PR #2069]

### Fixed
- core: fix some overlooked issues in msvc merge [PR #1912]
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
- stored: explicitly flush after labeling [PR #2022]
- bareos-version-from-git: fix output for wip tags [PR #2019]
- lib: add source_location; fix test crash [PR #2006]
- plugins: postgresql add support for PostgreSQL 17 + improvements [PR #2004]
- cats: fix version.map.in [PR #2065]
- webui: fix for PHP < 7.3 [PR #2070]

## [23.0.0] - 2023-12-13

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
- Improve python plugin configuration [PR #1619]
- tls: add tls v1.3 configuration option [PR #1577]
- restore: add fileregex parameter [PR #1618]
- webui: add machine-readable copyright file [PR #1419]
- webui: add config resource graph to analytics (experimental) [PR #1412]
- build: add Debian 12 [PR #1477]
- build: add Fedora 38 [PR #1563]
- build: add Fedora 39 [PR #1615]
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
- macOS: fix linking problem for macOS >= 14 [PR #1620]
- filedaemon: remove ovirt plugin [PR #1626]
- vadp-dumper: fix multithreaded backup/restore issues [PR #1623]
- VMware Plugin: Run bareos_vadp_dumper with optimized parameters to increase backup performance [PR #1631]

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

## [22.0.0] - 2022-12-21

### Breaking Changes
- when using PAM Bareos will now check authorization, too. If authorization is not configured, login will fail. See [updated documentation](https://docs.bareos.org/TasksAndConcepts/PAM.html#configuration) on how to proceed [PR #1115].
- Bareos 22 removes the `make_catalog_backup.pl`  **perl** script in favor of an improved `make_catalog_backup` **shell** script. There is a compatibility-wrapper, but that only supports simple cases. If you applied changes to `BackupCatalog` Job, you should immediately migrate to use `make_catalog_backup`. See [catalog documentation](https://docs.bareos.org/TasksAndConcepts/CatalogMaintenance.html#backing-up-your-bareos-database) [PR #1081]
- Bareos changed the way its binaries parse CLI arguments. Four existing options now require two minus signs (`--`) instead of one (`-`) :
    - `-xc` -> `--xc` to export all configuration at once.
    - `-xc[resource[=<name>]]` -> `--xc [resource [ressource-name]]` to export a specific resource.
    - `-xs` -> `--xs` to export configuration JSON schema.
    - `-dt` -> `--dt` to print the debug timestamp.
  The rest of the options stay the same. [PR #1187]
- Previously deprecated **rados/ceph backend** and **cephfs plugin** have been **removed**. Instead, please use the droplet backend to store into ceph via S3, and backup cephfs filesystems by backing up a mount point.
    - packages bareos-storage-ceph and bareos-filedaemon-ceph-plugin if previously installed **need to be removed manually**.
- Bareos 22 **removes bareos-webui support** for **RHEL 7** and **CentOS 7**
- Bareos 22 uses the VMware VDDK 8.0.0 for the VMware Plugin. [PR #1295]. VDDK 8.0.0 supports vSphere 8 and is backward compatible with vSphere 6.7 and 7. **vSphere 6.5 is not supported anymore**.
- Bareos 22 bareos-webui now uses **php_fpm instead of mod_php**. [PR #1287]. Update should run smoothly, for details check the "Installing Bareos Webui" chapter in the docs.

### Added
- The bareos repositories now contain `add_bareos_repositories.sh` which will setup the corresponding repository automatically
- VMware Plugin: Save VM configuration and recreate VM before restore [PR #1295]
- python plugins: enable direct I/O in core instead of calling python for read and write [PR #1297]
- NDMP_BAREOS: add support for autoxflate plugin [PR #1013]
- stored: dird: add backup checkpoints that save backup metadata to the Catalog during the execution of the backup. [PR #1074]
- dird: add command line feature to print specific resources [PR #1153]
- dird: extend the list command to be able to query volumes and pools by ID [PR #1041]
- ndmp: introduce workaround for isilon 9.1.0.0 'Invalid nlist.tape_offset -1' error [PR #1043]
- packaging: installation and upgrade will check for the presence of :file:`.enable-cap_sys_rawio` in your bareos config dir and will configure the required cap_sys_rawio capabilities [PR #1057]
- webui: analytics module: show stored data per jobname in treemap [PR #1082]
- webui: add pool column to volume lists [PR #1120]
- webui: introduce themes [PR #1281]
- dird: console: add `jobtype` parameter to `(l)list jobs` [PR #1149]
- contrib: add support for building and packaging [PR #768]:
  - bareos-contrib-director-python-plugins
  - bareos-contrib-filedaemon-python-plugins
  - bareos-contrib-tools
- tests: py3plug-fd-contrib-mysql_dump [PR #768]
- tests: py*plug-fd-contrib-bareos_tasks_mysql [PR #768]
- webui: introduce rerun of multiple jobs at once [PR #1109]
- dird: console: add the ability to rerun multiple commas separated jobids [PR #1170]
- build: Add support for Ubuntu 22.04, Fedora 36, EL 9, openSUSE 15.4 [PR #1179]
- build: Add support for SLE_15_SP4 [PR #1205]
- libcloud plugin: allow to configure the storage provider [PR #1226]
- core/platform: Adding Bareos firewalld service xml files [PR #1237]
- dird: Added `FS Type = vfat` in LinuxAll.conf for UEFI partition [PR #1236]
- bareos tools: reintegrate testfind binary [PR #1176]
- fd: add support for role switching on PostgreSQL add-on [Issue #1456] [PR #1178]
- build: switch from FreeBSD 13.0 to 13.1 [PR #1253]
- build: run a build and test with sanitizers enabled [PR #1244]
- catalog: update fileset text in fileset record [PR #1300]
- stored: emit warnings for Maximum Concurrent Jobs in device configs [PR #1282]
- webui: extend restore to handle plugin options [PR #1276]
- webui: introduce subscription backup unit report [PR #1280]

### Fixed
- webui: fix job timeline x-axis UTC timestamp issue [PR #1283]
- dird: fix possible crash in tls context on configuration reload [PR #1249]
- dird: RunScript fixes [PR #1217]
- fix file count mismatch on restores that use recycled volumes [PR #1330]
  - fix show command output for RunScript RunsOnClient
  - fix show verbose for RunScripts
  - execute console runscripts only on the Director
- VMware file daemon plugin: fix restore with `localvmdk=yes` requires an API connection to vCenter [PR #1219]
- python plugins: store architecture specific modules in sitearch (instead of sitelib) [PR #698]
- debian: fix package dependencies for webui and Ceph [PR #1183]
- Python plugins: fix handling of additional pluginoptions parameter [PR #1177]
- debian: Let `dbconfig` create the Bareos catalog also with `LC_COLLATE='C'` and `LC_CTYPE='C'`. The create_bareos_database script did always do so. Requires `dbconfig >= 2.0.21` [PR #1031]
- fix wrong `packages_dir` in restapi workflow, so restapi packages will be released to PyPI [PR #1033]
- core cats: Add `IF EXISTS` in `drop table` statements fix for bug #1409 (Allow usage of ExitOnFatal) [PR #1035]
- sql_get.cc: fix error logging in `GetJobRecord()` for jobname [PR #1042]
- webui: fix empty job timeline issue if `date.timezone` is not set in `php.ini` [PR #1051]
- Fix for wrong update message when updating all volumes from all pools with no existing volumes [PR #1015]
- Fix context confusion in Director's Python plugins [PR #1047]
- Fix several cases of undefined behaviour, memory corruption and memory leaks [PR #1060]
- webui: fix undefined array key warning [PR #1098]
- webui: fix deprecated notice required param follows optional param [PR #1097]
- webui: fix uncaught `TypeError` if node.data is null [PR #1087]
- core cats: Add `DROP VIEWS` instruction in `drop_bareos_table` script [PR #1092]
- Don't keep volume open after acquiring a read-storage failed in migrate/copy/virtual full [PR #1106]
- webui: show DIR message if ACL prevents a job rerun [PR #1110]
- webui: fix restore file tree rendering [PR #1127]
- dir: fix crash when there are no jobs to consolidate [PR #1131]
- webui: get volume and pool params from query instead of route [PR #1139]
- packaging: FreeBSD add missing `ddl/update` `2171_2192` and `2192_2210` files [PR #1147]
- Fix director connects to client while `Connection From Director To Client` is disabled. [PR #1099]
- cats: make `.bvfs_update` and `.bvfs_versions` take archive jobs into consideration [PR #1152]
- Fix `always-incremental-consolidate` systemtest sporadic fails, and rename it. [PR #1154]
- packaging: FreeBSD place all scripts into "normal" location /usr/local/lib/bareos/scripts [PR #1163]
- [Issue #1445] adding quotes to director name when using `configure export`. [PR #1171]
- dir: miscalculation when using `always incremental keep number` [PR #1159]
- Windows Installer: made 'Director PotsgreSQL Backend Support' checked by default if 'Full PostgreSQL' installation selected. [PR #1185]
- SQL: queries: fix sql queries to handle negative job duration value [PR #1198]
- dird: fix TLS-PSK credential not found error with very long job names [PR #1204]
- dird: Add missing newline to job message for TLS handshake [PR #1209]
- devtools/dist-tarball.sh: fix name if version contains "~pre" [PR #1221]
- dird: fix odd-even weeks parsing bug in schedule [PR #1210]
- bcopy: fix crash in `bcopy` when using certain cli arguments [PR #1211]
- webui: fix password string length limitation [Issue #1480][PR #1251]
- systemtest: fixed issues with systemtests not succeeding on first try [PR #1186]
- btape: dumplabel only when label is valid [PR #1266]
- dird: fix crash in `.jobstatus` [PR #1278]
- testfind: remove unnecessary libraries and fix systemtest [PR #1250]
- stored: systemtests: docs: checkpoints improvements [PR #1277]
- winbareos.nsi: fix working directory in configure.sed [PR #1288]
- core: BareosDb::FindLastJobStartTimeForJobAndClient: take into account Running job [Issue #1466] [PR #1265]
- backup.cc: fail backup when `Write Bootstrap` to pipe fails [PR #1296]
- webui: fix pool link in job details formatter [Issue #1489] [PR #1303]
- webui: patch zf2 to eliminate a php warning caused by zend-stdlib [PR #1305]
- dird: fix director resource not showing when using `show director` or the `--xc director` cli option [PR #1315]
- webui: add timeline chart by jobs [PR #1059]
- bareos-fd-postgres: properly close database connection [PR #1326]
- filed: fix handling of `STREAM_ACL_PLUGIN` during restore [PR #1308]
- dird: fix tls protocol shown and document TLS Protocol & ciphers restriction [PR #1319]
- dird: fix for crash when starting rescheduled jobs [PR #1327]
- VMware Plugin: fix restore of backups taken before version 22 [PR #1337]

### Changed
- contrib: rename Python modules to satisfy PEP8 [PR #768]
- contrib: adapt to Python interface of Bareos >= 20 [PR #768]
- Qmsg: in case of syslog logging use adapted log priority instead of always LOG_ERR [PR #1134]
- webui: remove an unnecessary `.bvfs_get_jobids` and `buildSubtree()` call [PR #1050]
- git: set merge strategy for CHANGELOG.md to union [PR #1062]
- stored: enable labeling of tapes in drives even if `autoselect=no` [PR #1021]
- dir, stored: start statistics threads only if needed [PR #1040]
- gitignore: cleanup `.gitignore` files [PR #1067]
- webui: update jstree from v3.3.8 to v3.3.12 [PR #1088]
- webui: update jstree-grid plugin [PR #1089]
- dird: Consolidation now purges candidate jobs with no files instead of ignoring them [PR #1056]
- dird: Virtual Full will now terminate if one of the input jobs had its files pruned [PR #1070]
- webui: new login screen background and adapted logo to support Ukraine  [PR #1122]
- console: multicolumn output: fill columns first [PR #1072]
- cats: include only jobtypes in `list jobtotals` that write data to volumes [PR #1135]
- jstreegrid: remove handling of IE < 8 using navigator interface to avoid warnings in chrome [PR #1140]
- dird: `bvfs_update` now uses `unordered_map` instead of `htable` for the pathid cache [PR #1138]
- cats: filtered zero file jobs list is now sorted [PR #1172]
- dird: console: changed list jobs jobstatus argument to accept comma separated value [PR #1169]
- cats: management scripts remove `db_driver` support [PR #1081]
- bconsole: multiple identical successive commands are only added to history once [PR #1162]
- build: Now use solaris 11.4-11.4.42.0.0.111.0 [PR #1189]
- bconsole: removed commas from jobid attribute in list jobs and llist jobs outputs [PR #1126]
- testing: `matrix.yml`: run multiple tests sequentially [PR #1193]
- console: aborting job run if jobid doesn't exist in catalog [PR #1188]
- daemons: changed daemon CLI arguments parsing [PR #1187]
- config parser: Refactor config reloading by using shared pointers instead of callbacks [PR #1151]
- tests: remove unused config files [PR #1247]
- dird: fix config reload and unit tests dependency issue [PR #1161]
- pruning: `prune jobs` doesn't ask for jobtypes anymore, and prunes all jobtypes except Archives (`A`) [PR #1215]
- dird: cats: remove copy and migration jobs with no data from catalog [PR #1262]
- build: enable compiling on ARM [PR #1270]
- core and webui: adapt binary info messages to new wording [PR #1298]
- build: enable `-Wextra` warning level and apply required changes [PR #1261]
- lib: make `foreach_res()` reload-safe [PR #1279]
- build: prepare Bareos for an upgrade to the C++20 standard [PR #1271]
- stored: refactor the SD's backend interface [PR #1272]
- core: use distinct names for JobControlRecordPrivate [PR #1307]
- webui-selenium-test: use `options` instead of `chrome_options` [PR #1306]
- systemtests: improve webui testing [PR #1313]
- dird: prohibit PAM usage with user ACL and Profiles in consoles [PR #1318]
- webui: cleanup webui source tree [PR #1314]
- dird: do device reservation as late as possible, i.e. after run before job scripts [PR #1273]
- ndmp_tape.cc: do not log current rctx->rec in joblog [PR #1324]
- dird: stored: set statistics collection as deprecated [PR #1320]
- webui: switch from `mod_php` to `php-fpm` [PR #1287]
- dird: status subscription: extend output [PR #1312]
- build: unify and merge builds where possible [PR #1309]
- python plugins: give python3 plugins priority over python2 plugins in packages [PR #1332]
- btraceback: make the gdb script processing failsafe [PR #1334]
- Python plugins: add default module_path to search path [PR #1038]
- systemtests: wait for mariadb shutdown [PR #1048]
- tests: simplify test coverage analysis [PR #1010]
- tests: skip mysql tests if root [PR #1197]
- webui: adapt links to new URLs after website relaunch. [PR #1275]

### Deprecated
- `make_catalog_backup.pl` is now a shell wrapper script which will be removed in version 23.
- marked config directive `Compatible` as deprecated [PR #1284]
- deprecated `Maximum Connections` directive from all daemons and removed all uses in code. Directive has no effect anymore [PR #1285]
- deprecate python2 plugins [PR #1331]

### Removed
- removed the `-r` run job option. [PR #1206]
- removed ceph/rados backend and filedaemon plugin [PR #1216]

### Security
- webui: update jquery from v3.2.0 to v3.6.0 [PR #1083]
- dird: check authorization on PAM login [PR #1115] [CVE-2022-24755](https://github.com/bareos/bareos/security/advisories/GHSA-4979-8ffj-4q26)
- dird: fix memory leak on failed PAM login [PR #1115] [CVE-2022-24756](https://github.com/bareos/bareos/security/advisories/GHSA-jh55-4wgw-xc9j)
- webui: update moment.js to version 2.29.2 [PR #1155] [CVE-2022-24785](https://github.com/advisories/GHSA-8hfj-j24r-96c4)
  - webui is NOT affected
  - webui does NOT use the npm server
  - webui does NOT use a user-provided locale string to directly switch moment locale
- webui: Fix URL rewrite vulnerability in zend-http component [PR #1213] [No known CVE](https://github.com/advisories/GHSA-f6p5-76fp-m248)

### Documentation
- Univention Corporate Server (UCS) has no longer extended integration, just normal Linux integration [PR #1242]
- cleanup update section [PR #1054]
- clarifies MySQL catalog migration process [PR #1054]
- split `Howtos.rst` file into one file per section [PR #1054]
- split the very long `Plugins.rst` file into one file per Bareos plugin [PR #1046]
- rework SD plugin scsicrypto linux `sg_io ioctl` subsection for `cap_sys_rawio` [PR #1057]
- improve action Python plugin documentation, by removing File in Fileset example [PR #1079]
- improve Mysql - PostgreSQL howto [PR #1093] fixing [Issue #1429]
- clarifies Sphinx bareos-extension `parallel_read_safe` status to False [PR #1037]
- fix incorrect link in contrib PythonFdPlugin [Issue #1450] [PR #1065]
- clarifies CheckFileChanges option not intended to be used with plugin [Issue #1452][PR #1180]
- fix broken links with sphinx linkcheck [PR #1200]
- add security `FIPS` section [PR #1181]
- add instruction to run sphinx linkcheck before release [PR #1218]
- FreeBSD install create `/usr/local/etc/pkg/repos` directory [PR #1227]
- remove deprecated rados & ceph references from documentation [PR #1216]
- improve Wild(*) section with links to example [PR #1228]
- add example to selection `type = sql query` [PR #1229]
- dir-job-RescheduleTimes remove wrong default sentence [PR #1225]
- update Appendix/HardwareSizing DB size with new numbers and formulas [Issue #1477][PR #1231]
- add description to fileset signature `sha256` and `sha512` parameter [PR #1230]
- improve troubleshooting and debugging chapter [PR #1233]
- mssql add a warning in case of pitr to run another backup full or diff afterwards [PR #1235]
- docs: Added Developer FAQ section with first question. [PR #1202]
- describe Debian installation based on `add_bareos_repositories.sh` [PR #1238]
- update Pull Request workflow description [PR #1243]
- docs: improve operating system table [PR #1254]
- add FAQ entry about howto upgrade from `Bareos < 20` with Bareos Python packages installed [PR #1260]
- describe usage of the `add_bareos_repositories.sh` script [PR #1248]
- Appendix/Bareos Programs improvements [PR #1255]
- obsolete comments removed [PR #1268]
- fix bsmtp `get-usage.sh` call [PR #1267]
- add new VSS troubleshooting instruction to Windows chapter [PR #1317]
- update installation and renew update chapters [PR #1329]
- add chapter about Bareos Binary Release Policy [PR #1333]
- add chapter for `mariabackup` db plugin [PR #1016]

## [21.0.0] - 2021-12-21

* 300+ PRs
* 1.000+ commits since Bareos 20
* 30 contributors
* 72.170 line added
* 318.812 lines removed

### Breaking Changes
- Bareos now automatically reconnects to the catalog database after the connection was lost. To keep the old behaviour, set `Reconnect = no`  in your Catalog resource. [PR #860]
- Bareos now listens on IPv4 and IPv6 by default. If you currently use `[DIR|SD|FD] Address` or `[DIR|SD|FD] Address`, you may need to update your configuration to get the same behaviour as before. [PR #882]
- Support for MySQL and SQLite catalog backends has been removed. Switch to PostgreSQL via `bareos-dbcopy` on Bareos 20 before upgrading. [PR #949]
- Bareos no longer supports Bacula tape formats <= 10 (Bacula <= 1.26 which was released 2002). [PR #1019]
- Deprecated configuration directives have been removed. If you have any configuration settings deprecated in Bareos 20, you will need to remove these before upgrading. [PR #938]

### Fixed
- [Issue #1374] Include zero-file incremental backups in always-incremental consolidation [PR #995]
- [Issue #847]: fix for [CVE-2017-14610](https://github.com/bareos/bareos/security/advisories/GHSA-426r-vgh8-vrw8) PID files that could be exploited on certain systems [PR #928]
- [Issue #1194]: when doing an accurate incremental backup, if there is a database error, a full backup is done instead of reporting the error [PR #810]
- fix a bug in VMware plugin where VMDK Files were created with wrong size when using the option `localvmdk=yes` [PR #826]
- fix a bug where the restore browser would not recognize globbing wildcards in paths [PR #801]
- [Issue #1329]: If CommandACL limits any command, no messages can be read but "you have messages" is displayed. [PR #763]
- fix lost byte in ChunkedDevice [PR #910]
- fix director crash on "update slots" when there is a parsing issue with the autochanger or tape devices [PR #919]
- Fix occassional "NULL volume name" error when non-busy, but blocked drive is unloaded [PR #973]
- reorder acquire on migrate/copy to avoid possible deadlock [PR #828]
- fix scheduler running disabled jobs after executing the disable command [PR #924]
- fix shutdown of the Storage Daemon backends, especially call UnlockDoor on tape devices [PR #809]
- fix possible deadlock in storage backend on Solaris and FreeBSD [PR #809]
- fix possible read/write problems when using droplet with https [PR #765]
- fix "configure add" handling of quoted strings [PR #764]
- fix invalid file descriptor issue in the libcloud plugin [PR #702]
- fix crash when loading both python-fd and python3-fd plugins [PR #730]
- fix parallel python plugin jobs [PR #729]
- fix oVirt plugin problem with config file [PR #729]
- [Issue #1316]: storage daemon loses a configured device instance [PR #739]
- fix python-bareos for Python < 2.7.13 [PR #748]
- fixed bug when user could enter wrong dates such as 2000-66-100 55:55:89 without being denied [PR #707]
- fix volume-pruning to be reliable on all test platforms [PR #761]
- fix memory leak in python module constants [PR #778]
- fix systemtests: reduce the number of broken tests [PR #771] [PR #791]
- fix gfapi-fd: avoid possible crash on second glfs_close() call [PR #792]
- [Issue #1205]: PHP 7.3 issue with compact() in HeadLink.php [PR #829]
- fix drive parameter handling on big endian architectures [PR #850]
- [Issue #1324]: Infinite loop when trying to log in with invalid account [PR #840]
- [Issue #579]: Unable to connect to the director from webui via ipv6 [PR #868]
- [Issue #1300]: some job status are not categorized properly [PR #874]
- [Issue #871]: UI will not load complete [PR #880]
- [Issue #1020]: Can not restore a client with spaces in its name [PR #893]
- [Issue #971]: Error building tree for filenames with backslashes [PR #892]
- [Issue #1251]: Error when displaying pool detail [PR #903]
- [Issue #1369]: webui tries to load a nonexistent file [PR #900]
- [Issue #1232]: bareos logrotate errors, reintroduce su directive in logrotate [PR #918]
- [Issue #1334]: After deleting storage from the configuration, it still persists in the catalog db [PR #912]
- [Issue #1191]: The web interface runs under any login and password [PR #936]
- Adapt Python DIR and SD plugin Baseclasses to the modernized Python plugin API [PR #923]
- Fixed all compiler warnings (on our default warning level) [PR #948]
- Log LDAP info error (e.g. expired SSL cert error) [PR #956]
- Adapt percona-xtrabackup test to work on updated test environment [PR #982]
- fix crash in "status scheduler" command when job->client is unset [PR #965]
- webui: fix a layout corner case where top navbar is hiding navtabs [PR #1022]
- webui: client details now can be displayed for client names containing dots [PR #1023]
- Fixed issue with error messages not showing up properly on windows systems [PR #959]
- Fixed libdroplet xattr.h include issue by using sys/xattr.h [PR #985]
- Fixed crash on bconsole when using autcomplete with tab [PR #969]
- Fix PostgreSQL create database script [PR #981]
- Unify level use with set client_min_message instruction in SQL update scripts [PR #981]
- dird: avoid crash in job listing when "current" and "count" are both specified at the same time [PR #1026]
- Fixed issue with `llist jobs last current enable` that would not display correct data [PR #1025]
- fix a bug in a date function that leads to errors on the 31st day of a month [PR #782]
- fix config-dump systemtest [PR #736]
- fix systemtests daemon control scripts [PR #762]


### Added
- plugin: added mariabackup python plugin, added systemtest for mariabackup and updated systemtest for percona-xtrabackup [PR #967]
- Build the package **bareos-filedaemon-postgresql-python-plugin** also for Debian, Ubuntu and UCS (deb packages) [PR #723].
- add `reload` commands to systemd service [PR #694]
- add multicolumn prompt selection for selection of more than 20 items [PR #731]
- add choice for the drive number in the truncate command [PR #823]
- support for shorter date formats, where shorter dates are compensated with lowest value possible to make a full date [PR #707]
- added option to delete selected storage in bconsole if it is orphaned [PR #912]
- webui: add inchanger column to volume tables [PR #998]
- webui and console: add job duration column in job listings [PR #1007]
- console: prune command gained support to prune multiple volumes at once [PR #966]
- webui: introduce a job timeline chart [PR #1017]
- add support for Universal Linux Client [PR #1019]
- systemtests: make database credentials configurable [PR #950]
- systemtests: allows multiple subtests per systemtest [PR #857]
- systemtests: replaced the six reload-* tests by one test [PR #857]
- systemtests: replaced fileset-multiple-include-blocks and fileset-multiple-options-blocks by fileset-multiple-blocks test [PR #857]
- systemtests: add systemtests fileset-multiple-include-blocks, fileset-multiple-options-blocks, quota-softquota, sparse-file, truncate-command and block-size, (migrated from ``regress/``) [PR #780]
- add bvfs and dbcheck tests to python-bareos systemtest [PR #780]
- systemtests for NDMP functionalities [PR #822]
- systemtests for S3 functionalities (droplet, libcloud) now use https [PR #765]
- add an informative debugmessage when a dynamic backend cannot be loaded [PR #740]
- add external repo bareos-contrib as subtree [PR #752]
- add script devtools/dist-tarball.sh to produce the same tarball from a cloned repo everywhere [PR #861]
- packages: Build also for openSUSE Leap 15.3 [PR #870]
- systemtest:bareos test now also runs on btrfs filesystem [PR #907]
- packages: Build also for Fedora_34 [PR #869]
- packages: Build also for Debian_11 [PR #914]
- packages: Build also for SLE_15_SP3 [PR #952]
- add job name in End Job Session output in bls tool [PR #916]
- added check for orphaned storages in dbcheck [PR #912]
- contrib: Add Python DIR plugin for prometheus metrics [PR #911]
- bench: added a benchmarks tool with google-benchmark, with an initial benchmark for the restore command [PR #883]
- database: Add index on jobmedia mediaid,jobid [PR #958]
- contrib: Add Python FD Plugin for OpenVZ container [PR #908]
- packages: Build also for Fedora_35 [PR #972]
- cmake: check for chflags() function and enable FreeBSD File Flags support [PR #963]
- packages: Build EL_8 packages instead of CentOS_8 for RHEL 8 derivates (Alma, Rocky, CentOS Stream)[PR #992]
- webui: provide a notification that NDMP restores are NOT supported by webui [PR #1020]
- systemtests: add a test for the jobs last status table in webui [PR #1024]

### Changed
- core: systemd service: change daemon type from forking to simple and start daemons in foreground [PR #824]
- debian: unify daemon (Dir,SD,FD) start behavior with RPM based distributions. Don't start any daemon after installation. Enable all daemons. Only restart FD after upgrade (before: FD has been started after installation, No daemon have been restarted on upgrade) [PR #1019] [PR #1029]
- core: cleanup systemd service dependencies: Requires network.target, but start after the network-online.target [PR #700]
- Enable c++17 support [PR #741]
- postgresql filedaemon plugin: switched from psycopg2 to pg8000, dropped support for python2.
- reconnecting to the database is now automatic per default without the need to specify it in the catalog [PR #860]
- bareos is now set to listen to both IPv6 and IPv4 by default, instead of needing to specify it via a directive [PR #882]
- bareos is now able to create IPv6 addresses with the `DirAddress` directive [PR #882]
- show more details about subscriptions and units data with `status subscriptions` [PR #989]
- webui: optimize bvfs update cache calls [PR #999]
- replace complicated memory pool allocator with plain malloc() [PR #997]
- systemtest python-bareos: split tests in separate files [PR #944]
- systemtests: define variable BackupDirectory globally [PR #780]
- systemtests: run all systemstests with ``set -o pipefail`` [PR #780]
- core: Make the jansson library mandatory when compiling the Bareos Director [PR #793]
- repaired or added all header guards in libdroplet [PR #765]
- When using Python > 3.7 the postgres and libcloud plugins will cancel the job and write an error message [PR #769]
- bstrncpy: workaround when used with overlapping strings [PR #736]
- Disabled test "statefile" for big endian, use temporary state files for all other architectures [PR #757]
- Package **bareos-database-postgresql**: add recommendation for package **dbconfig-pgsql**.
- Adapt the init scripts for some platform to not refer to a specific (outdated) configuration file, but to use the default config file instead.
- scripts: cleaned up code for postgresql db creation [PR #709]
- Change Copy Job behaviour regarding Archive Jobs [PR #717]
- py2lug-fd-ovirt systemtest: use ovirt-plugin.ini config file [PR #729]
- Ctest now runs in scripted mode. [PR #742]
- storage daemon: class Device: rename dev_name to archive_device_string (as the value stored here is the value of the "Archive Device" directive) [PR #744]
- webui: Localization updated [PR #776]
- running cmake for the core-directory only is now forbidden [PR #767]
- dird: ignore duplicate job checking on virtual fulls started by consolidation [PR #552]
- buildsystem: switch to cross build chain of Fedora 34 [PR #819]
- FreeBSD: adapt pkglists for FreeBSD 13.0 [PR #819]
- Fedora34: do not build mysql db backend, adapt pkglist [PR #819]
- bscan and bareos systemtests: also test bextract and bls binaries, use autoxflate plugin and FSType fileset options [PR #790]
- Windows release package now ships source code as optional component, so there is no need for a debug-package anymore [PR #858]
- version information was moved from core/cmake/ and webui/cmake/ directories into the toplevel cmake/ directory [PR #861]
- add chromedriver options to improve reliability of selenium tests [PR #920]
- webui: improve log message keyword highlighting [PR #937]
- webui: change retention/expiration column to only show retention on volume tables [PR #943]
- webui: introduce confirmation dialog on rerun and cancel job actions [PR #951]
- Allow `release` command to be used in scripts [PR #961]
- webui: format total jobs and total files language-sensitive [PR #996]
- webui: update localization [PR #1018]

### Deprecated
- Deprecated directives [PR #928] [PR #949]
  - Director
    - Director resource
      - `Pid Directory`
    - Catalog resource
      - `Db Driver`
  - Storage Daemon
    - Storage resource
      - `Pid Directory`
  - File Daemon
    - Client resource
      - `Pid Directory`

### Removed
- Remove MySQL and SQLite catalog backends [PR #949]
- Remove deprecated directives [PR #938]
  - Director
    - Director resource
      - `Omit Defaults`
      - `Sub Sys Directory`
    - Storage resource
      - `Sdd Port`
    - Pool resource
      - `Use Volume Once`
    - Job resource
      - `Write Part After Job`
      - Alias `Differential Max Wait Time` for `Differential Max Runtime`
      - Alias `Full Max Wait Time` for `Full Max Runtime`
      - Alias `Incremental Max Wait Time` for `Incremental Max Runtime`
      - Alias `Plugin Options` for `Fd Plugin Options`
    - Client resource
      - Alias `Allow Client Connect` for `Connection From Client To Director`
  - Storage Daemon
    - Device resource
      - `Free Space Command`
      - `Maximum Part Size`
      - `Write Part Command`
      - `Maximum Volume Size`
    - Storage resource
      - `Sub Sys Directory`
  - File Daemon
    - Client resource
      - `Sub Sys Directory`
- Debian: Old SysV init scripts have been removed. Bareos daemons are controlled by systemd [PR #1019]
- Removed language support files for the core daemons, as these files are outdated and not used at all.
- core: remove deprecated config directive `dbdriver` from all config files, templates and from automatic configuration during install [PR #1027]
- Remove regression tests (``regress/`` directory). Tests still relevant tests have been migrated into systemtests [PR #780]
- Removed support for IPv4-only build. IPv6 now is required for building bareos. [PR #913]
- Removed package lists for platforms no longer built: Fedora_30.x86_64, RHEL_6.x86_64, SLE_12_SP4.x86_64, openSUSE_Leap_15.0.x86_64, openSUSE_Leap_15.1.x86_64.
- Removed outdated support for platforms Alpha, BSDi, Gentoo, Irix and Mandrake.
- Removed outdated configuration files (example files).
- Removed package **bareos-devel**.
- Removed package **bareos-regress** and **bareos-regress-config**. The package **bareos-regress** has not been build for a long time.
- Removed unused script **breload**.
- Removed some workaround for Ubuntu 8.04.

### Security
- [CVE-2017-14610](https://github.com/bareos/bareos/security/advisories/GHSA-426r-vgh8-vrw8): PID files are now created before dropping privileges and before the parent process exits. [PR #928]

### Documentation
- add documentation about Unified Linux Client [PR #1028]
- added "Hardware Sizing" chapter [PR #926]
- Improve data-type subsubsection in CustomizingTheConfiguration. Add a special Sphinx directive for them (``config:datatype``) [PR #1008]
- add "copy button" to code snippets in documentation for easy copying [PR #802]
- Restore error "could not hard link" documented: what is the cause and how it can be avoided or solved. [PR #759]
- Developer guide: add small chapter about c++ exceptions. [PR #777]
- Fix links to configuration directives and issue warnings on dangling links [PR #1008]
- Adapted the documentation of the VMware plugin due to update to VDDK 7 [PR #844]
- declare shell scripts code blocks as "sh" instead of "shell-session" [PR #802]
- BareosSecurityIssues add remark about new systemd service (non forking) logged information into systemd-journal [PR #927]
- check if configuration directives are defined as CamelCase in core. Otherwise building the documentation will fail with an error [PR #1008]
- Describe how to get debugging info when using the VMware plugin [PR #921]
- Disable autotosectionlabel plugin in documentation and add required labels [PR #942]
- update documentation of config data types [PR #962]
- Fixed broken link in https://docs.bareos.org/IntroductionAndTutorial/WhatIsBareos.html documentation page


## [20.0.0] - 2020-12-16

### Fixed

- Fix memory corruption in storage daemon [PR #616]
- [Issue #1039]: Can not login in webui [PR #591]
- Improve the behaviour of some console commands [PR #604]
- Several improvements to the LDAP Plugin for filedaemon [PR #595]
- Avoid duplicate actions after submission of an action followed by a page refresh in the webui [PR #594]
- Fix several droplet timeout problems to enable connection retries [PR #561]
- Close connection pool sockets properly (client initiated connections) [PR #562]
- Check user ACL for status configuration [PR #570]
- Allow backup/restore of cephfs mounts [PR #554]
- Fix percona xtrabackup get last_lsn value [PR #551]
- Prevent director daemon crash by adding a nullptr-guard to CreateAttributesRecord() [PR #566]
- Enable testing of deprecated databases (MySQL and SQLite) [PR #573]
- Fix heartbeat thread shutdown race condition [PR #565]
- Fix build problems for several distributions [PR #569]
- Prevent from unique key violations on .bvfs_update [PR #545]
- Fix "make install" and add "make uninstall" when building from source [PR #556]
- [Issue #1257]: Prune volume command may crash the director [PR #546]
- Remove compiler warnings [PR #543]
- python-plugins: improve statp handling in localfileset plugin [PR #530]
- Fix some issues in the tray-monitor configuration [PR #539]
- Avoid a memory double-free in a spool function of the storage daemon [PR #535]
- [Issue #1246]: oVirt plugin fails to restore to local disk with KeyError [PR #534]
- Huge source code cleanup to reduce compiler warnings and increase readability [PR #479]
- Fix bscan handling of restore object data [PR #516]
- Adapt Python Bareos version number according to PEP-440 [PR #521]
- Fixed JobDetails joblog messages order [PR #509]
- Huge source code cleanup of storagedaemon Device backend [PR #498]
- [Issue #1227]: Fix fadvise bug [PR #496]
- Improve webui frontend login dialog [PR #505]
- Fixed the use of a specified since time for the run command [PR #503]
- Fixed that the status scheduler command shows jobs for disabled clients [PR #499]
- Allow Python FD Plugins to set since time [PR #504]
- Fixed a bug when backup a VM by using VM id [PR #415]
- Fixed that droplet is waiting forever if flushing a file fails: [PR #468]
- [Issue #1220]: default drive index not set to 0: [PR #472]
- Fixed a segmentation fault caused by deprecated OpenSSL functions [PR #453]
- [Issue #1211]: Bareos director and bareos fd crash when regexwhere is specified [PR #451]
- Always Incremental: Address early prune problem when vol retentions are too small [PR #423]
- [Issue #1206]: Form field restore job is not populated after client selection [PR #438]
- [Issue #1192]: Authorization key rejected by Storage daemon since upgrading director and storage daemons [PR #431]
- Allow path lengths of a messages resource to be longer that 128 bytes [PR #433]
- [Issue #1190]: Schedules without a client will not be run
- Fixed a bug where hourly schedules fail on the general rule without a day [PR #422]
- [Issue #1175]: bareos crashes with invalid character in ACL [PR #410]
- [Issue #1184]: Using syslog or an invalid message type crashes a daemon [PR #414]
- [Issue #1176]: Upgrade 18.2.5 to 19.2.5 failed while upgrading MySQL database [PR #409]
- [Issue #1174]: Reversed sort order on the restore client selection [PR #411]
- Fixed errors in client only build [PR #413]
- Fixed checksum algorithm regarding byte order [PR #412]
- Avoid a race condition in job name creation [PR #385]
- [Issue #1281]: Director crashes on memory access violation in CloseMemoryPool() and is_regex()

### Added

- Add console command setdevice [PR #686]
- Add support for Fedora 33 [PR #643]
- Add development tools for changelog-handling [PR #617]
- Added documentation and background information for the new python 3 plugin support [PR #599]
- Added the libcloud filedaemon plugin to backup S3 buckets
- Allow terabytes, tebibytes, petabytes, pebibytes, exabytes and exbibytes unit symbols when specifying sizes [PR #600]
- Add technology preview for a REST API to configure and manage Bareos [PR #587]
- Add some configuration warnings to the storagedaemon [PR #583]
- Add python3 support for filedaemon, storagedaemon and director plugins [PR #572]
- Use configurable OpenSSL "Protocol" parameter [PR #575]
- Extend bconsole show command to support JSON output format (API 2) [PR #574]
- Allow multiline quoted strings in the configuration [PR #578]
- Print toolchain details during cmake run [PR #567]
- Build libdroplet as submodule and use systemtest test [PR #560]
- Run systemtests during build, build packages from source root directory [PR #559]
- Add pkglists to documentation for all OS we build for [PR #557]
- Merge VMware plugin code into core subdirectories, add system test [PR #544]
- The VMware plugin now allows to use an optional config file to specify plugin options [PR #544]
- Removed the dependency to pyvmomi from rpm and deb packages [PR #667]
- Add next pool selection to custom job run form in the webui [PR #518]
- In job reports display who triggered a job (scheduler, user, ..) [PR #532]
- Added VirtualFull option for manual defined Jobs [PR #508]
- Add support for Arch and Manjaro Linux [PR #513]
- Document Verify VolumeToCatalog limitation [PR #507]
- Document drawbacks for job intermixing [PR #506]
- Add postgres-backup filedaemon plugin with incremental backups and point in time restore [PR #502]
- Write job- and file-choice to audit log on restore [PR #493]
- Add infrastructure for configuration warnings [PR #492]
- Add logging parameters to gfapi backend [PR #455]
- Add a security policy (`SECURITY.md`) to the project [PR #463]
- Pre-fill restore form element "replace" according to selected restore job in the webui: [PR #456]
- Add documentation for a critical bscan limitation: [PR #460]
- Pre-fill restore location form element according to selected restore job in webui [PR #454]
- Add more logging messages to NDMP interface [PR #448]
- Add start and finish time in the webui-joblist [PR #429]
- Add a database upgrade test [PR #406]
- Add recommendation for PHP 7 to documentation [PR #420]
- Add encryption signature test [PR #404]
- Add percona extra backup [PR #386]
- Add test for client initiated backup [PR #383]

### Changed

- Release-notes / changelog have been moved to CHANGELOG.md [PR #613]
- Bareos now requires at least CMake 3.12 to build
- Updated many localization files [PR #609]
- Display archived jobs on list backups command [PR #581]
- Use a configurable VDI timeout when requesting the MSSQL config [PR #571]
- Improved the formatting of job log entries in the webui [PR #525]
- Cleanup unused images [PR #538]
- Use precise file names for public and private key files to make documentation clear [PR #537]
- Significantly speed-up bareos-dbcopy [PR #524]
- Publish PythonBareos to PyPI.org using Github Actions [PR #514]
- Updated documentation for tests [PR #497]
- Display numeric values of UID and GID in addition to user and group name in webui [PR #488]
- Display file permissions in restore dialog of webui [PR #477]
- Improve warnings on Maximum Block Size of the droplet backend: [PR #476]
- Improve the selection of a specific file version restore in the webui: [PR #471]
- Only allow configuration maximum concurrent jobs of 0 or 1 when using droplet: [PR #465]
- Improve filtering on webui-jobs-bootstrap-table [PR #444]
- Allow systemtests to run on installed packaged [PR #400]
- Allow systemtests to be run in parallel [PR #392]
- Disable systemtests on non compatible systems [PR #387]
- Update ovirt plugin documentation [PR #384]
- Improve ovirt plugin

### Deprecated
- Using MySQL as a catalog backend is now deprecated [PR #564]
- Using Rados as a storage backend is now deprecated [PR #672]
- With full Python 3 support, using Python 2 is now considered deprecated
- Plugin bareos-fd-ldap is deprecated, because it requires Python 2


### Removed
- Remove support for creating MySQL / SQLite catalogs [PR #563] [PR #603]

### Security

- Do not accept cram challenge if own hostname is used [PR #536] [CVE-2020-4042](https://github.com/bareos/bareos/security/advisories/GHSA-vqpj-2vhj-h752)
- [Issue #1210]: Heap overflow in director when running a verify job against a malicious filedaemon [CVE-2020-11061](https://github.com/bareos/bareos/security/advisories/GHSA-mm45-cg35-54j4)


[20.0.0]: https://github.com/bareos/bareos/releases/tag/Release%2F20.0.0
[21.0.0]: https://github.com/bareos/bareos/releases/tag/Release%2F21.0.0
[22.0.0]: https://github.com/bareos/bareos/releases/tag/Release%2F22.0.0
[23.0.0]: https://github.com/bareos/bareos/releases/tag/Release%2F23.0.0
[24.0.0]: https://github.com/bareos/bareos/releases/tag/Release%2F24.0.0
[25.0.0]: https://github.com/bareos/bareos/releases/tag/Release%2F25.0.0
[25.0.1]: https://github.com/bareos/bareos/releases/tag/Release%2F25.0.1
[Issue #579]: https://bugs.bareos.org/view.php?id=579
[Issue #847]: https://bugs.bareos.org/view.php?id=847
[Issue #871]: https://bugs.bareos.org/view.php?id=871
[Issue #971]: https://bugs.bareos.org/view.php?id=971
[Issue #1020]: https://bugs.bareos.org/view.php?id=1020
[Issue #1039]: https://bugs.bareos.org/view.php?id=1039
[Issue #1174]: https://bugs.bareos.org/view.php?id=1174
[Issue #1175]: https://bugs.bareos.org/view.php?id=1175
[Issue #1176]: https://bugs.bareos.org/view.php?id=1176
[Issue #1184]: https://bugs.bareos.org/view.php?id=1184
[Issue #1190]: https://bugs.bareos.org/view.php?id=1190
[Issue #1191]: https://bugs.bareos.org/view.php?id=1191
[Issue #1192]: https://bugs.bareos.org/view.php?id=1192
[Issue #1194]: https://bugs.bareos.org/view.php?id=1194
[Issue #1205]: https://bugs.bareos.org/view.php?id=1205
[Issue #1206]: https://bugs.bareos.org/view.php?id=1206
[Issue #1210]: https://bugs.bareos.org/view.php?id=1210
[Issue #1211]: https://bugs.bareos.org/view.php?id=1211
[Issue #1220]: https://bugs.bareos.org/view.php?id=1220
[Issue #1227]: https://bugs.bareos.org/view.php?id=1227
[Issue #1232]: https://bugs.bareos.org/view.php?id=1232
[Issue #1246]: https://bugs.bareos.org/view.php?id=1246
[Issue #1251]: https://bugs.bareos.org/view.php?id=1251
[Issue #1257]: https://bugs.bareos.org/view.php?id=1257
[Issue #1281]: https://bugs.bareos.org/view.php?id=1281
[Issue #1300]: https://bugs.bareos.org/view.php?id=1300
[Issue #1316]: https://bugs.bareos.org/view.php?id=1316
[Issue #1324]: https://bugs.bareos.org/view.php?id=1324
[Issue #1329]: https://bugs.bareos.org/view.php?id=1329
[Issue #1334]: https://bugs.bareos.org/view.php?id=1334
[Issue #1369]: https://bugs.bareos.org/view.php?id=1369
[Issue #1374]: https://bugs.bareos.org/view.php?id=1374
[Issue #1429]: https://bugs.bareos.org/view.php?id=1429
[Issue #1445]: https://bugs.bareos.org/view.php?id=1445
[Issue #1450]: https://bugs.bareos.org/view.php?id=1450
[Issue #1452]: https://bugs.bareos.org/view.php?id=1452
[Issue #1456]: https://bugs.bareos.org/view.php?id=1456
[Issue #1466]: https://bugs.bareos.org/view.php?id=1466
[Issue #1477]: https://bugs.bareos.org/view.php?id=1477
[Issue #1480]: https://bugs.bareos.org/view.php?id=1480
[Issue #1489]: https://bugs.bareos.org/view.php?id=1489
[Issue #1965]: https://bugs.bareos.org/view.php?id=1965
[PR #383]: https://github.com/bareos/bareos/pull/383
[PR #384]: https://github.com/bareos/bareos/pull/384
[PR #385]: https://github.com/bareos/bareos/pull/385
[PR #386]: https://github.com/bareos/bareos/pull/386
[PR #387]: https://github.com/bareos/bareos/pull/387
[PR #392]: https://github.com/bareos/bareos/pull/392
[PR #400]: https://github.com/bareos/bareos/pull/400
[PR #404]: https://github.com/bareos/bareos/pull/404
[PR #406]: https://github.com/bareos/bareos/pull/406
[PR #409]: https://github.com/bareos/bareos/pull/409
[PR #410]: https://github.com/bareos/bareos/pull/410
[PR #411]: https://github.com/bareos/bareos/pull/411
[PR #412]: https://github.com/bareos/bareos/pull/412
[PR #413]: https://github.com/bareos/bareos/pull/413
[PR #414]: https://github.com/bareos/bareos/pull/414
[PR #415]: https://github.com/bareos/bareos/pull/415
[PR #420]: https://github.com/bareos/bareos/pull/420
[PR #422]: https://github.com/bareos/bareos/pull/422
[PR #423]: https://github.com/bareos/bareos/pull/423
[PR #429]: https://github.com/bareos/bareos/pull/429
[PR #431]: https://github.com/bareos/bareos/pull/431
[PR #433]: https://github.com/bareos/bareos/pull/433
[PR #438]: https://github.com/bareos/bareos/pull/438
[PR #444]: https://github.com/bareos/bareos/pull/444
[PR #448]: https://github.com/bareos/bareos/pull/448
[PR #451]: https://github.com/bareos/bareos/pull/451
[PR #453]: https://github.com/bareos/bareos/pull/453
[PR #454]: https://github.com/bareos/bareos/pull/454
[PR #455]: https://github.com/bareos/bareos/pull/455
[PR #456]: https://github.com/bareos/bareos/pull/456
[PR #460]: https://github.com/bareos/bareos/pull/460
[PR #463]: https://github.com/bareos/bareos/pull/463
[PR #465]: https://github.com/bareos/bareos/pull/465
[PR #468]: https://github.com/bareos/bareos/pull/468
[PR #471]: https://github.com/bareos/bareos/pull/471
[PR #472]: https://github.com/bareos/bareos/pull/472
[PR #476]: https://github.com/bareos/bareos/pull/476
[PR #477]: https://github.com/bareos/bareos/pull/477
[PR #479]: https://github.com/bareos/bareos/pull/479
[PR #488]: https://github.com/bareos/bareos/pull/488
[PR #492]: https://github.com/bareos/bareos/pull/492
[PR #493]: https://github.com/bareos/bareos/pull/493
[PR #496]: https://github.com/bareos/bareos/pull/496
[PR #497]: https://github.com/bareos/bareos/pull/497
[PR #498]: https://github.com/bareos/bareos/pull/498
[PR #499]: https://github.com/bareos/bareos/pull/499
[PR #502]: https://github.com/bareos/bareos/pull/502
[PR #503]: https://github.com/bareos/bareos/pull/503
[PR #504]: https://github.com/bareos/bareos/pull/504
[PR #505]: https://github.com/bareos/bareos/pull/505
[PR #506]: https://github.com/bareos/bareos/pull/506
[PR #507]: https://github.com/bareos/bareos/pull/507
[PR #508]: https://github.com/bareos/bareos/pull/508
[PR #509]: https://github.com/bareos/bareos/pull/509
[PR #513]: https://github.com/bareos/bareos/pull/513
[PR #514]: https://github.com/bareos/bareos/pull/514
[PR #516]: https://github.com/bareos/bareos/pull/516
[PR #518]: https://github.com/bareos/bareos/pull/518
[PR #521]: https://github.com/bareos/bareos/pull/521
[PR #524]: https://github.com/bareos/bareos/pull/524
[PR #525]: https://github.com/bareos/bareos/pull/525
[PR #530]: https://github.com/bareos/bareos/pull/530
[PR #532]: https://github.com/bareos/bareos/pull/532
[PR #534]: https://github.com/bareos/bareos/pull/534
[PR #535]: https://github.com/bareos/bareos/pull/535
[PR #536]: https://github.com/bareos/bareos/pull/536
[PR #537]: https://github.com/bareos/bareos/pull/537
[PR #538]: https://github.com/bareos/bareos/pull/538
[PR #539]: https://github.com/bareos/bareos/pull/539
[PR #543]: https://github.com/bareos/bareos/pull/543
[PR #544]: https://github.com/bareos/bareos/pull/544
[PR #545]: https://github.com/bareos/bareos/pull/545
[PR #546]: https://github.com/bareos/bareos/pull/546
[PR #551]: https://github.com/bareos/bareos/pull/551
[PR #552]: https://github.com/bareos/bareos/pull/552
[PR #554]: https://github.com/bareos/bareos/pull/554
[PR #556]: https://github.com/bareos/bareos/pull/556
[PR #557]: https://github.com/bareos/bareos/pull/557
[PR #559]: https://github.com/bareos/bareos/pull/559
[PR #560]: https://github.com/bareos/bareos/pull/560
[PR #561]: https://github.com/bareos/bareos/pull/561
[PR #562]: https://github.com/bareos/bareos/pull/562
[PR #563]: https://github.com/bareos/bareos/pull/563
[PR #564]: https://github.com/bareos/bareos/pull/564
[PR #565]: https://github.com/bareos/bareos/pull/565
[PR #566]: https://github.com/bareos/bareos/pull/566
[PR #567]: https://github.com/bareos/bareos/pull/567
[PR #569]: https://github.com/bareos/bareos/pull/569
[PR #570]: https://github.com/bareos/bareos/pull/570
[PR #571]: https://github.com/bareos/bareos/pull/571
[PR #572]: https://github.com/bareos/bareos/pull/572
[PR #573]: https://github.com/bareos/bareos/pull/573
[PR #574]: https://github.com/bareos/bareos/pull/574
[PR #575]: https://github.com/bareos/bareos/pull/575
[PR #578]: https://github.com/bareos/bareos/pull/578
[PR #581]: https://github.com/bareos/bareos/pull/581
[PR #583]: https://github.com/bareos/bareos/pull/583
[PR #587]: https://github.com/bareos/bareos/pull/587
[PR #591]: https://github.com/bareos/bareos/pull/591
[PR #594]: https://github.com/bareos/bareos/pull/594
[PR #595]: https://github.com/bareos/bareos/pull/595
[PR #599]: https://github.com/bareos/bareos/pull/599
[PR #600]: https://github.com/bareos/bareos/pull/600
[PR #603]: https://github.com/bareos/bareos/pull/603
[PR #604]: https://github.com/bareos/bareos/pull/604
[PR #609]: https://github.com/bareos/bareos/pull/609
[PR #613]: https://github.com/bareos/bareos/pull/613
[PR #616]: https://github.com/bareos/bareos/pull/616
[PR #617]: https://github.com/bareos/bareos/pull/617
[PR #643]: https://github.com/bareos/bareos/pull/643
[PR #667]: https://github.com/bareos/bareos/pull/667
[PR #672]: https://github.com/bareos/bareos/pull/672
[PR #686]: https://github.com/bareos/bareos/pull/686
[PR #694]: https://github.com/bareos/bareos/pull/694
[PR #698]: https://github.com/bareos/bareos/pull/698
[PR #700]: https://github.com/bareos/bareos/pull/700
[PR #702]: https://github.com/bareos/bareos/pull/702
[PR #707]: https://github.com/bareos/bareos/pull/707
[PR #709]: https://github.com/bareos/bareos/pull/709
[PR #717]: https://github.com/bareos/bareos/pull/717
[PR #723]: https://github.com/bareos/bareos/pull/723
[PR #729]: https://github.com/bareos/bareos/pull/729
[PR #730]: https://github.com/bareos/bareos/pull/730
[PR #731]: https://github.com/bareos/bareos/pull/731
[PR #736]: https://github.com/bareos/bareos/pull/736
[PR #739]: https://github.com/bareos/bareos/pull/739
[PR #740]: https://github.com/bareos/bareos/pull/740
[PR #741]: https://github.com/bareos/bareos/pull/741
[PR #742]: https://github.com/bareos/bareos/pull/742
[PR #744]: https://github.com/bareos/bareos/pull/744
[PR #748]: https://github.com/bareos/bareos/pull/748
[PR #752]: https://github.com/bareos/bareos/pull/752
[PR #757]: https://github.com/bareos/bareos/pull/757
[PR #759]: https://github.com/bareos/bareos/pull/759
[PR #761]: https://github.com/bareos/bareos/pull/761
[PR #762]: https://github.com/bareos/bareos/pull/762
[PR #763]: https://github.com/bareos/bareos/pull/763
[PR #764]: https://github.com/bareos/bareos/pull/764
[PR #765]: https://github.com/bareos/bareos/pull/765
[PR #767]: https://github.com/bareos/bareos/pull/767
[PR #768]: https://github.com/bareos/bareos/pull/768
[PR #769]: https://github.com/bareos/bareos/pull/769
[PR #771]: https://github.com/bareos/bareos/pull/771
[PR #776]: https://github.com/bareos/bareos/pull/776
[PR #777]: https://github.com/bareos/bareos/pull/777
[PR #778]: https://github.com/bareos/bareos/pull/778
[PR #780]: https://github.com/bareos/bareos/pull/780
[PR #782]: https://github.com/bareos/bareos/pull/782
[PR #790]: https://github.com/bareos/bareos/pull/790
[PR #791]: https://github.com/bareos/bareos/pull/791
[PR #792]: https://github.com/bareos/bareos/pull/792
[PR #793]: https://github.com/bareos/bareos/pull/793
[PR #801]: https://github.com/bareos/bareos/pull/801
[PR #802]: https://github.com/bareos/bareos/pull/802
[PR #809]: https://github.com/bareos/bareos/pull/809
[PR #810]: https://github.com/bareos/bareos/pull/810
[PR #819]: https://github.com/bareos/bareos/pull/819
[PR #822]: https://github.com/bareos/bareos/pull/822
[PR #823]: https://github.com/bareos/bareos/pull/823
[PR #824]: https://github.com/bareos/bareos/pull/824
[PR #826]: https://github.com/bareos/bareos/pull/826
[PR #828]: https://github.com/bareos/bareos/pull/828
[PR #829]: https://github.com/bareos/bareos/pull/829
[PR #840]: https://github.com/bareos/bareos/pull/840
[PR #844]: https://github.com/bareos/bareos/pull/844
[PR #850]: https://github.com/bareos/bareos/pull/850
[PR #857]: https://github.com/bareos/bareos/pull/857
[PR #858]: https://github.com/bareos/bareos/pull/858
[PR #860]: https://github.com/bareos/bareos/pull/860
[PR #861]: https://github.com/bareos/bareos/pull/861
[PR #868]: https://github.com/bareos/bareos/pull/868
[PR #869]: https://github.com/bareos/bareos/pull/869
[PR #870]: https://github.com/bareos/bareos/pull/870
[PR #874]: https://github.com/bareos/bareos/pull/874
[PR #880]: https://github.com/bareos/bareos/pull/880
[PR #882]: https://github.com/bareos/bareos/pull/882
[PR #883]: https://github.com/bareos/bareos/pull/883
[PR #892]: https://github.com/bareos/bareos/pull/892
[PR #893]: https://github.com/bareos/bareos/pull/893
[PR #900]: https://github.com/bareos/bareos/pull/900
[PR #903]: https://github.com/bareos/bareos/pull/903
[PR #907]: https://github.com/bareos/bareos/pull/907
[PR #908]: https://github.com/bareos/bareos/pull/908
[PR #910]: https://github.com/bareos/bareos/pull/910
[PR #911]: https://github.com/bareos/bareos/pull/911
[PR #912]: https://github.com/bareos/bareos/pull/912
[PR #913]: https://github.com/bareos/bareos/pull/913
[PR #914]: https://github.com/bareos/bareos/pull/914
[PR #916]: https://github.com/bareos/bareos/pull/916
[PR #918]: https://github.com/bareos/bareos/pull/918
[PR #919]: https://github.com/bareos/bareos/pull/919
[PR #920]: https://github.com/bareos/bareos/pull/920
[PR #921]: https://github.com/bareos/bareos/pull/921
[PR #923]: https://github.com/bareos/bareos/pull/923
[PR #924]: https://github.com/bareos/bareos/pull/924
[PR #926]: https://github.com/bareos/bareos/pull/926
[PR #927]: https://github.com/bareos/bareos/pull/927
[PR #928]: https://github.com/bareos/bareos/pull/928
[PR #935]: https://github.com/bareos/bareos/pull/935
[PR #936]: https://github.com/bareos/bareos/pull/936
[PR #937]: https://github.com/bareos/bareos/pull/937
[PR #938]: https://github.com/bareos/bareos/pull/938
[PR #942]: https://github.com/bareos/bareos/pull/942
[PR #943]: https://github.com/bareos/bareos/pull/943
[PR #944]: https://github.com/bareos/bareos/pull/944
[PR #948]: https://github.com/bareos/bareos/pull/948
[PR #949]: https://github.com/bareos/bareos/pull/949
[PR #950]: https://github.com/bareos/bareos/pull/950
[PR #951]: https://github.com/bareos/bareos/pull/951
[PR #952]: https://github.com/bareos/bareos/pull/952
[PR #956]: https://github.com/bareos/bareos/pull/956
[PR #958]: https://github.com/bareos/bareos/pull/958
[PR #959]: https://github.com/bareos/bareos/pull/959
[PR #961]: https://github.com/bareos/bareos/pull/961
[PR #962]: https://github.com/bareos/bareos/pull/962
[PR #963]: https://github.com/bareos/bareos/pull/963
[PR #965]: https://github.com/bareos/bareos/pull/965
[PR #966]: https://github.com/bareos/bareos/pull/966
[PR #967]: https://github.com/bareos/bareos/pull/967
[PR #969]: https://github.com/bareos/bareos/pull/969
[PR #972]: https://github.com/bareos/bareos/pull/972
[PR #973]: https://github.com/bareos/bareos/pull/973
[PR #981]: https://github.com/bareos/bareos/pull/981
[PR #982]: https://github.com/bareos/bareos/pull/982
[PR #985]: https://github.com/bareos/bareos/pull/985
[PR #989]: https://github.com/bareos/bareos/pull/989
[PR #992]: https://github.com/bareos/bareos/pull/992
[PR #995]: https://github.com/bareos/bareos/pull/995
[PR #996]: https://github.com/bareos/bareos/pull/996
[PR #997]: https://github.com/bareos/bareos/pull/997
[PR #998]: https://github.com/bareos/bareos/pull/998
[PR #999]: https://github.com/bareos/bareos/pull/999
[PR #1007]: https://github.com/bareos/bareos/pull/1007
[PR #1008]: https://github.com/bareos/bareos/pull/1008
[PR #1010]: https://github.com/bareos/bareos/pull/1010
[PR #1011]: https://github.com/bareos/bareos/pull/1011
[PR #1013]: https://github.com/bareos/bareos/pull/1013
[PR #1015]: https://github.com/bareos/bareos/pull/1015
[PR #1016]: https://github.com/bareos/bareos/pull/1016
[PR #1017]: https://github.com/bareos/bareos/pull/1017
[PR #1018]: https://github.com/bareos/bareos/pull/1018
[PR #1019]: https://github.com/bareos/bareos/pull/1019
[PR #1020]: https://github.com/bareos/bareos/pull/1020
[PR #1021]: https://github.com/bareos/bareos/pull/1021
[PR #1022]: https://github.com/bareos/bareos/pull/1022
[PR #1023]: https://github.com/bareos/bareos/pull/1023
[PR #1024]: https://github.com/bareos/bareos/pull/1024
[PR #1025]: https://github.com/bareos/bareos/pull/1025
[PR #1026]: https://github.com/bareos/bareos/pull/1026
[PR #1027]: https://github.com/bareos/bareos/pull/1027
[PR #1028]: https://github.com/bareos/bareos/pull/1028
[PR #1029]: https://github.com/bareos/bareos/pull/1029
[PR #1031]: https://github.com/bareos/bareos/pull/1031
[PR #1033]: https://github.com/bareos/bareos/pull/1033
[PR #1035]: https://github.com/bareos/bareos/pull/1035
[PR #1037]: https://github.com/bareos/bareos/pull/1037
[PR #1038]: https://github.com/bareos/bareos/pull/1038
[PR #1040]: https://github.com/bareos/bareos/pull/1040
[PR #1041]: https://github.com/bareos/bareos/pull/1041
[PR #1042]: https://github.com/bareos/bareos/pull/1042
[PR #1043]: https://github.com/bareos/bareos/pull/1043
[PR #1046]: https://github.com/bareos/bareos/pull/1046
[PR #1047]: https://github.com/bareos/bareos/pull/1047
[PR #1048]: https://github.com/bareos/bareos/pull/1048
[PR #1050]: https://github.com/bareos/bareos/pull/1050
[PR #1051]: https://github.com/bareos/bareos/pull/1051
[PR #1054]: https://github.com/bareos/bareos/pull/1054
[PR #1056]: https://github.com/bareos/bareos/pull/1056
[PR #1057]: https://github.com/bareos/bareos/pull/1057
[PR #1059]: https://github.com/bareos/bareos/pull/1059
[PR #1060]: https://github.com/bareos/bareos/pull/1060
[PR #1062]: https://github.com/bareos/bareos/pull/1062
[PR #1065]: https://github.com/bareos/bareos/pull/1065
[PR #1067]: https://github.com/bareos/bareos/pull/1067
[PR #1070]: https://github.com/bareos/bareos/pull/1070
[PR #1072]: https://github.com/bareos/bareos/pull/1072
[PR #1074]: https://github.com/bareos/bareos/pull/1074
[PR #1079]: https://github.com/bareos/bareos/pull/1079
[PR #1081]: https://github.com/bareos/bareos/pull/1081
[PR #1082]: https://github.com/bareos/bareos/pull/1082
[PR #1083]: https://github.com/bareos/bareos/pull/1083
[PR #1087]: https://github.com/bareos/bareos/pull/1087
[PR #1088]: https://github.com/bareos/bareos/pull/1088
[PR #1089]: https://github.com/bareos/bareos/pull/1089
[PR #1092]: https://github.com/bareos/bareos/pull/1092
[PR #1093]: https://github.com/bareos/bareos/pull/1093
[PR #1097]: https://github.com/bareos/bareos/pull/1097
[PR #1098]: https://github.com/bareos/bareos/pull/1098
[PR #1099]: https://github.com/bareos/bareos/pull/1099
[PR #1106]: https://github.com/bareos/bareos/pull/1106
[PR #1109]: https://github.com/bareos/bareos/pull/1109
[PR #1110]: https://github.com/bareos/bareos/pull/1110
[PR #1115]: https://github.com/bareos/bareos/pull/1115
[PR #1120]: https://github.com/bareos/bareos/pull/1120
[PR #1122]: https://github.com/bareos/bareos/pull/1122
[PR #1126]: https://github.com/bareos/bareos/pull/1126
[PR #1127]: https://github.com/bareos/bareos/pull/1127
[PR #1130]: https://github.com/bareos/bareos/pull/1130
[PR #1131]: https://github.com/bareos/bareos/pull/1131
[PR #1134]: https://github.com/bareos/bareos/pull/1134
[PR #1135]: https://github.com/bareos/bareos/pull/1135
[PR #1138]: https://github.com/bareos/bareos/pull/1138
[PR #1139]: https://github.com/bareos/bareos/pull/1139
[PR #1140]: https://github.com/bareos/bareos/pull/1140
[PR #1147]: https://github.com/bareos/bareos/pull/1147
[PR #1149]: https://github.com/bareos/bareos/pull/1149
[PR #1151]: https://github.com/bareos/bareos/pull/1151
[PR #1152]: https://github.com/bareos/bareos/pull/1152
[PR #1153]: https://github.com/bareos/bareos/pull/1153
[PR #1154]: https://github.com/bareos/bareos/pull/1154
[PR #1155]: https://github.com/bareos/bareos/pull/1155
[PR #1159]: https://github.com/bareos/bareos/pull/1159
[PR #1161]: https://github.com/bareos/bareos/pull/1161
[PR #1162]: https://github.com/bareos/bareos/pull/1162
[PR #1163]: https://github.com/bareos/bareos/pull/1163
[PR #1169]: https://github.com/bareos/bareos/pull/1169
[PR #1170]: https://github.com/bareos/bareos/pull/1170
[PR #1171]: https://github.com/bareos/bareos/pull/1171
[PR #1172]: https://github.com/bareos/bareos/pull/1172
[PR #1176]: https://github.com/bareos/bareos/pull/1176
[PR #1177]: https://github.com/bareos/bareos/pull/1177
[PR #1178]: https://github.com/bareos/bareos/pull/1178
[PR #1179]: https://github.com/bareos/bareos/pull/1179
[PR #1180]: https://github.com/bareos/bareos/pull/1180
[PR #1181]: https://github.com/bareos/bareos/pull/1181
[PR #1183]: https://github.com/bareos/bareos/pull/1183
[PR #1185]: https://github.com/bareos/bareos/pull/1185
[PR #1186]: https://github.com/bareos/bareos/pull/1186
[PR #1187]: https://github.com/bareos/bareos/pull/1187
[PR #1188]: https://github.com/bareos/bareos/pull/1188
[PR #1189]: https://github.com/bareos/bareos/pull/1189
[PR #1193]: https://github.com/bareos/bareos/pull/1193
[PR #1197]: https://github.com/bareos/bareos/pull/1197
[PR #1198]: https://github.com/bareos/bareos/pull/1198
[PR #1200]: https://github.com/bareos/bareos/pull/1200
[PR #1202]: https://github.com/bareos/bareos/pull/1202
[PR #1204]: https://github.com/bareos/bareos/pull/1204
[PR #1205]: https://github.com/bareos/bareos/pull/1205
[PR #1206]: https://github.com/bareos/bareos/pull/1206
[PR #1209]: https://github.com/bareos/bareos/pull/1209
[PR #1210]: https://github.com/bareos/bareos/pull/1210
[PR #1211]: https://github.com/bareos/bareos/pull/1211
[PR #1213]: https://github.com/bareos/bareos/pull/1213
[PR #1215]: https://github.com/bareos/bareos/pull/1215
[PR #1216]: https://github.com/bareos/bareos/pull/1216
[PR #1217]: https://github.com/bareos/bareos/pull/1217
[PR #1218]: https://github.com/bareos/bareos/pull/1218
[PR #1219]: https://github.com/bareos/bareos/pull/1219
[PR #1221]: https://github.com/bareos/bareos/pull/1221
[PR #1225]: https://github.com/bareos/bareos/pull/1225
[PR #1226]: https://github.com/bareos/bareos/pull/1226
[PR #1227]: https://github.com/bareos/bareos/pull/1227
[PR #1228]: https://github.com/bareos/bareos/pull/1228
[PR #1229]: https://github.com/bareos/bareos/pull/1229
[PR #1230]: https://github.com/bareos/bareos/pull/1230
[PR #1231]: https://github.com/bareos/bareos/pull/1231
[PR #1233]: https://github.com/bareos/bareos/pull/1233
[PR #1234]: https://github.com/bareos/bareos/pull/1234
[PR #1235]: https://github.com/bareos/bareos/pull/1235
[PR #1236]: https://github.com/bareos/bareos/pull/1236
[PR #1237]: https://github.com/bareos/bareos/pull/1237
[PR #1238]: https://github.com/bareos/bareos/pull/1238
[PR #1242]: https://github.com/bareos/bareos/pull/1242
[PR #1243]: https://github.com/bareos/bareos/pull/1243
[PR #1244]: https://github.com/bareos/bareos/pull/1244
[PR #1247]: https://github.com/bareos/bareos/pull/1247
[PR #1248]: https://github.com/bareos/bareos/pull/1248
[PR #1249]: https://github.com/bareos/bareos/pull/1249
[PR #1250]: https://github.com/bareos/bareos/pull/1250
[PR #1251]: https://github.com/bareos/bareos/pull/1251
[PR #1253]: https://github.com/bareos/bareos/pull/1253
[PR #1254]: https://github.com/bareos/bareos/pull/1254
[PR #1255]: https://github.com/bareos/bareos/pull/1255
[PR #1260]: https://github.com/bareos/bareos/pull/1260
[PR #1261]: https://github.com/bareos/bareos/pull/1261
[PR #1262]: https://github.com/bareos/bareos/pull/1262
[PR #1265]: https://github.com/bareos/bareos/pull/1265
[PR #1266]: https://github.com/bareos/bareos/pull/1266
[PR #1267]: https://github.com/bareos/bareos/pull/1267
[PR #1268]: https://github.com/bareos/bareos/pull/1268
[PR #1270]: https://github.com/bareos/bareos/pull/1270
[PR #1271]: https://github.com/bareos/bareos/pull/1271
[PR #1272]: https://github.com/bareos/bareos/pull/1272
[PR #1273]: https://github.com/bareos/bareos/pull/1273
[PR #1275]: https://github.com/bareos/bareos/pull/1275
[PR #1276]: https://github.com/bareos/bareos/pull/1276
[PR #1277]: https://github.com/bareos/bareos/pull/1277
[PR #1278]: https://github.com/bareos/bareos/pull/1278
[PR #1279]: https://github.com/bareos/bareos/pull/1279
[PR #1280]: https://github.com/bareos/bareos/pull/1280
[PR #1281]: https://github.com/bareos/bareos/pull/1281
[PR #1282]: https://github.com/bareos/bareos/pull/1282
[PR #1283]: https://github.com/bareos/bareos/pull/1283
[PR #1284]: https://github.com/bareos/bareos/pull/1284
[PR #1285]: https://github.com/bareos/bareos/pull/1285
[PR #1287]: https://github.com/bareos/bareos/pull/1287
[PR #1288]: https://github.com/bareos/bareos/pull/1288
[PR #1295]: https://github.com/bareos/bareos/pull/1295
[PR #1296]: https://github.com/bareos/bareos/pull/1296
[PR #1297]: https://github.com/bareos/bareos/pull/1297
[PR #1298]: https://github.com/bareos/bareos/pull/1298
[PR #1300]: https://github.com/bareos/bareos/pull/1300
[PR #1303]: https://github.com/bareos/bareos/pull/1303
[PR #1305]: https://github.com/bareos/bareos/pull/1305
[PR #1306]: https://github.com/bareos/bareos/pull/1306
[PR #1307]: https://github.com/bareos/bareos/pull/1307
[PR #1308]: https://github.com/bareos/bareos/pull/1308
[PR #1309]: https://github.com/bareos/bareos/pull/1309
[PR #1312]: https://github.com/bareos/bareos/pull/1312
[PR #1313]: https://github.com/bareos/bareos/pull/1313
[PR #1314]: https://github.com/bareos/bareos/pull/1314
[PR #1315]: https://github.com/bareos/bareos/pull/1315
[PR #1316]: https://github.com/bareos/bareos/pull/1316
[PR #1317]: https://github.com/bareos/bareos/pull/1317
[PR #1318]: https://github.com/bareos/bareos/pull/1318
[PR #1319]: https://github.com/bareos/bareos/pull/1319
[PR #1320]: https://github.com/bareos/bareos/pull/1320
[PR #1324]: https://github.com/bareos/bareos/pull/1324
[PR #1325]: https://github.com/bareos/bareos/pull/1325
[PR #1326]: https://github.com/bareos/bareos/pull/1326
[PR #1327]: https://github.com/bareos/bareos/pull/1327
[PR #1329]: https://github.com/bareos/bareos/pull/1329
[PR #1330]: https://github.com/bareos/bareos/pull/1330
[PR #1331]: https://github.com/bareos/bareos/pull/1331
[PR #1332]: https://github.com/bareos/bareos/pull/1332
[PR #1333]: https://github.com/bareos/bareos/pull/1333
[PR #1334]: https://github.com/bareos/bareos/pull/1334
[PR #1335]: https://github.com/bareos/bareos/pull/1335
[PR #1336]: https://github.com/bareos/bareos/pull/1336
[PR #1337]: https://github.com/bareos/bareos/pull/1337
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
[PR #1538]: https://github.com/bareos/bareos/pull/1538
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
[PR #1581]: https://github.com/bareos/bareos/pull/1581
[PR #1582]: https://github.com/bareos/bareos/pull/1582
[PR #1583]: https://github.com/bareos/bareos/pull/1583
[PR #1587]: https://github.com/bareos/bareos/pull/1587
[PR #1588]: https://github.com/bareos/bareos/pull/1588
[PR #1589]: https://github.com/bareos/bareos/pull/1589
[PR #1592]: https://github.com/bareos/bareos/pull/1592
[PR #1593]: https://github.com/bareos/bareos/pull/1593
[PR #1595]: https://github.com/bareos/bareos/pull/1595
[PR #1598]: https://github.com/bareos/bareos/pull/1598
[PR #1599]: https://github.com/bareos/bareos/pull/1599
[PR #1601]: https://github.com/bareos/bareos/pull/1601
[PR #1602]: https://github.com/bareos/bareos/pull/1602
[PR #1604]: https://github.com/bareos/bareos/pull/1604
[PR #1605]: https://github.com/bareos/bareos/pull/1605
[PR #1606]: https://github.com/bareos/bareos/pull/1606
[PR #1607]: https://github.com/bareos/bareos/pull/1607
[PR #1608]: https://github.com/bareos/bareos/pull/1608
[PR #1609]: https://github.com/bareos/bareos/pull/1609
[PR #1611]: https://github.com/bareos/bareos/pull/1611
[PR #1612]: https://github.com/bareos/bareos/pull/1612
[PR #1613]: https://github.com/bareos/bareos/pull/1613
[PR #1614]: https://github.com/bareos/bareos/pull/1614
[PR #1615]: https://github.com/bareos/bareos/pull/1615
[PR #1617]: https://github.com/bareos/bareos/pull/1617
[PR #1618]: https://github.com/bareos/bareos/pull/1618
[PR #1619]: https://github.com/bareos/bareos/pull/1619
[PR #1620]: https://github.com/bareos/bareos/pull/1620
[PR #1623]: https://github.com/bareos/bareos/pull/1623
[PR #1626]: https://github.com/bareos/bareos/pull/1626
[PR #1628]: https://github.com/bareos/bareos/pull/1628
[PR #1629]: https://github.com/bareos/bareos/pull/1629
[PR #1630]: https://github.com/bareos/bareos/pull/1630
[PR #1631]: https://github.com/bareos/bareos/pull/1631
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
[PR #1697]: https://github.com/bareos/bareos/pull/1697
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
[PR #1790]: https://github.com/bareos/bareos/pull/1790
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
[PR #1862]: https://github.com/bareos/bareos/pull/1862
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
[PR #1893]: https://github.com/bareos/bareos/pull/1893
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
[PR #1993]: https://github.com/bareos/bareos/pull/1993
[PR #1994]: https://github.com/bareos/bareos/pull/1994
[PR #1996]: https://github.com/bareos/bareos/pull/1996
[PR #1998]: https://github.com/bareos/bareos/pull/1998
[PR #2004]: https://github.com/bareos/bareos/pull/2004
[PR #2006]: https://github.com/bareos/bareos/pull/2006
[PR #2008]: https://github.com/bareos/bareos/pull/2008
[PR #2018]: https://github.com/bareos/bareos/pull/2018
[PR #2019]: https://github.com/bareos/bareos/pull/2019
[PR #2022]: https://github.com/bareos/bareos/pull/2022
[PR #2027]: https://github.com/bareos/bareos/pull/2027
[PR #2029]: https://github.com/bareos/bareos/pull/2029
[PR #2031]: https://github.com/bareos/bareos/pull/2031
[PR #2035]: https://github.com/bareos/bareos/pull/2035
[PR #2037]: https://github.com/bareos/bareos/pull/2037
[PR #2039]: https://github.com/bareos/bareos/pull/2039
[PR #2040]: https://github.com/bareos/bareos/pull/2040
[PR #2056]: https://github.com/bareos/bareos/pull/2056
[PR #2059]: https://github.com/bareos/bareos/pull/2059
[PR #2064]: https://github.com/bareos/bareos/pull/2064
[PR #2065]: https://github.com/bareos/bareos/pull/2065
[PR #2066]: https://github.com/bareos/bareos/pull/2066
[PR #2067]: https://github.com/bareos/bareos/pull/2067
[PR #2068]: https://github.com/bareos/bareos/pull/2068
[PR #2069]: https://github.com/bareos/bareos/pull/2069
[PR #2070]: https://github.com/bareos/bareos/pull/2070
[PR #2071]: https://github.com/bareos/bareos/pull/2071
[PR #2072]: https://github.com/bareos/bareos/pull/2072
[PR #2076]: https://github.com/bareos/bareos/pull/2076
[PR #2078]: https://github.com/bareos/bareos/pull/2078
[PR #2079]: https://github.com/bareos/bareos/pull/2079
[PR #2086]: https://github.com/bareos/bareos/pull/2086
[PR #2090]: https://github.com/bareos/bareos/pull/2090
[PR #2097]: https://github.com/bareos/bareos/pull/2097
[PR #2102]: https://github.com/bareos/bareos/pull/2102
[PR #2104]: https://github.com/bareos/bareos/pull/2104
[PR #2105]: https://github.com/bareos/bareos/pull/2105
[PR #2106]: https://github.com/bareos/bareos/pull/2106
[PR #2109]: https://github.com/bareos/bareos/pull/2109
[PR #2111]: https://github.com/bareos/bareos/pull/2111
[PR #2116]: https://github.com/bareos/bareos/pull/2116
[PR #2117]: https://github.com/bareos/bareos/pull/2117
[PR #2120]: https://github.com/bareos/bareos/pull/2120
[PR #2121]: https://github.com/bareos/bareos/pull/2121
[PR #2122]: https://github.com/bareos/bareos/pull/2122
[PR #2128]: https://github.com/bareos/bareos/pull/2128
[PR #2130]: https://github.com/bareos/bareos/pull/2130
[PR #2132]: https://github.com/bareos/bareos/pull/2132
[PR #2141]: https://github.com/bareos/bareos/pull/2141
[PR #2144]: https://github.com/bareos/bareos/pull/2144
[PR #2147]: https://github.com/bareos/bareos/pull/2147
[PR #2149]: https://github.com/bareos/bareos/pull/2149
[PR #2152]: https://github.com/bareos/bareos/pull/2152
[PR #2153]: https://github.com/bareos/bareos/pull/2153
[PR #2159]: https://github.com/bareos/bareos/pull/2159
[PR #2169]: https://github.com/bareos/bareos/pull/2169
[PR #2175]: https://github.com/bareos/bareos/pull/2175
[PR #2176]: https://github.com/bareos/bareos/pull/2176
[PR #2180]: https://github.com/bareos/bareos/pull/2180
[PR #2187]: https://github.com/bareos/bareos/pull/2187
[PR #2188]: https://github.com/bareos/bareos/pull/2188
[PR #2191]: https://github.com/bareos/bareos/pull/2191
[PR #2192]: https://github.com/bareos/bareos/pull/2192
[PR #2194]: https://github.com/bareos/bareos/pull/2194
[PR #2198]: https://github.com/bareos/bareos/pull/2198
[PR #2201]: https://github.com/bareos/bareos/pull/2201
[PR #2202]: https://github.com/bareos/bareos/pull/2202
[PR #2205]: https://github.com/bareos/bareos/pull/2205
[PR #2207]: https://github.com/bareos/bareos/pull/2207
[PR #2211]: https://github.com/bareos/bareos/pull/2211
[PR #2213]: https://github.com/bareos/bareos/pull/2213
[PR #2215]: https://github.com/bareos/bareos/pull/2215
[PR #2217]: https://github.com/bareos/bareos/pull/2217
[PR #2220]: https://github.com/bareos/bareos/pull/2220
[PR #2222]: https://github.com/bareos/bareos/pull/2222
[PR #2225]: https://github.com/bareos/bareos/pull/2225
[PR #2232]: https://github.com/bareos/bareos/pull/2232
[PR #2240]: https://github.com/bareos/bareos/pull/2240
[PR #2241]: https://github.com/bareos/bareos/pull/2241
[PR #2250]: https://github.com/bareos/bareos/pull/2250
[PR #2252]: https://github.com/bareos/bareos/pull/2252
[PR #2256]: https://github.com/bareos/bareos/pull/2256
[PR #2259]: https://github.com/bareos/bareos/pull/2259
[PR #2260]: https://github.com/bareos/bareos/pull/2260
[PR #2263]: https://github.com/bareos/bareos/pull/2263
[PR #2264]: https://github.com/bareos/bareos/pull/2264
[PR #2267]: https://github.com/bareos/bareos/pull/2267
[PR #2268]: https://github.com/bareos/bareos/pull/2268
[PR #2269]: https://github.com/bareos/bareos/pull/2269
[PR #2270]: https://github.com/bareos/bareos/pull/2270
[PR #2272]: https://github.com/bareos/bareos/pull/2272
[PR #2273]: https://github.com/bareos/bareos/pull/2273
[PR #2275]: https://github.com/bareos/bareos/pull/2275
[PR #2278]: https://github.com/bareos/bareos/pull/2278
[PR #2279]: https://github.com/bareos/bareos/pull/2279
[PR #2283]: https://github.com/bareos/bareos/pull/2283
[PR #2284]: https://github.com/bareos/bareos/pull/2284
[PR #2285]: https://github.com/bareos/bareos/pull/2285
[PR #2286]: https://github.com/bareos/bareos/pull/2286
[PR #2287]: https://github.com/bareos/bareos/pull/2287
[PR #2290]: https://github.com/bareos/bareos/pull/2290
[PR #2300]: https://github.com/bareos/bareos/pull/2300
[PR #2303]: https://github.com/bareos/bareos/pull/2303
[PR #2310]: https://github.com/bareos/bareos/pull/2310
[PR #2315]: https://github.com/bareos/bareos/pull/2315
[PR #2321]: https://github.com/bareos/bareos/pull/2321
[PR #2328]: https://github.com/bareos/bareos/pull/2328
[PR #2329]: https://github.com/bareos/bareos/pull/2329
[PR #2331]: https://github.com/bareos/bareos/pull/2331
[PR #2332]: https://github.com/bareos/bareos/pull/2332
[PR #2333]: https://github.com/bareos/bareos/pull/2333
[PR #2335]: https://github.com/bareos/bareos/pull/2335
[PR #2338]: https://github.com/bareos/bareos/pull/2338
[PR #2340]: https://github.com/bareos/bareos/pull/2340
[PR #2341]: https://github.com/bareos/bareos/pull/2341
[PR #2344]: https://github.com/bareos/bareos/pull/2344
[PR #2347]: https://github.com/bareos/bareos/pull/2347
[PR #2350]: https://github.com/bareos/bareos/pull/2350
[PR #2357]: https://github.com/bareos/bareos/pull/2357
[PR #2358]: https://github.com/bareos/bareos/pull/2358
[PR #2359]: https://github.com/bareos/bareos/pull/2359
[PR #2370]: https://github.com/bareos/bareos/pull/2370
[PR #2372]: https://github.com/bareos/bareos/pull/2372
[PR #2373]: https://github.com/bareos/bareos/pull/2373
[PR #2376]: https://github.com/bareos/bareos/pull/2376
[PR #2381]: https://github.com/bareos/bareos/pull/2381
[PR #2385]: https://github.com/bareos/bareos/pull/2385
[PR #2386]: https://github.com/bareos/bareos/pull/2386
[PR #2391]: https://github.com/bareos/bareos/pull/2391
[PR #2392]: https://github.com/bareos/bareos/pull/2392
[PR #2394]: https://github.com/bareos/bareos/pull/2394
[PR #2395]: https://github.com/bareos/bareos/pull/2395
[PR #2399]: https://github.com/bareos/bareos/pull/2399
[PR #2401]: https://github.com/bareos/bareos/pull/2401
[PR #2403]: https://github.com/bareos/bareos/pull/2403
[PR #2404]: https://github.com/bareos/bareos/pull/2404
[PR #2407]: https://github.com/bareos/bareos/pull/2407
[PR #2408]: https://github.com/bareos/bareos/pull/2408
[PR #2412]: https://github.com/bareos/bareos/pull/2412
[PR #2416]: https://github.com/bareos/bareos/pull/2416
[PR #2417]: https://github.com/bareos/bareos/pull/2417
[PR #2418]: https://github.com/bareos/bareos/pull/2418
[PR #2423]: https://github.com/bareos/bareos/pull/2423
[PR #2427]: https://github.com/bareos/bareos/pull/2427
[PR #2428]: https://github.com/bareos/bareos/pull/2428
[PR #2429]: https://github.com/bareos/bareos/pull/2429
[PR #2431]: https://github.com/bareos/bareos/pull/2431
[PR #2432]: https://github.com/bareos/bareos/pull/2432
[PR #2434]: https://github.com/bareos/bareos/pull/2434
[PR #2439]: https://github.com/bareos/bareos/pull/2439
[PR #2442]: https://github.com/bareos/bareos/pull/2442
[PR #2443]: https://github.com/bareos/bareos/pull/2443
[PR #2444]: https://github.com/bareos/bareos/pull/2444
[PR #2445]: https://github.com/bareos/bareos/pull/2445
[PR #2452]: https://github.com/bareos/bareos/pull/2452
[PR #2456]: https://github.com/bareos/bareos/pull/2456
[PR #2471]: https://github.com/bareos/bareos/pull/2471
[PR #2472]: https://github.com/bareos/bareos/pull/2472
[PR #2474]: https://github.com/bareos/bareos/pull/2474
[PR #2475]: https://github.com/bareos/bareos/pull/2475
[PR #2477]: https://github.com/bareos/bareos/pull/2477
[PR #2481]: https://github.com/bareos/bareos/pull/2481
[PR #2502]: https://github.com/bareos/bareos/pull/2502
[PR #2504]: https://github.com/bareos/bareos/pull/2504
[PR #2507]: https://github.com/bareos/bareos/pull/2507
[PR #2512]: https://github.com/bareos/bareos/pull/2512
[PR #2513]: https://github.com/bareos/bareos/pull/2513
[PR #2516]: https://github.com/bareos/bareos/pull/2516
[unreleased]: https://github.com/bareos/bareos/tree/master
