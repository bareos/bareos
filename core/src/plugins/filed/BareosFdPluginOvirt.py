#!/usr/bin/env python

# Bareos python class for Ovirt related backup and restore

import bareosfd
from bareos_fd_consts import (
    bJobMessageType,
    bFileType,
    bRCs,
    bIOPS,
    bEventType,
    bVariable,
    bCFs,
)

import os
import io
import time
import uuid
import json

import lxml.etree

import logging
import ovirtsdk4 as sdk
import ovirtsdk4.types as types

import ssl

from httplib import HTTPSConnection

try:
    from urllib.parse import urlparse
except ImportError:
    from urlparse import urlparse

import BareosFdPluginBaseclass

# The name of the application, to be used as the 'origin' of events
# sent to the audit log:
APPLICATION_NAME = "Bareos oVirt plugin"


class BareosFdPluginOvirt(BareosFdPluginBaseclass.BareosFdPluginBaseclass):
    """
        Plugin for oVirt backup and restore
    """

    def __init__(self, context, plugindef):
        bareosfd.DebugMessage(
            context,
            100,
            "Constructor called in module %s with plugindef=%s\n"
            % (__name__, plugindef),
        )
        super(BareosFdPluginOvirt, self).__init__(context, plugindef)

        if self.mandatory_options is None:
            self.mandatory_options = []

        # self.mandatory_options.extend(['vmname','storage_domain'])

        self.ovirt = BareosOvirtWrapper()

    def parse_plugin_definition(self, context, plugindef):
        """
        Parses the plugin arguments
        """
        super(BareosFdPluginOvirt, self).parse_plugin_definition(context, plugindef)

        bareosfd.DebugMessage(
            context,
            100,
            "BareosFdPluginOvirt:parse_plugin_definition() called with options '%s' \n"
            % str(self.options),
        )
        self.ovirt.set_options(self.options)
        return bRCs["bRC_OK"]

    def check_options(self, context, mandatory_options=None):
        """
        Check Plugin options
        Note: this is called by parent class parse_plugin_definition(),
        to handle plugin options entered at restore, the real check
        here is done by check_plugin_options() which is called from
        start_backup_job() and start_restore_job()
        """
        return bRCs["bRC_OK"]

    def start_backup_job(self, context):
        """
        Start of Backup Job. Called just before backup job really start.
        Overload this to arrange whatever you have to do at this time.
        """
        bareosfd.DebugMessage(
            context, 100, "BareosFdPluginOvirt:start_backup_job() called\n"
        )

        if chr(self.level) != "F":
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_FATAL"],
                "BareosFdPluginOvirt can only perform level F (Full) backups, but level is %s\n"
                % (chr(self.level)),
            )
            return bRCs["bRC_Error"]

        if not self.ovirt.connect_api(context):
            return bRCs["bRC_Error"]

        return self.ovirt.prepare_vm_backup(context)

    def start_backup_file(self, context, savepkt):
        """
        Defines the file to backup and creates the savepkt.
        """
        bareosfd.DebugMessage(
            context, 100, "BareosFdPluginOvirt:start_backup_file() called\n"
        )

        if not self.ovirt.backup_objects:
            bareosfd.JobMessage(
                context, bJobMessageType["M_ERROR"], "Nothing to backup.\n"
            )
            self.backup_obj = None
            return bRCs["bRC_Skip"]

        self.backup_obj = self.ovirt.backup_objects.pop(0)

        # create a stat packet for a restore object
        statp = bareosfd.StatPacket()
        savepkt.statp = statp

        if "file" in self.backup_obj:

            # regular file
            vmfile = self.backup_obj["file"]

            bareosfd.DebugMessage(
                context,
                100,
                "BareosFdPluginOvirt:start_backup_file() backup regular file '%s' of VM '%s'\n"
                % (vmfile["filename"], self.backup_obj["vmname"]),
            )

            savepkt.type = bFileType["FT_REG"]
            savepkt.fname = "/VMS/%s-%s/%s" % (
                self.backup_obj["vmname"],
                self.backup_obj["vmid"],
                vmfile["filename"],
            )
            self.backup_obj["file"]["fh"] = io.BytesIO(vmfile["data"])

        elif "disk" in self.backup_obj:

            # disk file
            disk = self.backup_obj["disk"]
            # snapshot
            snapshot = self.backup_obj["snapshot"]

            bareosfd.DebugMessage(
                context,
                100,
                "BareosFdPluginOvirt:start_backup_file() backup disk file '%s'('%s'/'%s') of VM '%s'\n"
                % (disk.alias, disk.id, snapshot.id, self.backup_obj["vmname"]),
            )

            savepkt.type = bFileType["FT_REG"]
            savepkt.fname = "/VMS/%s-%s/%s-%s/%s" % (
                self.backup_obj["vmname"],
                self.backup_obj["vmid"],
                disk.alias,
                disk.id,
                snapshot.id,
            )

            try:
                self.ovirt.start_download(context, snapshot, disk)
            except Exception as e:
                bareosfd.JobMessage(
                    context,
                    bJobMessageType["M_ERROR"],
                    "BareosFdPluginOvirt:start_backup_file() Error: %s" % str(e),
                )
                self.ovirt.end_transfer(context)
                return bRCs["bRC_Error"]

        elif "disk_metadata" in self.backup_obj:
            # save disk metadata as restoreobject

            disk_alias = self.backup_obj["disk_metadata"]["alias"]
            disk_id = self.backup_obj["disk_metadata"]["id"]
            snapshot_id = self.backup_obj["snapshot_id"]
            disk_metadata_json = json.dumps(
                {"disk_metadata": self.backup_obj["disk_metadata"]}
            )

            bareosfd.DebugMessage(
                context,
                100,
                "BareosFdPluginOvirt:start_backup_file() backup disk metadata '%s'('%s'/'%s') of VM '%s': %s\n"
                % (
                    disk_alias,
                    disk_id,
                    snapshot_id,
                    self.backup_obj["vmname"],
                    disk_metadata_json,
                ),
            )

            savepkt.type = bFileType["FT_RESTORE_FIRST"]
            savepkt.fname = "/VMS/%s-%s/%s-%s/%s.metadata" % (
                self.backup_obj["vmname"],
                self.backup_obj["vmid"],
                disk_alias,
                disk_id,
                snapshot_id,
            )
            savepkt.object_name = savepkt.fname
            savepkt.object = bytearray(disk_metadata_json)
            savepkt.object_len = len(savepkt.object)
            savepkt.object_index = int(time.time())

        else:
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_FATAL"],
                "BareosFdPluginOvirt:start_backup_file(): Invalid data in backup_obj, keys: %s\n"
                % (self.backup_obj.keys()),
            )
            return bRCs["bRC_Error"]

        bareosfd.JobMessage(
            context,
            bJobMessageType["M_INFO"],
            "Starting backup of %s\n" % savepkt.fname,
        )

        return bRCs["bRC_OK"]

    def create_file(self, context, restorepkt):
        """
        Creates the file to be restored and directory structure, if needed.
        Adapt this in your derived class, if you need modifications for
        virtual files or similar
        """
        bareosfd.DebugMessage(
            context,
            100,
            "create_file() entry point in Python called with %s\n" % (restorepkt),
        )

        if self.options.get("local") == "yes":
            FNAME = restorepkt.ofname
            dirname = os.path.dirname(FNAME)
            if not os.path.exists(dirname):
                bareosfd.DebugMessage(
                    context,
                    200,
                    "Directory %s does not exist, creating it now\n" % dirname,
                )
                os.makedirs(dirname)
            # open creates the file, if not yet existing, we close it again right
            # aways it will be opened again in plugin_io.
            # But: only do this for regular files, prevent from
            # IOError: (21, 'Is a directory', '/tmp/bareos-restores/my/dir/')
            # if it's a directory
            if restorepkt.type == bFileType["FT_REG"]:
                open(FNAME, "wb").close()
        else:
            if not restorepkt.ofname.endswith(".ovf"):
                FNAME = restorepkt.ofname
                FNAME = FNAME.decode("utf-8")

                disk = self.ovirt.get_vm_disk_by_basename(context, FNAME)
                if disk is None:
                    bareosfd.JobMessage(
                        context,
                        bJobMessageType["M_ERROR"],
                        "BareosFdPluginOvirt:create_file() Unable to restore disk %s.\n"
                        % (FNAME),
                    )
                    return bRCs["bRC_Error"]
                else:
                    self.ovirt.start_upload(context, disk)

        if restorepkt.type == bFileType["FT_REG"]:
            restorepkt.create_status = bCFs["CF_EXTRACT"]
        return bRCs["bRC_OK"]

    def start_restore_job(self, context):
        """
        Start of Restore Job. Called , if you have Restore objects.
        Overload this to handle restore objects, if applicable
        """
        bareosfd.DebugMessage(
            context, 100, "BareosFdPluginOvirt:start_restore_job() called\n"
        )
        if self.options.get("local") == "yes":
            bareosfd.DebugMessage(
                context,
                100,
                "BareosFdPluginOvirt:start_restore_job(): restore to local file, skipping checks\n",
            )
            return bRCs["bRC_OK"]
        else:
            # restore to VM to OVirt
            if not self.ovirt.connect_api(context):
                return bRCs["bRC_Error"]

        return bRCs["bRC_OK"]

    def start_restore_file(self, context, cmd):
        bareosfd.DebugMessage(
            context,
            100,
            "BareosFdPluginOvirt:start_restore_file() called with %s\n" % (cmd),
        )
        return bRCs["bRC_OK"]

    def plugin_io(self, context, IOP):
        """
        Called for io operations.
        """
        bareosfd.DebugMessage(
            context, 100, "plugin_io called with function %s\n" % (IOP.func)
        )
        bareosfd.DebugMessage(context, 100, "FNAME is set to %s\n" % (self.FNAME))
        self.jobType = chr(bareosfd.GetValue(context, bVariable["bVarType"]))
        bareosfd.DebugMessage(
            context,
            100,
            "BareosFdPluginOvirt::plugin_io() jobType: %s\n" % (self.jobType),
        )

        if IOP.func == bIOPS["IO_OPEN"]:

            self.FNAME = IOP.fname
            bareosfd.DebugMessage(
                context, 100, "self.FNAME was set to %s from IOP.fname\n" % (self.FNAME)
            )

            if self.options.get("local") == "yes":
                try:
                    if IOP.flags & (os.O_CREAT | os.O_WRONLY):
                        bareosfd.DebugMessage(
                            context,
                            100,
                            "Open file %s for writing with %s\n" % (self.FNAME, IOP),
                        )

                        dirname = os.path.dirname(self.FNAME)
                        if not os.path.exists(dirname):
                            bareosfd.DebugMessage(
                                context,
                                100,
                                "Directory %s does not exist, creating it now\n"
                                % (dirname),
                            )
                            os.makedirs(dirname)
                        self.file = open(self.FNAME, "wb")
                    else:
                        IOP.status = -1
                        bareosfd.JobMessage(
                            context,
                            bJobMessageType["M_FATAL"],
                            "plugin_io: option local=yes can only be used on restore\n",
                        )
                    return bRCs["bRC_Error"]
                except (OSError, IOError) as io_open_error:
                    IOP.status = -1
                    bareosfd.DebugMessage(
                        context,
                        100,
                        "plugin_io: failed to open %s: %s\n"
                        % (self.FNAME, io_open_error.strerror),
                    )
                    bareosfd.JobMessage(
                        context,
                        bJobMessageType["M_FATAL"],
                        "plugin_io: failed to open %s: %s\n"
                        % (self.FNAME, io_open_error.strerror),
                    )
                    return bRCs["bRC_Error"]

            return bRCs["bRC_OK"]

        elif IOP.func == bIOPS["IO_CLOSE"]:
            if self.file is not None:
                bareosfd.DebugMessage(context, 100, "Closing file " + "\n")
                self.file.close()
            elif self.jobType == "B":
                if "file" in self.backup_obj:
                    self.backup_obj["file"]["fh"].close()
                elif "disk" in self.backup_obj:
                    bareosfd.DebugMessage(
                        context, 100, "plugin_io: calling end_transfer()\n"
                    )
                    # Backup Job
                    self.ovirt.end_transfer(context)
            elif self.jobType == "R":
                if self.FNAME.endswith(".ovf"):
                    return self.ovirt.prepare_vm_restore(context)
                else:
                    self.ovirt.end_transfer(context)
            return bRCs["bRC_OK"]
        elif IOP.func == bIOPS["IO_SEEK"]:
            return bRCs["bRC_OK"]
        elif IOP.func == bIOPS["IO_READ"]:
            if "file" in self.backup_obj:
                IOP.buf = bytearray(IOP.count)
                IOP.status = self.backup_obj["file"]["fh"].readinto(IOP.buf)
                IOP.io_errno = 0
                bareosfd.DebugMessage(
                    context,
                    100,
                    "plugin_io: read from file %s\n"
                    % (self.backup_obj["file"]["filename"]),
                )
            elif "disk" in self.backup_obj:
                bareosfd.DebugMessage(
                    context,
                    100,
                    "plugin_io: read from disk %s\n" % (self.backup_obj["disk"].alias),
                )
                try:
                    IOP.buf = bytearray(IOP.count)
                    chunk = self.ovirt.process_download(context, IOP.count)
                    IOP.buf[:] = chunk
                    IOP.status = len(IOP.buf)
                    IOP.io_errno = 0
                except Exception as e:
                    bareosfd.JobMessage(
                        context,
                        bJobMessageType["M_ERROR"],
                        "BareosFdPluginOvirt:plugin_io() Error: %s" % str(e),
                    )
                    self.ovirt.end_transfer(context)
                    return bRCs["bRC_Error"]
            else:
                bareosfd.JobMessage(
                    context,
                    bJobMessageType["M_ERROR"],
                    "BareosFdPluginOvirt:plugin_io() Unable to read data to backup.",
                )
                return bRCs["bRC_Error"]
            return bRCs["bRC_OK"]
        elif IOP.func == bIOPS["IO_WRITE"]:
            if self.file is not None:
                try:
                    bareosfd.DebugMessage(
                        context, 200, "Writing buffer to file %s\n" % (self.FNAME)
                    )
                    self.file.write(IOP.buf)
                    IOP.status = IOP.count
                    IOP.io_errno = 0
                except IOError as msg:
                    IOP.io_errno = -1
                    bareosfd.DebugMessage(
                        context, 100, "Error writing data: " + msg + "\n"
                    )
                    return bRCs["bRC_Error"]
            elif self.FNAME.endswith(".ovf"):
                self.ovirt.process_ovf(context, IOP.buf)
                IOP.status = IOP.count
                IOP.io_errno = 0
            else:
                self.ovirt.process_upload(context, IOP.buf)
                IOP.status = IOP.count
                IOP.io_errno = 0
            return bRCs["bRC_OK"]

    def handle_plugin_event(self, context, event):

        if event == bEventType["bEventEndBackupJob"]:
            bareosfd.DebugMessage(
                context,
                100,
                "BareosFdPluginOvirt::handle_plugin_event() called with bEventEndBackupJob\n",
            )
            bareosfd.DebugMessage(context, 100, "removing Snapshot\n")
            self.ovirt.end_vm_backup(context)

        elif event == bEventType["bEventEndFileSet"]:
            bareosfd.DebugMessage(
                context,
                100,
                "BareosFdPluginOvirt::handle_plugin_event() called with bEventEndFileSet\n",
            )

        elif event == bEventType["bEventStartBackupJob"]:
            bareosfd.DebugMessage(
                context,
                100,
                "BareosFdPluginOvirt::handle_plugin_event() called with bEventStartBackupJob\n",
            )

            return self.start_backup_job(context)

        elif event == bEventType["bEventStartRestoreJob"]:
            bareosfd.DebugMessage(
                context,
                100,
                "BareosFdPluginOvirt::handle_plugin_event() called with bEventStartRestoreJob\n",
            )

            return self.start_restore_job(context)

        elif event == bEventType["bEventEndRestoreJob"]:
            bareosfd.DebugMessage(
                context,
                100,
                "BareosFdPluginOvirt::handle_plugin_event() called with bEventEndRestoreJob\n",
            )
            bareosfd.DebugMessage(context, 100, "removing Snapshot\n")
            self.ovirt.end_vm_restore(context)
        else:
            bareosfd.DebugMessage(
                context,
                100,
                "BareosFdPluginOvirt::handle_plugin_event() called with event %s\n"
                % (event),
            )

        return bRCs["bRC_OK"]

    def end_backup_file(self, context):
        bareosfd.DebugMessage(
            context,
            100,
            "BareosFdPluginOvirt::end_backup_file() entry point in Python called\n",
        )
        if self.ovirt.transfer_start_time:
            elapsed_seconds = round(time.time() - self.ovirt.transfer_start_time, 2)
            download_rate = round(
                self.ovirt.init_bytes_to_transf / 1000.0 / elapsed_seconds, 1
            )
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_INFO"],
                "   Transfer time: %s s bytes: %s rate: %s KB/s\n"
                % (elapsed_seconds, self.ovirt.init_bytes_to_transf, download_rate),
            )
            self.ovirt.transfer_start_time = None

        if self.ovirt.backup_objects:
            return bRCs["bRC_More"]
        else:
            return bRCs["bRC_OK"]

    def end_restore_file(self, context):
        bareosfd.DebugMessage(
            context,
            100,
            "BareosFdPluginOvirt::end_restore_file() entry point in Python called\n",
        )
        if self.ovirt.transfer_start_time:
            elapsed_seconds = round(time.time() - self.ovirt.transfer_start_time, 2)
            download_rate = round(
                self.ovirt.init_bytes_to_transf / 1000.0 / elapsed_seconds, 1
            )
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_INFO"],
                "   Upload time: %s s bytes: %s rate: %s KB/s\n"
                % (elapsed_seconds, self.ovirt.init_bytes_to_transf, download_rate),
            )
            self.ovirt.transfer_start_time = None
        return bRCs["bRC_OK"]

    def restore_object_data(self, context, ROP):
        """
        Note:
        This is called in two cases:
        - on diff/inc backup (should be called only once)
        - on restore (for every job id being restored)
        But at the point in time called, it is not possible
        to distinguish which of them it is, because job type
        is "I" until the bEventStartBackupJob event
        """
        bareosfd.DebugMessage(
            context,
            100,
            "BareosFdPluginOvirt:restore_object_data() called with ROP:%s\n" % (ROP),
        )
        bareosfd.DebugMessage(
            context,
            100,
            "ROP.object_name(%s): %s\n" % (type(ROP.object_name), ROP.object_name),
        )
        bareosfd.DebugMessage(
            context,
            100,
            "ROP.plugin_name(%s): %s\n" % (type(ROP.plugin_name), ROP.plugin_name),
        )
        bareosfd.DebugMessage(
            context,
            100,
            "ROP.object_len(%s): %s\n" % (type(ROP.object_len), ROP.object_len),
        )
        bareosfd.DebugMessage(
            context,
            100,
            "ROP.object_full_len(%s): %s\n"
            % (type(ROP.object_full_len), ROP.object_full_len),
        )
        bareosfd.DebugMessage(
            context, 100, "ROP.object(%s): %s\n" % (type(ROP.object), ROP.object)
        )
        ro_data = json.loads(str(ROP.object))
        self.ovirt.disk_metadata_by_id[ro_data["disk_metadata"]["id"]] = ro_data[
            "disk_metadata"
        ]

        return bRCs["bRC_OK"]


