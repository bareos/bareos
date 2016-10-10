Format: 1.0
Source: bareos
Binary: bareos, bareos-bat, bareos-bconsole, bareos-client, bareos-common, bareos-database-common, bareos-database-postgresql, bareos-database-mysql, bareos-database-sqlite3, bareos-database-tools, bareos-devel, bareos-director, bareos-director-python-plugin, bareos-filedaemon, bareos-filedaemon-python-plugin, bareos-storage, bareos-storage-fifo, bareos-storage-tape, bareos-storage-python-plugin, bareos-tools, bareos-traymonitor
Architecture: any
Version: 16.4.1
Maintainer: Joerg Steffens <joerg.steffens@bareos.com>
Homepage: http://www.bareos.org/
Standards-Version: 3.9.4
Build-Depends: acl-dev, autotools-dev, bc, chrpath, debhelper (>= 7.0.50~), dpkg-dev (>= 1.13.19), git-core, libacl1-dev, libcap-dev, libcmocka-dev (>= 1.0.1), libfastlz-dev, libjansson-dev, liblzo2-dev, libqt4-dev, libreadline-dev, libssl-dev, libwrap0-dev, libx11-dev, libsqlite3-dev, libmysqlclient-dev, libpq-dev, logrotate, lsb-release, mtx, ncurses-dev, openssl, pkg-config, po-debconf (>= 0.8.2), python-dev, zlib1g-dev
# optional (via OBS project config):
#   0%{?debian_version} >= 800 || 0%{?ubuntu_version} >= 1404
#     libcephfs-dev
#     librados-dev
#   0%{?ubuntu_version} >= 1604
#     libradosstriper-dev
#   0%{?debian_version} >= 800 || 0%{?ubuntu_version} >= 1504
#     glusterfs-common
#     dh-systemd
Build-Conflicts: python2.2-dev, python2.3, python2.4, qt3-dev-tools
DEBTRANSFORM-RELEASE: 1
Files:
