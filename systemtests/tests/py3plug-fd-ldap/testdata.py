#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2025 Bareos GmbH & Co. KG
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

from __future__ import division, absolute_import, print_function, unicode_literals
import io
import ldap
import ldap.modlist
import ldif
import sys
import hashlib
from argparse import ArgumentParser
from ldif import LDIFWriter


def _safe_encode(data):
    if isinstance(data, str):
        return data.encode("utf-8")
    return data


def ldap_connect(address, binddn, password):
    conn = ldap.initialize("ldap://%s" % address)

    conn.set_option(ldap.OPT_REFERRALS, 0)
    conn.simple_bind_s(binddn, password)

    return conn


def ldap_create_or_fail(conn, dn, modlist):
    """
    create an object dn with the attributes from modlist dict
    """
    try:
        conn.add_s(dn, ldap.modlist.addModlist(modlist))
    except ldap.ALREADY_EXISTS:
        print("Object '%s' already exists." % dn, file=sys.stderr)
        sys.exit(1)


def action_clean(conn, basedn, testname):
    """
    Clean up all objects in our subtrees (ldap-variant of "rm -rf").

    find the dns ob all objects below our bases and remove them ordered from
    longest to shortest dn, so we remove parent objects later
    """

    for subtree_dn in [
        f"ou=backup-{testname},{basedn}",
        f"ou=restore-{testname},{basedn}",
    ]:
        try:
            for dn in sorted(
                map(
                    lambda x: x[0],
                    conn.search_s(subtree_dn, ldap.SCOPE_SUBTREE, attrsonly=1),
                ),
                key=len,
                reverse=True,
            ):
                conn.delete_s(dn)
        except ldap.NO_SUCH_OBJECT:
            # if the top object doesn't exist, there's nothing to remove
            pass


def action_populate(conn, basedn, testname):
    """Populate our backup data"""
    ldap_create_or_fail(
        conn,
        f"ou=backup-{testname},{basedn}",
        {
            "objectClass": [b"organizationalUnit"],
            "ou": [f"restore-{testname}".encode()],
        },
    )

    ldap_create_or_fail(
        conn,
        f"cn=No JPEG,ou=backup-{testname},{basedn}",
        {
            "objectClass": [b"inetOrgPerson", b"posixAccount", b"shadowAccount"],
            "uid": [b"njpeg"],
            "sn": [b"JPEG"],
            "givenName": [b"No"],
            "cn": [b"No JPEG"],
            "displayName": [b"No JPEG"],
            "uidNumber": [b"1000"],
            "gidNumber": [b"1000"],
            "loginShell": [b"/bin/bash"],
            "homeDirectory": [b"/home/njpeg"],
        },
    )

    photo = open("image-small.jpg", "rb").read()

    ldap_create_or_fail(
        conn,
        f"cn=Small JPEG,ou=backup-{testname},{basedn}",
        {
            "objectClass": [b"inetOrgPerson", b"posixAccount", b"shadowAccount"],
            "uid": [b"sjpeg"],
            "sn": [b"JPEG"],
            "givenName": [b"Small"],
            "cn": [b"Small JPEG"],
            "displayName": [b"Small JPEG"],
            "uidNumber": [b"1001"],
            "gidNumber": [b"1000"],
            "loginShell": [b"/bin/bash"],
            "homeDirectory": [b"/home/sjpeg"],
            "jpegPhoto": photo,
        },
    )

    photo = open("image-medium.jpg", "rb").read()

    ldap_create_or_fail(
        conn,
        f"cn=Medium JPEG,ou=backup-{testname},{basedn}",
        {
            "objectClass": [b"inetOrgPerson", b"posixAccount", b"shadowAccount"],
            "uid": [b"mjpeg"],
            "sn": [b"JPEG"],
            "givenName": [b"Medium"],
            "cn": [b"Medium JPEG"],
            "displayName": [b"Medium JPEG"],
            "uidNumber": [b"1002"],
            "gidNumber": [b"1000"],
            "loginShell": [b"/bin/bash"],
            "homeDirectory": [b"/home/mjpeg"],
            "jpegPhoto": photo,
        },
    )

    photo = open("image-large.jpg", "rb").read()

    ldap_create_or_fail(
        conn,
        f"cn=Large JPEG,ou=backup-{testname},{basedn}",
        {
            "objectClass": [b"inetOrgPerson", b"posixAccount", b"shadowAccount"],
            "uid": [b"ljpeg"],
            "sn": [b"JPEG"],
            "givenName": [b"Large"],
            "cn": [b"Large JPEG"],
            "displayName": [b"Large JPEG"],
            "uidNumber": [b"1003"],
            "gidNumber": [b"1000"],
            "loginShell": [b"/bin/bash"],
            "homeDirectory": [b"/home/ljpeg"],
            "jpegPhoto": photo,
        },
    )

    ldap_create_or_fail(
        conn,
        f"o=Bareos GmbH & Co. KG,ou=backup-{testname},{basedn}",
        {"objectClass": [b"top", b"organization"], "o": [b"Bareos GmbH & Co. KG"]},
    )

    ldap_create_or_fail(
        conn,
        f"ou=automount,ou=backup-{testname},{basedn}",
        {"objectClass": [b"top", b"organizationalUnit"], "ou": [b"automount"]},
    )

    # # Objects with / in the DN are currently not supported
    # ldap_create_or_fail(
    #     conn,
    #     "cn=/home,ou=automount,ou=backup,%s" % basedn,
    #     {
    #         "objectClass": [b"top", b"person"],
    #         "cn": [b"/home"],
    #         "sn": [b"Automount objects don't have a surname"],
    #     },
    # )

    ldap_create_or_fail(
        conn,
        f"ou=weird-names,ou=backup-{testname},{basedn}",
        {"objectClass": [b"top", b"organizationalUnit"], "ou": [b"weird-names"]},
    )

    for ou in [
        b" leading-space",
        b"#leading-hash",
        b"space in middle",
        b"trailing-space ",
        b"with\nnewline",
        b"with,comma",
        b'with"quotes"',
        b"with\\backslash",
        b"with+plus",
        b"with#hash",
        b"with;semicolon",
        b"with<less-than",
        b"with=equals",
        b"with>greater-than",
    ]:
        escaped_ou = ldap.dn.escape_dn_chars(ou.decode("utf8"))
        ldap_create_or_fail(
            conn,
            f"ou={escaped_ou},ou=weird-names,ou=backup-{testname},{basedn}",
            {"objectClass": [b"top", b"organizationalUnit"], "ou": [ou]},
        )

    # creating the DN using the normal method wouldn't work, so we create a
    # temporary LDIF and parse that.
    ldif_data = io.BytesIO()
    ldif_data.write(
        f"dn: ou=böses encoding,ou=weird-names,ou=backup-{testname},".encode("utf8")
    )
    ldif_data.write(b"%s\n" % basedn.encode("utf8"))
    ldif_data.write(b"objectClass: top\n")
    ldif_data.write(b"objectClass: organizationalUnit\n")
    ldif_data.write("ou: böses encoding\n".encode("utf8"))
    ldif_data.seek(0)

    ldif_parser = ldif.LDIFRecordList(ldif_data, max_entries=1)
    ldif_parser.parse()

    dn, entry = ldif_parser.all_records[0]
    ldif_data.close()

    ldap_create_or_fail(conn, dn, entry)


