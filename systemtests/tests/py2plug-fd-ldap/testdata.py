#!/usr/bin/env python2
# -*- coding: utf-8 -*-

from __future__ import division, absolute_import, print_function, unicode_literals
import io
import ldap
import ldap.modlist
import ldif
import sys
import hashlib
from argparse import ArgumentParser
from ldif import LDIFWriter


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


def action_clean(conn, basedn):
    """
    Clean up all objects in our subtrees (ldap-variant of "rm -rf").

    find the dns ob all objects below our bases and remove them ordered from
    longest to shortest dn, so we remove parent objects later
    """

    for subtree_dn in ["ou=backup,%s" % basedn, "ou=restore,%s" % basedn]:
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


def action_populate(conn, basedn):
    """Populate our backup data"""
    ldap_create_or_fail(
        conn,
        "ou=backup,%s" % basedn,
        {"objectClass": [b"organizationalUnit"], "ou": [b"restore"]},
    )

    ldap_create_or_fail(
        conn,
        "cn=No JPEG,ou=backup,%s" % basedn,
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

    ldap_create_or_fail(
        conn,
        "cn=Small JPEG,ou=backup,%s" % basedn,
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
            "jpegPhoto": open("image-small.jpg", "rb").read(),
        },
    )

    ldap_create_or_fail(
        conn,
        "cn=Medium JPEG,ou=backup,%s" % basedn,
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
            "jpegPhoto": open("image-medium.jpg", "rb").read(),
        },
    )

    ldap_create_or_fail(
        conn,
        "cn=Large JPEG,ou=backup,%s" % basedn,
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
            "jpegPhoto": open("image-large.jpg", "rb").read(),
        },
    )

    ldap_create_or_fail(
        conn,
        "o=Bareos GmbH & Co. KG,ou=backup,%s" % basedn,
        {"objectClass": [b"top", b"organization"], "o": [b"Bareos GmbH & Co. KG"]},
    )

    ldap_create_or_fail(
        conn,
        "ou=automount,ou=backup,%s" % basedn,
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
        "ou=weird-names,ou=backup,%s" % basedn,
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
        ldap_create_or_fail(
            conn,
            "ou=%s,ou=weird-names,ou=backup,%s" % (ldap.dn.escape_dn_chars(ou), basedn),
            {"objectClass": [b"top", b"organizationalUnit"], "ou": [ou]},
        )

    # creating the DN using the normal method wouldn't work, so we create a
    # temporary LDIF and parse that.
    ldif_data = io.BytesIO()
    ldif_data.write(b"dn: ou=böses encoding,ou=weird-names,ou=backup,")
    ldif_data.write(b"%s\n" % basedn.encode('ascii'))
    ldif_data.write(b"objectClass: top\n")
    ldif_data.write(b"objectClass: organizationalUnit\n")
    ldif_data.write(b"ou: böses encoding\n")
    ldif_data.seek(0)

    ldif_parser = ldif.LDIFRecordList(ldif_data, max_entries=1)
    ldif_parser.parse()
    dn, entry = ldif_parser.all_records[0]
    ldif_data.close()

    ldap_create_or_fail(
        conn,
        dn,
        entry)


def abbrev_value(v):
    """Abbreviate long values for readable LDIF output"""
    length = len(v)
    if length > 80:
        digest = hashlib.sha1(v).hexdigest()
        return "BLOB len:%d sha1:%s" % (length, digest)
    return v


def action_dump(conn, basedn, shorten=True, rewrite_dn=True):
    writer = LDIFWriter(sys.stdout)
    try:
        for dn, attrs in conn.search_s(basedn, ldap.SCOPE_SUBTREE):
            if rewrite_dn:
                dn = (
                    dn.decode("utf-8")
                    .replace(basedn, "dc=unified,dc=base,dc=dn")
                    .encode("utf-8")
                )
            if shorten:
                attrs = {
                    k: [abbrev_value(v) for v in vals] for k, vals in attrs.iteritems()
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
        "--address", "--host", default="ldap://localhost", help="LDAP URI"
    )
    parser.add_argument(
        "--basedn", "-b", default="dc=example,dc=org", help="LDAP base dn"
    )
    parser.add_argument(
        "--binddn", "-D", default="cn=admin,dc=example,dc=org", help="LDAP bind dn"
    )
    parser.add_argument("--password", "-w", default="admin", help="LDAP password")

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
        action_clean(conn, args.basedn)
    if args.populate:
        action_populate(conn, args.basedn)
    if args.dump_backup:
        action_dump(
            conn,
            "ou=backup,%s" % args.basedn,
            shorten=not args.full_value,
            rewrite_dn=not args.real_dn,
        )
    if args.dump_restore:
        action_dump(
            conn,
            "ou=restore,%s" % args.basedn,
            shorten=not args.full_value,
            rewrite_dn=not args.real_dn,
        )