class BareosOvirtWrapper(object):
    """
    Ovirt wrapper class
    """

    def __init__(self):

        self.options = None
        self.ca = None

        self.connection = None
        self.system_service = None
        self.vms_service = None
        self.storage_domains_service = None
        self.events_service = None
        self.network_profiles = None

        self.vm = None
        self.ovf_data = None
        self.ovf = None
        self.vm_service = None
        self.snap_service = None
        self.transfer_service = None
        self.event_id = None

        self.proxy_connection = None
        self.bytes_to_transf = None
        self.init_bytes_to_transf = None
        self.transfer_start_time = None

        self.backup_objects = None
        self.restore_objects = None
        self.disk_metadata_by_id = {}

        self.old_new_ids = {}

        self.snapshot_remove_timeout = 300

    def set_options(self, options):
        # make the plugin options also available in this class
        self.options = options

    def connect_api(self, context):

        # ca certificate
        self.ca = self.options["ca"]

        # set server url
        server_url = "https://%s/ovirt-engine/api" % str(self.options["server"])

        ovirt_sdk_debug = False
        if self.options.get("ovirt_sdk_debug_log") is not None:
            logging.basicConfig(
                level=logging.DEBUG, filename=self.options["ovirt_sdk_debug_log"]
            )
            ovirt_sdk_debug = True

        # Create a connection to the server:
        self.connection = sdk.Connection(
            url=server_url,
            username=self.options["username"],
            password=self.options["password"],
            ca_file=self.ca,
            debug=ovirt_sdk_debug,
            log=logging.getLogger(),
        )

        if not self.connection:
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_FATAL"],
                "Cannot connect to host %s with user %s and ca file %s\n"
                % (self.options["server"], self.options["username"], self.ca),
            )
            return False

        # Get a reference to the root service:
        self.system_service = self.connection.system_service()

        # Get the reference to the "vms" service:
        self.vms_service = self.system_service.vms_service()

        if not self.vms_service:
            return False

        # get network profiles
        profiles_service = self.system_service.vnic_profiles_service()
        self.network_profiles = {
            profile.name: profile.id for profile in profiles_service.list()
        }

        # Get the reference to the service that we will use to send events to
        # the audit log:
        self.events_service = self.system_service.events_service()

        # In order to send events we need to also send unique integer ids.
        self.event_id = int(time.time())

        bareosfd.DebugMessage(
            context,
            100,
            (
                "Successfully connected to Ovirt Engine API on '%s' with"
                " user %s and ca file %s\n"
            )
            % (self.options["server"], self.options["username"], self.ca),
        )

        return True

    def prepare_vm_backup(self, context):
        """
        prepare VM backup:
        - get vm details
        - take snapshot
        - get disk devices
        """
        bareosfd.DebugMessage(
            context,
            100,
            "BareosOvirtWrapper::prepare_vm_backup: prepare VM to backup\n",
        )

        if not self.get_vm(context):
            bareosfd.DebugMessage(context, 100, "Error getting details for VM\n")

            return bRCs["bRC_Error"]
        else:
            # Locate the service that manages the virtual machine:
            self.vm_service = self.vms_service.vm_service(self.vm.id)

            # check if vm have snapshosts
            snaps_service = self.vm_service.snapshots_service()
            if len(snaps_service.list()) > 1:
                bareosfd.JobMessage(
                    context,
                    bJobMessageType["M_FATAL"],
                    "Error '%s' already have snapshosts. This is not supported\n"
                    % (self.vm.name),
                )
                return bRCs["bRC_Error"]

            bareosfd.DebugMessage(
                context, 100, "Start the backup of VM %s\n" % (self.vm.name)
            )

            # Save the OVF to a file, so that we can use to restore the virtual
            # machine later. The name of the file is the name of the virtual
            # machine, followed by a dash and the identifier of the virtual machine,
            # to make it unique:
            ovf_data = self.vm.initialization.configuration.data
            ovf_file = "%s-%s.ovf" % (self.vm.name, self.vm.id)
            if self.backup_objects is None:
                self.backup_objects = []

            self.backup_objects.append(
                {
                    "vmname": self.vm.name,
                    "vmid": self.vm.id,
                    "file": {"data": ovf_data, "filename": ovf_file},
                }
            )

            # create vm snapshots
            self.create_vm_snapshot(context)

            # get vm backup disks from snapshot
            if not self.get_vm_backup_disks(context):
                bareosfd.JobMessage(
                    context,
                    bJobMessageType["M_FATAL"],
                    "Error getting Backup Disks VM %s from snapshot\n" % (self.vm.name),
                )
                return bRCs["bRC_Error"]

            return bRCs["bRC_OK"]

    def get_vm(self, context):
        search = None
        if "uuid" in self.options:
            search = "uuid=%s" % str(self.options["uuid"])
        elif "vm_name" in self.options:
            search = "name=%s" % str(self.options["vm_name"])

        if search is not None:
            bareosfd.DebugMessage(context, 100, "get_vm search vm by '%s'\n" % (search))
            res = self.vms_service.list(search=search, all_content=True)
            if len(res) > 0:
                self.vm = res[0]
                return True
        return False

    def create_vm_snapshot(self, context):
        """
        Creates a snapshot
        """

        # Create an unique description for the snapshot, so that it is easier
        # for the administrator to identify this snapshot as a temporary one
        # created just for backup purposes:
        snap_description = "%s-backup-%s" % (self.vm.name, uuid.uuid4())

        # Send an external event to indicate to the administrator that the
        # backup of the virtual machine is starting. Note that the description
        # of the event contains the name of the virtual machine and the name of
        # the temporary snapshot, this way, if something fails, the administrator
        # will know what snapshot was used and remove it manually.
        self.events_service.add(
            event=types.Event(
                vm=types.Vm(id=self.vm.id,),
                origin=APPLICATION_NAME,
                severity=types.LogSeverity.NORMAL,
                custom_id=self.event_id,
                description=(
                    "Backup of virtual machine '%s' using snapshot '%s' is "
                    "starting." % (self.vm.name, snap_description)
                ),
            ),
        )
        self.event_id += 1

        # Send the request to create the snapshot. Note that this will return
        # before the snapshot is completely created, so we will later need to
        # wait till the snapshot is completely created.
        # The snapshot will not include memory. Change to True the parameter
        # persist_memorystate to get it (in that case the VM will be paused for a while).
        snaps_service = self.vm_service.snapshots_service()
        snap = snaps_service.add(
            snapshot=types.Snapshot(
                description=snap_description, persist_memorystate=False,
            ),
        )
        bareosfd.JobMessage(
            context,
            bJobMessageType["M_INFO"],
            "Sent request to create snapshot '%s', the id is '%s'.\n"
            % (snap.description, snap.id),
        )

        # Poll and wait till the status of the snapshot is 'ok', which means
        # that it is completely created:
        self.snap_service = snaps_service.snapshot_service(snap.id)
        while snap.snapshot_status != types.SnapshotStatus.OK:
            bareosfd.DebugMessage(
                context,
                100,
                "Waiting till the snapshot is created, the status is now '%s'.\n"
                % snap.snapshot_status,
            )
            time.sleep(1)
            snap = self.snap_service.get()
            time.sleep(1)

        # wait some time until snapshot real complete
        time.sleep(10)

        bareosfd.JobMessage(
            context, bJobMessageType["M_INFO"], "'  The snapshot is now complete.\n"
        )

    def get_vm_backup_disks(self, context):

        has_disks = False

        if self.backup_objects is None:
            self.backup_objects = []

        # get snapshot
        self.snap_service.get()

        # Get a reference to the storage domains service:
        storage_domains_service = self.system_service.storage_domains_service()

        # Retrieve the descriptions of the disks of the snapshot:
        snap_disks_service = self.snap_service.disks_service()
        snap_disks = snap_disks_service.list()

        # download disk snaphost
        for snap_disk in snap_disks:
            disk_id = snap_disk.id
            disk_alias = snap_disk.alias
            bareosfd.DebugMessage(
                context,
                200,
                "get_vm_backup_disks(): disk_id: '%s' disk_alias '%s'\n"
                % (disk_id, disk_alias),
            )

            if not self.is_included(context, snap_disk):
                continue

            if self.is_excluded(context, snap_disk):
                continue

            sd_id = snap_disk.storage_domains[0].id

            # Get a reference to the storage domain service in which the disk snapshots reside:
            storage_domain_service = storage_domains_service.storage_domain_service(
                sd_id
            )

            # Get a reference to the disk snapshots service:
            disk_snapshot_service = storage_domain_service.disk_snapshots_service()

            # Get a list of disk snapshots by a disk ID
            all_disk_snapshots = disk_snapshot_service.list()

            # Filter disk snapshots list by disk id
            disk_snapshots = [s for s in all_disk_snapshots if s.disk.id == disk_id]

            # Download disk snapshot
            if len(disk_snapshots) > 0:
                for disk_snapshot in disk_snapshots:
                    self.backup_objects.append(
                        {
                            "vmname": self.vm.name,
                            "vmid": self.vm.id,
                            "snapshot": disk_snapshot,
                            "disk": snap_disk,
                        }
                    )
                has_disks = True
        return has_disks

    def is_included(self, context, disk):
        if self.options.get("include_disk_aliases") is None:
            return True

        include_disk_aliases = self.options["include_disk_aliases"].split(",")
        if disk.alias in include_disk_aliases:
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_INFO"],
                "Including disk with alias %s (Id %s)\n" % (disk.alias, disk.id),
            )
            return True

        return False

    def is_excluded(self, context, disk):
        if self.options.get("exclude_disk_aliases") is None:
            return False

        exclude_disk_aliases = self.options["exclude_disk_aliases"].split(",")

        if disk.alias in exclude_disk_aliases:
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_INFO"],
                "Excluding disk with alias %s (Id %s)\n" % (disk.alias, disk.id),
            )
            return True

        return False

    def get_transfer_service(self, image_transfer):
        # Get a reference to the service that manages the image transfer:
        transfers_service = self.system_service.image_transfers_service()

        # Add a new image transfer:
        transfer = transfers_service.add(image_transfer)

        # Get reference to the created transfer service:
        transfer_service = transfers_service.image_transfer_service(transfer.id)

        while transfer.phase == types.ImageTransferPhase.INITIALIZING:
            time.sleep(1)
            transfer = transfer_service.get()

        return transfer_service

    def get_proxy_connection(self, proxy_url):
        # At this stage, the SDK granted the permission to start transferring the disk, and the
        # user should choose its preferred tool for doing it - regardless of the SDK.
        # In this example, we will use Python's httplib.HTTPSConnection for transferring the data.
        context = ssl.create_default_context()

        # Note that ovirt-imageio-proxy by default checks the certificates, so if you don't have
        # your CA certificate of the engine in the system, you need to pass it to HTTPSConnection.
        context.load_verify_locations(cafile=self.ca)

        return HTTPSConnection(proxy_url.hostname, proxy_url.port, context=context,)

    def start_download(self, context, snapshot, disk):

        bareosfd.JobMessage(
            context,
            bJobMessageType["M_INFO"],
            "Downloading snapshot '%s' of disk '%s'('%s')\n"
            % (snapshot.id, disk.alias, disk.id),
        )

        self.transfer_service = self.get_transfer_service(
            types.ImageTransfer(
                snapshot=types.DiskSnapshot(id=snapshot.id),
                direction=types.ImageTransferDirection.DOWNLOAD,
            )
        )
        transfer = self.transfer_service.get()
        proxy_url = urlparse(transfer.proxy_url)
        self.proxy_connection = self.get_proxy_connection(proxy_url)

        # Set needed headers for downloading:
        transfer_headers = {
            "Authorization": transfer.signed_ticket,
        }

        # Perform the request.
        self.proxy_connection.request(
            "GET", proxy_url.path, headers=transfer_headers,
        )
        # Get response
        self.response = self.proxy_connection.getresponse()

        # Check the response status:
        if self.response.status >= 300:
            bareosfd.JobMessage(
                context, bJobMessageType["M_ERROR"], "Error: %s" % self.response.read()
            )

        self.bytes_to_transf = int(self.response.getheader("Content-Length"))

        self.backup_objects.insert(
            0,
            {
                "vmname": self.vm.name,
                "vmid": self.vm.id,
                "snapshot_id": snapshot.id,
                "disk_metadata": {
                    "id": disk.id,
                    "alias": disk.alias,
                    "effective_size": self.bytes_to_transf,
                },
            },
        )

        bareosfd.JobMessage(
            context,
            bJobMessageType["M_INFO"],
            "   Transfer disk snapshot of %s bytes\n" % (str(self.bytes_to_transf)),
        )

        self.init_bytes_to_transf = self.bytes_to_transf
        self.transfer_start_time = time.time()

    def process_download(self, context, chunk_size):

        chunk = ""

        bareosfd.DebugMessage(
            context,
            100,
            "process_download(): transfer %s of %s (%s) \n"
            % (
                self.bytes_to_transf,
                self.response.getheader("Content-Length"),
                chunk_size,
            ),
        )

        if self.bytes_to_transf > 0:

            # Calculate next chunk to read
            to_read = min(self.bytes_to_transf, chunk_size)

            # Read next info buffer
            chunk = self.response.read(to_read)

            if chunk == "":
                bareosfd.DebugMessage(
                    context, 100, "process_download(): Socket disconnected. \n"
                )
                bareosfd.JobMessage(
                    context,
                    bJobMessageType["M_ERROR"],
                    "process_download(): Socket disconnected.",
                )

                raise RuntimeError("Socket disconnected")

            # Update bytes_to_transf
            self.bytes_to_transf -= len(chunk)

            completed = 1 - (
                self.bytes_to_transf / float(self.response.getheader("Content-Length"))
            )

            bareosfd.DebugMessage(
                context, 100, "process_download(): Completed {:.0%}\n".format(completed)
            )

        return chunk

    def prepare_vm_restore(self, context):
        restore_existing_vm = False
        if self.connection is None:
            # if not connected yet
            if not self.connect_api(context, self.options):
                return bRCs["bRC_Error"]

        if self.ovf_data is None:
            bareosfd.JobMessage(
                context,
                bJobMessageType["M_FATAL"],
                "Unable to restore VM. No OVF data. \n",
            )
            return bRCs["bRC_Error"]
        else:
            if "storage_domain" not in self.options:
                bareosfd.JobMessage(
                    context,
                    bJobMessageType["M_FATAL"],
                    "No storage domain specified.\n",
                )
                return bRCs["bRC_Error"]

            storage_domain = self.options["storage_domain"]

            # Parse the OVF as XML document:
            self.ovf = Ovf(self.ovf_data)

            bareosfd.DebugMessage(
                context, 200, "prepare_vm_restore(): %s\n" % (self.ovf)
            )

            # Find the name of the virtual machine within the OVF:
            vm_name = None
            if "vm_name" in self.options:
                vm_name = self.options["vm_name"]

            if vm_name is None:
                vm_name = self.ovf.get_element("vm_name")

            # Find the cluster name of the virtual machine within the OVF:
            cluster_name = None
            if "cluster_name" in self.options:
                cluster_name = self.options["cluster_name"]

            if cluster_name is None:
                # Find the cluster name of the virtual machine within the OVF:
                cluster_name = self.ovf.get_element("cluster_name")

            # check if VM exists
            res = self.vms_service.list(
                search="name=%s and cluster=%s" % (str(vm_name), str(cluster_name))
            )
            if len(res) > 1:
                bareosfd.JobMessage(
                    context,
                    bJobMessageType["M_FATAL"],
                    "Found %s VMs with name '%s'\n" % (len(res), str(vm_name)),
                )
                return bRCs["bRC_Error"]

            if len(res) == 1:
                if not self.options.get("overwrite") == "yes":
                    bareosfd.JobMessage(
                        context,
                        bJobMessageType["M_FATAL"],
                        "If you are sure you want to overwrite the existing VM '%s', please add the plugin option 'overwrite=yes'\n"
                        % (str(vm_name)),
                    )
                    return bRCs["bRC_Error"]

                bareosfd.JobMessage(
                    context,
                    bJobMessageType["M_INFO"],
                    "Restore to existing VM '%s'\n" % str(vm_name),
                )
                self.vm = res[0]

                if self.vm.status != types.VmStatus.DOWN:
                    bareosfd.JobMessage(
                        context,
                        bJobMessageType["M_FATAL"],
                        "VM '%s' must be down for restore, but status is %s\n"
                        % (str(vm_name), self.vm.status),
                    )
                    return bRCs["bRC_Error"]

                restore_existing_vm = True

            else:
                self.create_vm(context, vm_name, cluster_name)
                self.add_nics_to_vm(context)

            # Extract disk information from OVF
            disk_elements = self.ovf.get_elements("disk_elements")

            if self.restore_objects is None:
                self.restore_objects = []

            for disk_element in disk_elements:
                # Get disk properties:
                props = {}
                for key, value in disk_element.items():
                    key = key.replace("{%s}" % self.ovf.OVF_NAMESPACES["ovf"], "")
                    props[key] = value

                # set storage domain
                props["storage_domain"] = storage_domain
                self.restore_objects.append(props)
                if restore_existing_vm:
                    # When restoring to existing VM, old and new disk ID are identical
                    # Important: The diskId property in the OVF is not the oVirt
                    # diskId, which can be found in the first part of the OVF fileRef
                    old_disk_id = os.path.dirname(props["fileRef"])
                    self.old_new_ids[old_disk_id] = old_disk_id

            bareosfd.DebugMessage(
                context,
                200,
                "end of prepare_vm_restore(): self.restore_objects: %s self.old_new_ids: %s\n"
                % (self.restore_objects, self.old_new_ids),
            )
        return bRCs["bRC_OK"]

    def create_vm(self, context, vm_name, cluster_name):

        vm_template = "Blank"
        if "vm_template" in self.options:
            vm_template = self.options["vm_template"]

        # Add the virtual machine, the transfered disks will be
        # attached to this virtual machine:
        bareosfd.JobMessage(
            context, bJobMessageType["M_INFO"], "Adding virtual machine %s\n" % vm_name,
        )

        # vm type (server/desktop)
        vm_type = "server"
        if "vm_type" in self.options:
            vm_type = self.options["vm_type"]

        # vm memory and cpu
        vm_memory = None
        if "vm_memory" in self.options:
            vm_memory = int(self.options["vm_memory"]) * 2 ** 20

        vm_cpu = None
        if "vm_cpu" in self.options:
            vm_cpu_arr = self.options["vm_cpu"].split(",")
            if len(vm_cpu_arr) > 0:
                if len(vm_cpu_arr) < 2:
                    vm_cpu_arr.append(1)
                    vm_cpu_arr.append(1)
                elif len(vm_cpu_arr) < 3:
                    vm_cpu_arr.append(1)

                vm_cpu = types.Cpu(
                    topology=types.CpuTopology(
                        cores=int(vm_cpu_arr[0]),
                        sockets=int(vm_cpu_arr[1]),
                        threads=int(vm_cpu_arr[2]),
                    )
                )

        if vm_memory is None or vm_cpu is None:
            # Find the resources section
            resources_elements = self.ovf.get_elements("resources_elements")
            for resource in resources_elements:
                # Get resource properties:
                props = {}
                for e in resource:
                    key = e.tag
                    value = e.text
                    key = key.replace("{%s}" % self.ovf.OVF_NAMESPACES["rasd"], "")
                    props[key] = value

                if vm_cpu is None:
                    # for ResourceType = 3 (CPU)
                    if int(props["ResourceType"]) == 3:
                        vm_cpu = types.Cpu(
                            topology=types.CpuTopology(
                                cores=int(props["cpu_per_socket"]),
                                sockets=int(props["num_of_sockets"]),
                                threads=int(props["threads_per_cpu"]),
                            )
                        )
                if vm_memory is None:
                    # for ResourceType = 4 (Memory)
                    if int(props["ResourceType"]) == 4:
                        vm_memory = int(props["VirtualQuantity"])
                        if props["AllocationUnits"] == "GigaBytes":
                            vm_memory = vm_memory * 2 ** 30
                        elif props["AllocationUnits"] == "MegaBytes":
                            vm_memory = vm_memory * 2 ** 20
                        elif props["AllocationUnits"] == "KiloBytes":
                            vm_memory = vm_memory * 2 ** 10

        vm_memory_policy = None
        if vm_memory is not None:
            vm_maxmemory = 4 * vm_memory  # 4x memory
            vm_memory_policy = types.MemoryPolicy(
                guaranteed=vm_memory, max=vm_maxmemory
            )

        self.vm = self.vms_service.add(
            types.Vm(
                name=vm_name,
                type=types.VmType(vm_type),
                memory=vm_memory,
                memory_policy=vm_memory_policy,
                cpu=vm_cpu,
                template=types.Template(name=vm_template),
                cluster=types.Cluster(name=cluster_name),
            ),
        )

    def add_nics_to_vm(self, context):
        # Find the network section
        network_elements = self.ovf.get_elements("network_elements")
        bareosfd.DebugMessage(
            context,
            200,
            "add_nics_to_vm(): network_elements: %s\n" % (network_elements),
        )
        for nic in network_elements:
            # Get nic properties:
            props = {}
            for e in nic:
                key = e.tag
                value = e.text
                key = key.replace("{%s}" % self.ovf.OVF_NAMESPACES["rasd"], "")
                props[key] = value

            bareosfd.DebugMessage(
                context, 200, "add_nics_to_vm(): props: %s\n" % (props)
            )

            network = props["Connection"]
            if network not in self.network_profiles:
                bareosfd.JobMessage(
                    context,
                    bJobMessageType["M_WARNING"],
                    "No network profile found for '%s'\n" % (network),
                )
            else:
                profile_id = self.network_profiles[network]

                # Locate the service that manages the network interface cards of the
                # virtual machine:
                nics_service = self.vms_service.vm_service(self.vm.id).nics_service()

                # Use the "add" method of the network interface cards service to add the
                # new network interface card:
                nics_service.add(
                    types.Nic(
                        name=props["Name"],
                        description=props["Caption"],
                        vnic_profile=types.VnicProfile(id=profile_id,),
                    ),
                )
                bareosfd.DebugMessage(
                    context,
                    200,
                    "add_nics_to_vm(): added NIC with props %s\n" % (props),
                )

    def get_vm_disk_by_basename(self, context, fname):

        dirpath = os.path.dirname(fname)
        dirname = os.path.basename(dirpath)
        basename = os.path.basename(fname)
        relname = "%s/%s" % (dirname, basename)

        found = None
        bareosfd.DebugMessage(
            context,
            200,
            "get_vm_disk_by_basename(): %s %s\n" % (basename, self.restore_objects),
        )
        if self.restore_objects is not None:
            i = 0
            while i < len(self.restore_objects) and found is None:
                obj = self.restore_objects[i]
                key = "%s-%s" % (obj["disk-alias"], obj["fileRef"])
                bareosfd.DebugMessage(
                    context,
                    200,
                    "get_vm_disk_by_basename(): lookup %s == %s and %s == %s\n"
                    % (repr(relname), repr(key), repr(basename), repr(obj["diskId"])),
                )
                if relname == key and basename == obj["diskId"]:
                    bareosfd.DebugMessage(
                        context, 200, "get_vm_disk_by_basename(): lookup matched\n"
                    )
                    old_disk_id = os.path.dirname(obj["fileRef"])

                    new_disk_id = None
                    if old_disk_id in self.old_new_ids:
                        bareosfd.DebugMessage(
                            context,
                            200,
                            "get_vm_disk_by_basename(): disk id %s found in old_new_ids: %s\n"
                            % (old_disk_id, self.old_new_ids),
                        )
                        new_disk_id = self.old_new_ids[old_disk_id]

                    # get base disks
                    if not obj["parentRef"]:
                        disk = self.get_or_add_vm_disk(context, obj, new_disk_id)

                        if disk is not None:
                            new_disk_id = disk.id
                            self.old_new_ids[old_disk_id] = new_disk_id
                            found = disk
                    else:
                        bareosfd.JobMessage(
                            context,
                            bJobMessageType["M_WARNING"],
                            "The backup have snapshots and only base will be restored\n",
                        )

                i += 1
        bareosfd.DebugMessage(
            context, 200, "get_vm_disk_by_basename(): found disk %s\n" % (found)
        )
        return found

    def get_or_add_vm_disk(self, context, obj, disk_id=None):
        # Create the disks:
        disks_service = self.system_service.disks_service()

        disk = None
        if "storage_domain" not in obj:
            bareosfd.JobMessage(
                context, bJobMessageType["M_FATAL"], "No storage domain specified.\n"
            )
        else:
            storage_domain = obj["storage_domain"]

            # Find the disk we want to download by the id:
            if disk_id:
                disk_service = disks_service.disk_service(disk_id)
                if disk_service:
                    disk = disk_service.get()

            if not disk:
                # Locate the service that manages the disk attachments of the virtual
                # machine:
                disk_attachments_service = self.vms_service.vm_service(
                    self.vm.id
                ).disk_attachments_service()

                # Determine the volume format
                disk_format = types.DiskFormat.RAW
                if "volume-format" in obj and obj["volume-format"] == "COW":
                    disk_format = types.DiskFormat.COW

                disk_alias = None
                if "disk-alias" in obj:
                    disk_alias = obj["disk-alias"]

                description = None
                if "description" in obj:
                    description = obj["description"]

                size = 0
                if "size" in obj:
                    size = int(obj["size"]) * 2 ** 30

                actual_size = 0
                if "actual_size" in obj:
                    actual_size = int(obj["actual_size"]) * 2 ** 30

                disk_interface = types.DiskInterface.VIRTIO
                if "disk-interface" in obj:
                    if obj["disk-interface"] == "VirtIO_SCSI":
                        disk_interface = types.DiskInterface.VIRTIO_SCSI
                    elif obj["disk-interface"] == "IDE":
                        disk_interface = types.DiskInterface.IDE

                disk_bootable = False
                if "boot" in obj:
                    if obj["boot"] == "true":
                        disk_bootable = True

                #####
                # Use the "add" method of the disk attachments service to add the disk.
                # Note that the size of the disk, the `provisioned_size` attribute, is
                # specified in bytes, so to create a disk of 10 GiB the value should
                # be 10 * 2^30.
                disk_attachment = disk_attachments_service.add(
                    types.DiskAttachment(
                        disk=types.Disk(
                            id=disk_id,
                            name=disk_alias,
                            description=description,
                            format=disk_format,
                            provisioned_size=size,
                            initial_size=actual_size,
                            sparse=disk_format == types.DiskFormat.COW,
                            storage_domains=[types.StorageDomain(name=storage_domain)],
                        ),
                        interface=disk_interface,
                        bootable=disk_bootable,
                        active=True,
                    ),
                )

                # 'Waiting for Disk creation to finish'
                disk_service = disks_service.disk_service(disk_attachment.disk.id)
                while True:
                    time.sleep(5)
                    disk = disk_service.get()
                    if disk.status == types.DiskStatus.OK:
                        time.sleep(5)
                        break

        return disk

    def start_upload(self, context, disk):

        bareosfd.JobMessage(
            context,
            bJobMessageType["M_INFO"],
            "Uploading disk '%s'('%s')\n" % (disk.alias, disk.id),
        )
        bareosfd.DebugMessage(
            context, 100, "Uploading disk '%s'('%s')\n" % (disk.alias, disk.id)
        )
        bareosfd.DebugMessage(context, 200, "old_new_ids: %s\n" % (self.old_new_ids))
        bareosfd.DebugMessage(
            context, 200, "self.restore_objects: %s\n" % (self.restore_objects)
        )

        self.transfer_service = self.get_transfer_service(
            types.ImageTransfer(
                image=types.Image(id=disk.id),
                direction=types.ImageTransferDirection.UPLOAD,
            )
        )
        transfer = self.transfer_service.get()
        proxy_url = urlparse(transfer.proxy_url)
        self.proxy_connection = self.get_proxy_connection(proxy_url)

        # Send the request head
        self.proxy_connection.putrequest("PUT", proxy_url.path)
        self.proxy_connection.putheader("Authorization", transfer.signed_ticket)

        # To prevent from errors on transfer, the exact number of bytes that
        # will be sent must be used, we call it effective_size here. It was
        # saved at backup time in restoreobjects, see start_backup_file(),
        # and restoreobjects are restored before any file.
        new_old_ids = {v: k for k, v in self.old_new_ids.items()}
        old_id = new_old_ids[disk.id]
        effective_size = self.disk_metadata_by_id[old_id]["effective_size"]
        self.init_bytes_to_transf = self.bytes_to_transf = effective_size

        content_range = "bytes %d-%d/%d" % (
            0,
            self.bytes_to_transf - 1,
            self.bytes_to_transf,
        )
        self.proxy_connection.putheader("Content-Range", content_range)
        self.proxy_connection.putheader("Content-Length", "%d" % (self.bytes_to_transf))
        self.proxy_connection.endheaders()

        bareosfd.JobMessage(
            context,
            bJobMessageType["M_INFO"],
            "   Upload disk of %s bytes\n" % (str(self.bytes_to_transf)),
        )
        self.transfer_start_time = time.time()

    def process_ovf(self, context, chunk):
        if self.ovf_data is None:
            self.ovf_data = str(chunk)
        else:
            self.ovf_data = self.ovf_data.str(chunk)

    def process_upload(self, context, chunk):

        bareosfd.DebugMessage(
            context,
            100,
            "process_upload(): transfer %s of %s (%s) \n"
            % (self.bytes_to_transf, self.init_bytes_to_transf, len(chunk)),
        )

        self.proxy_connection.send(chunk)

        self.bytes_to_transf -= len(chunk)

        completed = 1 - (self.bytes_to_transf / float(self.init_bytes_to_transf))

        bareosfd.DebugMessage(
            context, 100, "process_upload(): Completed {:.0%}\n".format(completed)
        )

    def end_transfer(self, context):

        bareosfd.DebugMessage(context, 100, "end_transfer()\n")

        # Finalize the session.
        if self.transfer_service is not None:
            self.transfer_service.finalize()

    def end_vm_backup(self, context):

        if self.snap_service:
            t_start = int(time.time())
            snap = self.snap_service.get()

            # Remove the snapshot:
            snapshot_deleted_success = False

            bareosfd.JobMessage(
                context,
                bJobMessageType["M_INFO"],
                "Sending request to remove snapshot '%s', the id is '%s'.\n"
                % (snap.description, snap.id),
            )

            while True:
                try:
                    self.snap_service.remove()
                except sdk.Error as e:
                    if e.code in [400, 409]:

                        # Fail after defined timeout
                        elapsed = int(time.time()) - t_start
                        if elapsed >= self.snapshot_remove_timeout:
                            bareosfd.JobMessage(
                                context,
                                bJobMessageType["M_WARNING"],
                                "Remove snapshot timed out after %s s, reason: %s! Please remove it manually.\n"
                                % (elapsed, e.message),
                            )
                            return bRCs["bRC_Error"]

                        bareosfd.DebugMessage(
                            context,
                            100,
                            "Could not remove snapshot, reason: %s, retrying until timeout (%s seconds left).\n"
                            % (e.message, self.snapshot_remove_timeout - elapsed),
                        )
                        bareosfd.JobMessage(
                            context,
                            bJobMessageType["M_INFO"],
                            "Still waiting for snapshot removal, retrying until timeout (%s seconds left).\n"
                            % (self.snapshot_remove_timeout - elapsed),
                        )
                    else:
                        bareosfd.JobMessage(
                            context,
                            bJobMessageType["M_WARNING"],
                            "Unexpected error removing snapshot: %s, Please remove it manually.\n"
                            % e.message,
                        )
                        return bRCs["bRC_Error"]

                if self.wait_for_snapshot_removal(snap.id):
                    snapshot_deleted_success = True
                    break
                else:
                    # the snapshot still exists
                    continue

            if snapshot_deleted_success:
                bareosfd.JobMessage(
                    context,
                    bJobMessageType["M_INFO"],
                    "Removed the snapshot '%s'.\n" % snap.description,
                )

            # Send an external event to indicate to the administrator that the
            # backup of the virtual machine is completed:
            self.events_service.add(
                event=types.Event(
                    vm=types.Vm(id=self.vm.id,),
                    origin=APPLICATION_NAME,
                    severity=types.LogSeverity.NORMAL,
                    custom_id=self.event_id,
                    description=(
                        "Backup of virtual machine '%s' using snapshot '%s' is "
                        "completed." % (self.vm.name, snap.description)
                    ),
                ),
            )
            self.event_id += 1

        if self.connection:
            # Close the connection to the server:
            self.connection.close()

    def wait_for_snapshot_removal(self, snapshot_id, timeout=30, delay=10):
        t_start = int(time.time())
        snaps_service = self.vm_service.snapshots_service()

        while True:
            snaps_map = {snap.id: snap.description for snap in snaps_service.list()}

            if snapshot_id not in snaps_map:
                return True

            if int(time.time()) - t_start > timeout:
                return False

            time.sleep(delay)

        return

    def end_vm_restore(self, context):

        # Send an external event to indicate to the administrator that the
        # restore of the virtual machine is completed:
        self.events_service.add(
            event=types.Event(
                vm=types.Vm(id=self.vm.id,),
                origin=APPLICATION_NAME,
                severity=types.LogSeverity.NORMAL,
                custom_id=self.event_id,
                description=(
                    "Restore of virtual machine '%s' is " "completed." % (self.vm.name)
                ),
            ),
        )
        self.event_id += 1

        if self.connection:
            # Close the connection to the server:
            self.connection.close()


