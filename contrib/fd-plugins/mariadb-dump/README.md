# bareos-fd-mariadb-dump Plugin

This plugin makes a backup of each database found in a MariaDB cluster into a single file.
For restore select the needed database file, found in `@mariadbbackup@` in the restore tree.

see [Documentation mariadb-dump section](https://docs.bareos.org/Appendix/Howtos.html#backup-of-mariadb-databases-using-the-contrib-python-mariadb-plugin) for full instructions and details.