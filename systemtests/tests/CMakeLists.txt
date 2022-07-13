#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2021-2022 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

add_subdirectory(acl)
add_subdirectory(always-incremental-consolidate)
add_subdirectory(autochanger)
add_subdirectory(bareos)
add_subdirectory(bareos-acl)
add_subdirectory(bconsole)
add_subdirectory(bconsole-pam)
add_subdirectory(block-size)
add_subdirectory(bscan-bextract-bls)
add_subdirectory(catalog)
add_subdirectory(chflags)
add_subdirectory(client-initiated)
add_subdirectory(config-dump)
add_subdirectory(config-syntax-crash)
add_subdirectory(copy)
add_subdirectory(copy-archive-job)
add_subdirectory(copy-remote-bscan)
add_subdirectory(dbcopy-mysql-postgresql)
add_subdirectory(deprecation)
add_subdirectory(droplet-s3)
add_subdirectory(encrypt-signature)
add_subdirectory(encrypt-signature-tls-cert)
add_subdirectory(fileset-multiple-blocks)
add_subdirectory(filesets)
add_subdirectory(gfapi-fd)
add_subdirectory(glusterfs-backend)
add_subdirectory(list-backups)
add_subdirectory(messages)
add_subdirectory(multiplied-device)
add_subdirectory(ndmp)
add_subdirectory(notls)
add_subdirectory(passive)
add_subdirectory(py2plug-dir)
add_subdirectory(py2plug-fd-ldap)
add_subdirectory(py2plug-fd-libcloud)
add_subdirectory(py2plug-fd-local-fileset)
add_subdirectory(py2plug-fd-local-fileset-restoreobject)
add_subdirectory(py2plug-fd-mariabackup)
add_subdirectory(py2plug-fd-ovirt)
add_subdirectory(py2plug-fd-percona-xtrabackup)
add_subdirectory(py2plug-fd-postgres)
add_subdirectory(py2plug-fd-vmware)
add_subdirectory(py2plug-sd)
add_subdirectory(python-bareos)
add_subdirectory(python-pam)
add_subdirectory(quota-softquota)
add_subdirectory(reload)
add_subdirectory(restapi)
add_subdirectory(scheduler-backup)
add_subdirectory(sparse-file)
add_subdirectory(spool)
add_subdirectory(truncate-command)
add_subdirectory(upgrade-database)
add_subdirectory(virtualfull)
add_subdirectory(virtualfull-bscan)
add_subdirectory(volume-pruning)
add_subdirectory(xattr)