class Ovf(object):
    """
    This class is used to encapsulate XPath expressions to extract data from OVF
    """

    # OVF Namespaces
    OVF_NAMESPACES = {
        "ovf": "http://schemas.dmtf.org/ovf/envelope/1/",
        "xsi": "http://www.w3.org/2001/XMLSchema-instance",
        "rasd": "http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_ResourceAllocationSettingData",
    }

    # XPath expressions
    OVF_XPATH_EXPRESSIONS = {
        "vm_name": '/ovf:Envelope/Content[@xsi:type="ovf:VirtualSystem_Type"]/Name',
        "cluster_name": '/ovf:Envelope/Content[@xsi:type="ovf:VirtualSystem_Type"]/ClusterName',
        "network_elements": '/ovf:Envelope/Content[@xsi:type="ovf:VirtualSystem_Type"]/Section[@xsi:type="ovf:VirtualHardwareSection_Type"]/Item[Type="interface"]',
        "disk_elements": '/ovf:Envelope/Section[@xsi:type="ovf:DiskSection_Type"]/Disk',
        "resources_elements": '/ovf:Envelope/Content[@xsi:type="ovf:VirtualSystem_Type"]/Section[@xsi:type="ovf:VirtualHardwareSection_Type"]/Item[rasd:ResourceType=3 or rasd:ResourceType=4]',
    }

    def __init__(self, ovf_data=None):
        """
        Creates a new OVF object

        This method supports the following parameters:

        `ovf_data`:: A string containing the OVF XML data
        """

        # Parse the OVF as XML document:
        self.ovf = lxml.etree.fromstring(ovf_data)

    def get_element(self, element_name):
        """
        Method to extract a single element from OVF data

        This method supports the following parameters:
        `element_name':: A string with the element name to extract
        """
        return self.ovf.xpath(
            self.OVF_XPATH_EXPRESSIONS[element_name], namespaces=self.OVF_NAMESPACES
        )[0].text

    def get_elements(self, element_name):
        """
        Method to extract a list of elements from OVF data

        This method supports the following parameters:
        `element_name':: A string with the element name to extract
        """
        return self.ovf.xpath(
            self.OVF_XPATH_EXPRESSIONS[element_name], namespaces=self.OVF_NAMESPACES
        )


# vim: tabstop=4 shiftwidth=4 softtabstop=4 expandtab