def abbrev_value(v):
    """Abbreviate long values for readable LDIF output"""
    length = len(v)
    if length > 80:
        digest = hashlib.sha1(v).hexdigest()
        return ("BLOB len:%d sha1:%s" % (length, digest)).encode("utf8")
    return v


def action_dump(conn, basedn, shorten=True, rewrite_dn=True):
    writer = LDIFWriter(sys.stdout)
    try:
        for dn, attrs in conn.search_s(basedn, ldap.SCOPE_SUBTREE):
            if rewrite_dn:
                dn = dn.replace(basedn, "dc=unified,dc=base,dc=dn")
            if shorten:
                attrs = {
                    k: [abbrev_value(v) for v in vals] for k, vals in attrs.items()
                }
            writer.unparse(dn, attrs)
    except ldap.NO_SUCH_OBJECT:
        print("No object '%s' in directory." % basedn, file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    parser = ArgumentParser(description="Tool to create, remove LDAP test data")
    parser.add_argument(
        "--clean", action="store_true", help="remove data from LDAP server"
    )
    parser.add_argument(
        "--populate", action="store_true", help="populate LDAP server with data"
    )
    parser.add_argument(
        "--dump-backup",
        action="store_true",
        help="print representation of backup subtree",
    )
    parser.add_argument(
        "--dump-restore",
        action="store_true",
        help="print representation of restore subtree",
    )
    parser.add_argument(
        "--full-value",
        action="store_true",
        help="disable shortening of large values during dump",
    )
    parser.add_argument(
        "--real-dn", action="store_true", help="disable rewriting of DN during dump"
    )
    parser.add_argument(
        "--address", "--host", default="localhost", help="LDAP server address"
    )
    parser.add_argument(
        "--basedn", "-b", default=b"dc=example,dc=org", help="LDAP base dn"
    )
    parser.add_argument("--testname", "-t", default=b"py3plug-fd-ldap", help="testname")
    parser.add_argument(
        "--binddn", "-D", default=b"cn=admin,dc=example,dc=org", help="LDAP bind dn"
    )
    parser.add_argument("--password", "-w", default=b"admin", help="LDAP password")

    args = parser.parse_args()

    if (
        not args.clean
        and not args.populate
        and not args.dump_backup
        and not args.dump_restore
    ):
        print("please select at least one action", file=sys.stderr)
        sys.exit(1)

    conn = ldap_connect(args.address, args.binddn, args.password)
    if args.clean:
        action_clean(conn, args.basedn, args.testname)
    if args.populate:
        action_populate(conn, args.basedn, args.testname)
    if args.dump_backup:
        action_dump(
            conn,
            f"ou=backup-{args.testname},{args.basedn}",
            shorten=not args.full_value,
            rewrite_dn=not args.real_dn,
        )
    if args.dump_restore:
        action_dump(
            conn,
            f"ou=restore-{args.testname},{args.basedn}",
            shorten=not args.full_value,
            rewrite_dn=not args.real_dn,
        )
