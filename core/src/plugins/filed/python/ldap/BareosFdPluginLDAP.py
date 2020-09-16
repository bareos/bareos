#!/usr/bin/env python
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2015-2015 Planets Communications B.V.
# Copyright (C) 2015-2020 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation, which is
# listed in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
# Author: Marco van Wieringen
#
# Bareos python class for LDAP related backup and restore
#

import bareosfd

import BareosFdPluginBaseclass
from StringIO import StringIO
from stat import S_IFREG, S_IFDIR, S_IRWXU
import io
import ldif
import ldap
import ldap.resiter
import ldap.modlist
import time
from calendar import timegm


def _safe_encode(data):
    if isinstance(data, unicode):
        return data.encode('utf-8')
    return data


class BytesLDIFRecordList(ldif.LDIFRecordList):
    """Simple encoding wrapper for LDIFRecordList that converts keys to UTF-8"""
    def _next_key_and_value(self):
        # we do not descend from object, so we cannot use super()
        k, v = ldif.LDIFRecordList._next_key_and_value(self)
        try:
            return _safe_encode(k), v
        except:
            return k, v


class BareosFdPluginLDAP(BareosFdPluginBaseclass.BareosFdPluginBaseclass):
    """
    LDAP related backup and restore stuff
    """

    def __init__(self, plugindef):
        bareosfd.DebugMessage(
            100,
            "Constructor called in module %s with plugindef=%s\n"
            % (__name__, plugindef),
        )
        super(BareosFdPluginLDAP, self).__init__(plugindef, ["uri", "basedn"])
        self.ldap = BareosLDAPWrapper()
        self.data_stream = None

    def parse_plugin_definition(self, plugindef):
        """
        Parses the plugin arguments and validates the options.
        """
        bareosfd.DebugMessage(
            100, "parse_plugin_definition() was called in module %s\n" % (__name__)
        )
        super(BareosFdPluginLDAP, self).parse_plugin_definition(plugindef)

        return bareosfd.bRC_OK

    def start_backup_job(self):
        """
        Start of Backup Job
        """
        check_option_bRC = self.check_options()
        if check_option_bRC != bareosfd.bRC_OK:
            return check_option_bRC

        return self.ldap.prepare_backup(self.options)

    def start_backup_file(self, savepkt):
        """
        Defines the file to backup and creates the savepkt.
        """
        bareosfd.DebugMessage(100, "BareosFdPluginLDAP:start_backup_file() called\n")

        return self.ldap.get_next_file_to_backup(savepkt)

    def start_restore_job(self):
        """
        Start of Restore Job
        """
        check_option_bRC = self.check_options()
        if check_option_bRC != bareosfd.bRC_OK:
            return check_option_bRC

        return self.ldap.prepare_restore(self.options)

    def start_restore_file(self, cmd):
        bareosfd.DebugMessage(
            100, "BareosFdPluginLDAP:start_restore_file() called with %s\n" % (cmd)
        )

        # It can happen that this is the first entry point on a restore
        # and we missed the bEventStartRestoreJob event because we registerd
        # that event after it aready fired. If that is true self.ldap.ld will
        # not be set and we call start_restore_job() from here.
        if not self.ldap.ld:
            return self.start_restore_job()
        else:
            return bareosfd.bRC_OK

    def create_file(self, restorepkt):
        """
        Directories are placeholders only we use the data.ldif files
        to get the actual DN of the LDAP record
        """
        bareosfd.DebugMessage(
            100, "BareosFdPluginLDAP:create_file() called with %s\n" % (restorepkt)
        )
        if restorepkt.type == bareosfd.bFileType["FT_DIREND"]:
            restorepkt.create_status = bareosfd.bCFs["CF_SKIP"]
        elif restorepkt.type == bareosfd.bFileType["FT_REG"]:
            self.ldap.set_new_dn(restorepkt.ofname)
            restorepkt.create_status = bareosfd.bCFs["CF_EXTRACT"]
        else:
            bareosfd.JobMessage(
                bareosfd.bJobMessageType["M_FATAL"],
                "Request to restore illegal filetype %s\n" % (restorepkt.type),
            )
            return bareosfd.bRC_Error

        return bareosfd.bRC_OK

    def plugin_io(self, IOP):
        bareosfd.DebugMessage(
            100, "BareosFdPluginLDAP:plugin_io() called with function %s\n" % (IOP.func)
        )

        if IOP.func == bareosfd.bIOPS["IO_OPEN"]:
            self.last_op = IOP.func
            return bareosfd.bRC_OK

        elif IOP.func == bareosfd.bIOPS["IO_CLOSE"]:
            if self.last_op == bareosfd.bIOPS["IO_OPEN"]:
                bareosfd.JobMessage(
                    bareosfd.bJobMessageType["M_WARNING"],
                    "Missing data for DN %s\n" % self.ldap.dn,
                )
            self.last_op = IOP.func
            return bareosfd.bRC_OK

        elif IOP.func == bareosfd.bIOPS["IO_SEEK"]:
            self.last_op = IOP.func
            return bareosfd.bRC_OK

        elif IOP.func == bareosfd.bIOPS["IO_READ"]:
            if not self.data_stream:
                self.data_stream = io.BytesIO(_safe_encode(self.ldap.ldif))
            IOP.buf = bytearray(IOP.count)
            IOP.status = self.data_stream.readinto(IOP.buf)

            if IOP.status == 0:
                self.data_stream = None
                self.ldap.ldif = None

            IOP.io_errno = 0
            self.last_op = IOP.func
            return bareosfd.bRC_OK

        elif IOP.func == bareosfd.bIOPS["IO_WRITE"]:
            if not self.data_stream:
                self.data_stream = io.BytesIO()

            IOP.status = self.data_stream.write(IOP.buf)
            IOP.io_errno = 0

            self.last_op = IOP.func
            return bareosfd.bRC_OK

    def end_backup_file(self):
        bareosfd.DebugMessage(100, "BareosFdPluginLDAP:end_backup_file() called\n")

        return self.ldap.has_next_file()

    def end_restore_file(self):
        bareosfd.DebugMessage(100, "BareosFdPluginLDAP:end_restore_file() called\n")

        if self.data_stream is not None:
            self.ldap.ldif = self.data_stream.getvalue().decode("ascii")
            self.ldap.ldif_len = len(self.ldap.ldif)
            self.data_stream = None

        return self.ldap.restore_entry()


