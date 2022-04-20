All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and since Bareos version 20 this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Breaking Changes
- when using PAM Bareos will now check authorization, too. If authorization is not configured, login will fail. See [updated documentation](https://docs.bareos.org/TasksAndConcepts/PAM.html#configuration) on how to proceed [PR #1115].

### Added
- Python plugins: add default module_path to search path [PR #1038]
- dird: extend the list command to be able to query volumes and pools by ID [PR #1041]
- docs: Add chapter for mariabackup db plugin [PR #1016]
- ndmp: introduce workaround for isilon 9.1.0.0 'Invalid nlist.tape_offset -1' error [PR #1043]
- packaging: installation and upgrade will check for the presence of :file:`.enable-cap_sys_rawio` in your bareos config dir and will configure the required cap_sys_rawio capabilities [PR #1057]
- systemtests: wait for mariadb shutdown [PR #1048]
- tests: simplify test coverage analysis [PR #1010]
- webui: analytics module: show stored data per jobname in treemap [PR #1082]
- webui: add pool column to volume lists [PR #1120]
- dird: console: add `jobtype` parameter to `(l)list jobs` [PR #1149]
- contrib: add support for building and packaging [PR #768]:
  - bareos-contrib-director-python-plugins
  - bareos-contrib-filedaemon-python-plugins
  - bareos-contrib-tools
- systemtest py3plug-fd-contrib-mysql_dump [PR #768]
- systemtest py*plug-fd-contrib-bareos_tasks_mysql [PR #768]
- webui: introduce rerun of multiple jobs at once [PR #1109]

### Fixed
- NDMP_BAREOS: support autoxflate plugin [PR #1013]
- debian: Let dbconfig create the Bareos catalog also with LC_COLLATE='C' and LC_CTYPE='C'. The create_bareos_database script did always do so. Requires dbconfig >= 2.0.21 [PR #1031]
- fix wrong `packages_dir` in restapi workflow, so restapi packages will be released to PyPI [PR #1033]
- core cats: Add IF EXISTS in drop table statements fix for bug #1409 (Allow usage of ExitOnFatal) [PR #1035]
- sql_get.cc: fix error logging in GetJobRecord() for jobname [PR #1042]
- webui: fix empty job timeline issue if date.timezone is not set in php.ini [PR #1051]
- Fix for wrong update message when updating all volumes from all pools with no existing volumes [PR #1015]
- Fix context confusion in Director's Python plugins [PR #1047]
- Fix several cases of undefined behaviour, memory corruption and memory leaks [PR #1060]
- webui: fix undefined array key warning [PR #1098]
- webui: fix deprecated notice required param follows optional param [PR #1097]
- webui: fix uncaught TypeError if node.data is null [PR #1087]
- core cats: Add DROP VIEWS instruction in drop_bareos_table script [PR #1092]
- Don't keep volume open after acquiring a read-storage failed in migrate/copy/virtual full [PR #1106]
- webui: show DIR message if ACL prevents a job rerun [PR #1110]
- webui: fix restore file tree rendering [PR #1127]
- dir: fix crash when there are no jobs to consolidate [PR #1131]
- webui: get volume and pool params from query instead of route [PR #1139]
- FreeBSD packages: add missing ddl/update 2171_2192 and 2192_2210 files [PR #1147]
- Fix director connects to client while `Connection From Director To Client` is disabled. [PR #1099]

### Changed
- contrib: rename Python modules to satisfy PEP8 [PR #768]
- contrib: adapt to Python interface of Bareos >= 20 [PR #768]
- Qmsg: in case of syslog logging use adapted log priority instead of always LOG_ERR [PR #1134]
- webui: remove an unnecessary .bvfs_get_jobids and buildSubtree() call [PR #1050]
- git: set merge strategy for CHANGELOG.md to union [PR #1062]
- webui: add timeline chart by jobs [PR #1059]
- stored: enable labeling of tapes in drives even if `autoselect=no` [PR #1021]
- dir, stored: start statistics threads only if needed [PR #1040]
- gitignore: cleanup .gitignore files [PR #1067]
- webui: update jstree from v3.3.8 to v3.3.12 [PR #1088]
- webui: update jstree-grid plugin [PR #1089]
- Consolidation now purges candidate jobs with no files instead of ignoring them [PR #1056]
- Virtual Full will now terminate if one if the input jobs had its files pruned [PR #1070]
- webui: new login screen background and adapted logo to support Ukraine  [PR #1122]
- console: multicolumn output: fill columns first [PR #1072]
- cats: include only jobtypes in `list jobtotals` that write data to volumes [PR #1135]
- jstreegrid: remove handling of IE < 8 using navigator interface to avoid warnings in chrome [PR #1140]
- `bvfs_update` now uses `unordered_map` instead of `htable` for the pathid cache [PR #1138]

### Deprecated

### Removed

### Security
- webui: update jquery from v3.2.0 to v3.6.0 [PR #1083]
- dird: check authorization on PAM login [PR #1115] [CVE-2022-24755](https://github.com/bareos/bareos/security/advisories/GHSA-4979-8ffj-4q26)
- dird: fix memory leak on failed PAM login [PR #1115] [CVE-2022-24756](https://github.com/bareos/bareos/security/advisories/GHSA-jh55-4wgw-xc9j)
- webui: update moment.js to version 2.29.2 [PR #1155] [CVE-2022-24785](https://github.com/advisories/GHSA-8hfj-j24r-96c4)
  - webui is NOT affected
  - webui does NOT use the npm server
  - webui does NOT use a user-provided locale string to directly switch moment locale

### Documentation

- cleanup update section [PR #1054]
- clarifies MySQL catalog migration process [PR #1054]
- split `Howtos.rst` file into one file per section [PR #1054]
- split the very long `Plugins.rst` file into one file per Bareos plugin [PR #1046]
- rework SD plugin scsicrypto linux sg_io ioctl subsection for cap_sys_rawio [PR #1057]
- improve action Python plugin documentation, by removing File in Fileset example [PR #1079]
- improve Mysql - PostgreSQL howto [PR #1093] fixing [BUG #1429]
- clarifies Sphinx bareos-extension parallel_read_safe status to False [PR #1037]
- fix incorrect link in contrib PythonFdPlugin [BUG #1450] [PR #1065]

[PR #768]: https://github.com/bareos/bareos/pull/768
[PR #1010]: https://github.com/bareos/bareos/pull/1010
[PR #1013]: https://github.com/bareos/bareos/pull/1013
[PR #1015]: https://github.com/bareos/bareos/pull/1015
[PR #1016]: https://github.com/bareos/bareos/pull/1016
[PR #1021]: https://github.com/bareos/bareos/pull/1021
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
[PR #1079]: https://github.com/bareos/bareos/pull/1079
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
[PR #1127]: https://github.com/bareos/bareos/pull/1127
[PR #1131]: https://github.com/bareos/bareos/pull/1131
[PR #1134]: https://github.com/bareos/bareos/pull/1134
[PR #1135]: https://github.com/bareos/bareos/pull/1135
[PR #1138]: https://github.com/bareos/bareos/pull/1138
[PR #1139]: https://github.com/bareos/bareos/pull/1139
[PR #1140]: https://github.com/bareos/bareos/pull/1140
[PR #1147]: https://github.com/bareos/bareos/pull/1147
[PR #1149]: https://github.com/bareos/bareos/pull/1149
[PR #1155]: https://github.com/bareos/bareos/pull/1155
[unreleased]: https://github.com/bareos/bareos/tree/master