# Overload ldap class and ldap.resiter class for bulk retrieval
class BulkLDAP(ldap.ldapobject.LDAPObject, ldap.resiter.ResultProcessor):
    pass


class BareosLDAPWrapper:
    """
    LDAP specific class.
    """

    def __init__(self):
        self.ld = None
        self.resultset = None
        self.ldap_entries = None
        self.file_to_backup = None
        self.dn = None
        self.entry = None
        self.ldif = None
        self.ldif_len = None
        self.unix_create_time = None
        self.unix_modify_time = None
        self.msg_id = None

    def connect_and_bind(self, options, bulk=False):
        """
        Bind to LDAP URI using the given authentication tokens
        """
        if bulk:
            try:
                self.ld = BulkLDAP(options["uri"], bytes_mode=True)
            except TypeError:
                self.ld = BulkLDAP(options["uri"])
        else:
            try:
                self.ld = ldap.initialize(options["uri"], bytes_mode=True)
            except TypeError:
                self.ld = ldap.initialize(options["uri"])

        try:
            self.ld.protocol_version = ldap.VERSION3
            if "bind_dn" in options and "password" in options:
                self.ld.simple_bind_s(options["bind_dn"], options["password"])
            else:
                self.ld.simple_bind_s("", "")
        except ldap.INVALID_CREDENTIALS:
            bareosfd.JobMessage(
                bareosfd.bJobMessageType["M_FATAL"],
                "Failed to bind to LDAP uri %s\n" % (options["uri"]),
            )

            return bareosfd.bRC_Error
        except ldap.LDAPError as e:
            if type(e.message) == dict and "desc" in e.message:
                bareosfd.JobMessage(
                    bareosfd.bJobMessageType["M_FATAL"],
                    "Failed to bind to LDAP uri %s: %s\n"
                    % (options["uri"], e.message["desc"]),
                )
            else:
                bareosfd.JobMessage(
                    bareosfd.bJobMessageType["M_FATAL"],
                    "Failed to bind to LDAP uri %s: %s\n" % (options["uri"], e),
                )

            return bareosfd.bRC_Error

        bareosfd.DebugMessage(100, "connected to LDAP server\n")

        return bareosfd.bRC_OK

    def prepare_backup(self, options):
        """
        Prepare a LDAP backup
        """
        connect_bRC = self.connect_and_bind(options, True)
        if connect_bRC != bareosfd.bRC_OK:
            return connect_bRC

        bareosfd.DebugMessage(
            100, "Creating search filter and attribute filter to perform LDAP search\n"
        )

        # Get a subtree
        searchScope = ldap.SCOPE_SUBTREE

        # See if there is a specific search filter otherwise use the all object filter.
        if "search_filter" in options:
            searchFilter = options["search_filter"]
        else:
            searchFilter = r"(objectclass=*)"

        # Get all user attributes and createTimestamp + modifyTimestamp
        attributeFilter = ["*", "createTimestamp", "modifyTimestamp"]

        try:
            # Asynchronous search method
            self.msg_id = self.ld.search(
                options["basedn"], searchScope, searchFilter, attributeFilter
            )
        except ldap.LDAPError as e:
            if type(e.message) == dict and "desc" in e.message:
                bareosfd.JobMessage(
                    bareosfd.bJobMessageType["M_FATAL"],
                    "Failed to execute LDAP search on LDAP uri %s: %s\n"
                    % (options["uri"], e.message["desc"]),
                )
            else:
                bareosfd.JobMessage(
                    bareosfd.bJobMessageType["M_FATAL"],
                    "Failed to execute LDAP search on LDAP uri %s: %s\n"
                    % (options["uri"], e),
                )

            return bareosfd.bRC_Error

        bareosfd.DebugMessage(100, "Successfully performed LDAP search\n")

        return bareosfd.bRC_OK

    def prepare_restore(self, options):
        """
        Prepare a LDAP restore
        """
        connect_bRC = self.connect_and_bind(options)
        if connect_bRC != bareosfd.bRC_OK:
            return connect_bRC

        return bareosfd.bRC_OK

    def to_unix_timestamp(self, timestamp):
        if timestamp:
            unix_time = timegm(
                time.strptime(timestamp.replace("Z", "GMT"), "%Y%m%d%H%M%S%Z")
            )
            return int(unix_time)
        else:
            return None

    def get_next_file_to_backup(self, savepkt):
        """
        Find out the next file that should be backed up
        """
        # When file_to_backup is not None we should return the LDIF.
        if self.file_to_backup:
            # Remove some attributes from entry before creating the LDIF.
            ignore_attribute = ["createTimestamp", "modifyTimestamp"]

            keys = self.entry.keys()
            for value in keys:
                if value in ignore_attribute:
                    del self.entry[value]

            # Dump the content of the LDAP entry as LDIF text
            ldif_dump = StringIO()
            ldif_out = ldif.LDIFWriter(ldif_dump)
            try:
                ldif_out.unparse(self.dn, self.entry)
            except UnicodeDecodeError:
                ldif_out.unparse(self.dn.decode('utf-8'), self.entry)

            self.ldif = ldif_dump.getvalue()
            self.ldif_len = len(self.ldif)
            ldif_dump.close()

            statp = bareosfd.StatPacket()
            statp.st_mode = S_IRWXU | S_IFREG
            statp.st_size = self.ldif_len
            if self.unix_create_time:
                statp.st_ctime = self.unix_create_time
            if self.unix_modify_time:
                statp.st_mtime = self.unix_modify_time

            savepkt.statp = statp
            savepkt.type = bareosfd.bFileType["FT_REG"]
            savepkt.fname = self.file_to_backup + "/data.ldif"
            # Read the content of a file
            savepkt.no_read = False

            # On next run we need to get next entry from result set.
            self.file_to_backup = None
        else:
            # If we have no result set get what the LDAP search returned as resultset.
            if self.resultset is None:
                self.resultset = self.ld.allresults(self.msg_id)
                # Try to get the first result set from the query,
                # if there is nothing return an error.
                try:
                    res_type, res_data, res_msgid, res_controls = self.resultset.next()
                    self.ldap_entries = res_data
                except ldap.NO_SUCH_OBJECT:
                    return bareosfd.bRC_Error
                except StopIteration:
                    return bareosfd.bRC_Error

            # Get the next entry from the result set.
            if self.ldap_entries:
                self.dn, self.entry = self.ldap_entries.pop(0)

                if self.dn:
                    # Extract the createTimestamp and modifyTimestamp and
                    # convert it to an UNIX timestamp
                    self.unix_create_time = None
                    try:
                        createTimestamp = self.entry["createTimestamp"][0]
                    except KeyError:
                        pass
                    else:
                        self.unix_create_time = self.to_unix_timestamp(createTimestamp)

                    self.unix_modify_time = None
                    try:
                        modifyTimestamp = self.entry["modifyTimestamp"][0]
                    except KeyError:
                        pass
                    else:
                        self.unix_modify_time = self.to_unix_timestamp(modifyTimestamp)

                    # Convert the DN into a PATH e.g. reverse the elements.
                    dn_sliced = self.dn.split(",")
                    self.file_to_backup = "@LDAP" + "".join(
                        ["/" + element for element in reversed(dn_sliced)]
                    )

                    statp = bareosfd.StatPacket()
                    statp.st_mode = S_IRWXU | S_IFDIR
                    if self.unix_create_time:
                        statp.st_ctime = self.unix_create_time
                    if self.unix_modify_time:
                        statp.st_mtime = self.unix_modify_time

                    savepkt.statp = statp
                    savepkt.type = bareosfd.bFileType["FT_DIREND"]
                    savepkt.fname = self.file_to_backup
                    # A directory has a link field which contains
                    # the fname + a trailing '/'
                    savepkt.link = self.file_to_backup + "/"
                    # Don't read the content of a directory
                    savepkt.no_read = True

                    if "/" in self.dn:
                        bareosfd.JobMessage(
                            bareosfd.bJobMessageType["M_ERROR"],
                            "Slashes (/) in DN not supported. Skipping %s"
                            % self.dn,
                        )
                        # set to none, so the object will not be picket up
                        self.file_to_backup = None

        return bareosfd.bRC_OK

    def set_new_dn(self, fname):
        """
        Generate a LDAP DN based on provided filename and path.

        1. split filename into path components
        2. reverse the list
        3. filter out anything without an equals sign("=")
        4. join components into comma-separated string
        """
        self.dn = _safe_encode(",".join(filter(lambda x: "=" in x, reversed(fname.split("/")))))

    def has_next_file(self):
        # See if we are currently handling the LDIF file or
        # if there is still data in the result set
        if self.file_to_backup or self.ldap_entries:
            bareosfd.DebugMessage(100, "has_next_file(): returning bRC_More\n")
            return bareosfd.bRC_More
        else:
            # See if there are more result sets.
            if self.resultset:
                try:
                    # Get the next result set
                    res_type, res_data, res_msgid, res_controls = self.resultset.next()
                    self.ldap_entries = res_data

                    # We expect something to be in the result set but better check
                    if self.ldap_entries:
                        bareosfd.DebugMessage(
                            100, "has_next_file(): returning bRC_More\n"
                        )
                        return bareosfd.bRC_More
                except StopIteration:
                    pass

        return bareosfd.bRC_OK

    def restore_entry(self):
        # Restore the entry
        if self.ldif:
            # Parse the LDIF back to an attribute list
            ldif_dump = StringIO(str(self.ldif))
            ldif_parser = BytesLDIFRecordList(ldif_dump, max_entries=1)
            ldif_parser.parse()
            dn, entry = ldif_parser.all_records[0]
            dn = _safe_encode(dn)
            ldif_dump.close()

            if self.dn != dn:
                bareosfd.JobMessage(
                    bareosfd.bJobMessageType["M_INFO"],
                    "Restoring original DN %s as %s\n" % (dn, self.dn)
                )

            if dn:
                if self.ld:
                    # Try adding the entry
                    add_ldif = ldap.modlist.addModlist(entry)
                    try:
                        self.ld.add_s(self.dn, add_ldif)
                    except ldap.LDAPError as e:
                        # Delete the original DN
                        try:
                            self.ld.delete_s(self.dn)
                            self.ld.add_s(self.dn, add_ldif)
                        except ldap.LDAPError as e:
                            if type(e.message) == dict and "desc" in e.message:
                                bareosfd.JobMessage(
                                    bareosfd.bJobMessageType["M_ERROR"],
                                    "Failed to restore LDAP DN %s: %s\n"
                                    % (self.dn, e.message["desc"]),
                                )
                            else:
                                bareosfd.JobMessage(
                                    bareosfd.bJobMessageType["M_ERROR"],
                                    "Failed to restore LDAP DN %s: %s\n" % (self.dn, e),
                                )
                            self.ldif = None
                            return bareosfd.bRC_Error
                else:
                    bareosfd.JobMessage(
                        bareosfd.bJobMessageType["M_ERROR"],
                        "Failed to restore LDAP DN %s no writable binding to LDAP exists\n"
                        % (self.dn),
                    )
                    self.ldif = None
                    return bareosfd.bRC_Error

            # Processed ldif
            self.ldif = None

        return bareosfd.bRC_OK

    def cleanup(self):
        if self.ld:
            self.ld.unbind_s()

        return bareosfd.bRC_OK


# vim: ts=4 tabstop=4 expandtab shiftwidth=4 softtabstop=4
