#!/usr/bin/env python

# Bareos python class for Ovirt related backup and restore

import bareosfd
from bareos_fd_consts import bJobMessageType, bFileType, bRCs, bIOPS
from bareos_fd_consts import bEventType, bVariable, bCFs

import os
import io
import sys
import time
import uuid

import lxml.etree

import ovirtsdk4 as sdk
import ovirtsdk4.types as types

import ssl

from httplib import HTTPSConnection

try:
    from urllib.parse import urlparse
except ImportError:
    from urlparse import urlparse

import BareosFdPluginBaseclass
import BareosFdWrapper

# The name of the application, to be used as the 'origin' of events
# sent to the audit log:
APPLICATION_NAME = 'BareOS Ovirt plugin'

# OVF Namespaces
OVF_NAMESPACES = {
    'ovf': 'http://schemas.dmtf.org/ovf/envelope/1/',
    'xsi': 'http://www.w3.org/2001/XMLSchema-instance',
    'rasd': 'http://schemas.dmtf.org/wbem/wscim/1/cim-schema/2/CIM_ResourceAllocationSettingData'
}

class BareosFdPluginOvirt(BareosFdPluginBaseclass.BareosFdPluginBaseclass):
    '''
        Plugin for Ovirt backup and restore
    '''
    def __init__(self, context, plugindef):
        bareosfd.DebugMessage(
            context, 100,
            "Constructor called in module %s with plugindef=%s\n" %
            (__name__, plugindef))
        super(BareosFdPluginOvirt, self).__init__(context, plugindef)

        if self.mandatory_options is None:
            self.mandatory_options = []

        #self.mandatory_options.extend(['vmname','storage_domain'])

        self.ovirt = BareosOvirtWrapper()

    def parse_plugin_definition(self, context, plugindef):
        '''
        We have default options that should work out of the box in the most  use cases
        that the mysql/mariadb is on the same host and can be accessed without user/password information,
        e.g. with a valid my.cnf for user root.
        '''
        super(BareosFdPluginOvirt, self).parse_plugin_definition(context, plugindef)

        bareosfd.DebugMessage(
            context, 100,
            "BareosFdPluginOvirt:parse_plugin_definition() called with options '%s' \n" % str(self.options))
        return bRCs['bRC_OK']

    def check_options(self, context, mandatory_options=None):
        '''
        Check Plugin options
        Note: this is called by parent class parse_plugin_definition(),
        to handle plugin options entered at restore, the real check
        here is done by check_plugin_options() which is called from
        start_backup_job() and start_restore_job()
        '''
        return bRCs['bRC_OK']

    def start_backup_job(self, context):
        '''
        Start of Backup Job. Called just before backup job really start.
        Overload this to arrange whatever you have to do at this time.
        '''
        bareosfd.DebugMessage(
            context, 100,
            "BareosFdPluginOvirt:start_backup_job() called\n")

        if not self.ovirt.connect_api(context, self.options):
            return bRCs['bRC_Error']

        return self.ovirt.prepare_vm_backup(context, self.options)

    def start_backup_file(self, context, savepkt):
        '''
        Defines the file to backup and creates the savepkt.
        '''
        bareosfd.DebugMessage(
            context, 100,
            "BareosFdPluginOvirt:start_backup_file() called\n")

        self.backup_obj = None

        if not self.ovirt.backup_objects:
            bareosfd.JobMessage(
                context, bJobMessageType['M_ERROR'],
                "Nothing to backup.\n")
            return bRCs['bRC_Skip']

        self.backup_obj = self.ovirt.backup_objects.pop(0)

        # create a stat packet for a restore object
        statp = bareosfd.StatPacket()
        savepkt.statp = statp

        if 'file' in self.backup_obj:

            # regular file
            vmfile = self.backup_obj['file']

            bareosfd.DebugMessage(
                context, 100,
                "BareosFdPluginOvirt:start_backup_file() backup regular file '%s' of VM '%s'\n" % (vmfile['filename'],self.backup_obj['vmname']))

            savepkt.type = bFileType['FT_REG']
            savepkt.fname = "/VMS/%s-%s/%s" % (self.backup_obj['vmname'],self.backup_obj['vmid'],vmfile['filename'])
            self.backup_obj['file']['fh'] = io.BytesIO(vmfile['data'])

        elif 'disk' in self.backup_obj:

            # disk file
            disk = self.backup_obj['disk']
            # snapshot
            snapshot = self.backup_obj['snapshot']

            bareosfd.DebugMessage(
                context, 100,
                "BareosFdPluginOvirt:start_backup_file() backup disk file '%s'('%s'/'%s') of VM '%s'\n" % (disk.alias,disk.id,snapshot.id,self.backup_obj['vmname']))

            savepkt.type = bFileType['FT_REG']
            savepkt.fname = "/VMS/%s-%s/%s-%s/%s" % (self.backup_obj['vmname'],self.backup_obj['vmid'],disk.alias,disk.id,snapshot.id)

            try:
                self.ovirt.start_download(context, snapshot, disk)
            except Exception as e:
                bareosfd.JobMessage(
                    context, bJobMessageType['M_ERROR'],
                    "BareosFdPluginOvirt:start_backup_file() Error: %s" % str(e))
                self.ovirt.end_transfer(context)
                return bRCs['bRC_Error']
        else:
            return bRCs['bRC_Error']
    
        bareosfd.JobMessage(
            context, bJobMessageType['M_INFO'],
            "Starting backup of %s\n" %
            savepkt.fname)

        return bRCs['bRC_OK']

    def create_file(self, context, restorepkt):
        '''
        Creates the file to be restored and directory structure, if needed.
        Adapt this in your derived class, if you need modifications for
        virtual files or similar
        '''
        bareosfd.DebugMessage(
            context, 100,
            "create_file() entry point in Python called with %s\n" %
            (restorepkt))

        if self.options.get('local') == 'yes':
            FNAME = restorepkt.ofname
            dirname = os.path.dirname(FNAME)
            if not os.path.exists(dirname):
                bareosfd.DebugMessage(
                    context, 200,
                    "Directory %s does not exist, creating it now\n" %
                    dirname)
                os.makedirs(dirname)
            # open creates the file, if not yet existing, we close it again right
            # aways it will be opened again in plugin_io.
            # But: only do this for regular files, prevent from
            # IOError: (21, 'Is a directory', '/tmp/bareos-restores/my/dir/')
            # if it's a directory
            if restorepkt.type == bFileType['FT_REG']:
                open(FNAME, 'wb').close()
        else:
            if not restorepkt.ofname.endswith('.ovf'):
                FNAME = restorepkt.ofname

                disk = self.ovirt.get_vm_disk_by_basename(context, FNAME)
                if disk is None:
                    bareosfd.JobMessage(
                        context, bJobMessageType['M_ERROR'],
                        "BareosFdPluginOvirt:create_file() Unable to restore disk %s.\n" % (FNAME))
                    return bRCs['bRC_Error']
                else:
                    self.ovirt.start_upload(context, disk)

        if restorepkt.type == bFileType['FT_REG']:
            restorepkt.create_status = bCFs['CF_EXTRACT']
        return bRCs['bRC_OK']

    def start_restore_job(self, context):
        '''
        Start of Restore Job. Called , if you have Restore objects.
        Overload this to handle restore objects, if applicable
        '''
        bareosfd.DebugMessage(
            context, 100,
            "BareosFdPluginOvirt:start_restore_job() called\n")
        if self.options.get('local') == 'yes':
            bareosfd.DebugMessage(
                context, 100, "BareosFdPluginOvirt:start_restore_job(): restore to local file, skipping checks\n")
            return bRCs['bRC_OK']
        else:
            # restore to VM to OVirt
            if not self.ovirt.connect_api(context, self.options):
                return bRCs['bRC_Error']

        return bRCs['bRC_OK']

    def start_restore_file(self, context, cmd):
        bareosfd.DebugMessage(
            context, 100,
            "BareosFdPluginOvirt:start_restore_file() called with %s\n" %
            (cmd))
        return bRCs['bRC_OK']

    def plugin_io(self, context, IOP):
        '''
        Called for io operations.
        '''
        bareosfd.DebugMessage(
            context, 100,
            "plugin_io called with function %s\n" %
            (IOP.func))
        bareosfd.DebugMessage(
            context, 100,
            "FNAME is set to %s\n" %
            (self.FNAME))
        self.jobType = chr(bareosfd.GetValue(context, bVariable['bVarType']))
        bareosfd.DebugMessage(
            context, 100,
            "BareosFdPluginOvirt::plugin_io() jobType: %s\n" %
            (self.jobType))

        if IOP.func == bIOPS['IO_OPEN']:

            self.FNAME = IOP.fname
            bareosfd.DebugMessage(
                context, 100,
                "self.FNAME was set to %s from IOP.fname\n" %
                (self.FNAME))

            if self.options.get('local') == 'yes':
                try:
                    if IOP.flags & (os.O_CREAT | os.O_WRONLY):
                        bareosfd.DebugMessage(
                            context, 100,
                            "Open file %s for writing with %s\n" %
                            (self.FNAME, IOP))

                        dirname = os.path.dirname(self.FNAME)
                        if not os.path.exists(dirname):
                            bareosfd.DebugMessage(
                                context, 100,
                                "Directory %s does not exist, creating it now\n" %
                                (dirname))
                            os.makedirs(dirname)
                        self.file = open(self.FNAME, 'wb')
                    # else:
                    #     bareosfd.DebugMessage(
                    #         context, 100,
                    #         "Open file %s for reading with %s\n" %
                    #         (self.FNAME, IOP))
                    #     self.file = open(self.FNAME, 'rb')
                except:
                    IOP.status = -1
                    return bRCs['bRC_Error']

            return bRCs['bRC_OK']

        elif IOP.func == bIOPS['IO_CLOSE']:
            if self.file is not None:
                bareosfd.DebugMessage(
                    context, 100,
                    "Closing file " + "\n")
                self.file.close()
            elif self.jobType == 'B':
                if 'file' in self.backup_obj:
                    self.backup_obj['file']['fh'].close()
                elif 'disk' in self.backup_obj:
                    bareosfd.DebugMessage(
                        context, 100,
                        "plugin_io: calling end_transfer()\n" )
                    # Backup Job
                    self.ovirt.end_transfer(context)
            elif self.jobType == 'R':
                if self.FNAME.endswith('.ovf'):
                    return self.ovirt.prepare_vm_restore(context,self.options)
                else:
                    self.ovirt.end_transfer(context)
            return bRCs['bRC_OK']
        elif IOP.func == bIOPS['IO_SEEK']:
            return bRCs['bRC_OK']
        elif IOP.func == bIOPS['IO_READ']:
            if 'file' in self.backup_obj:
                IOP.buf = bytearray(IOP.count)
                IOP.status = self.backup_obj['file']['fh'].readinto(IOP.buf)
                IOP.io_errno = 0
                bareosfd.DebugMessage(
                    context, 100,
                    "plugin_io: read from file %s\n" % (self.backup_obj['file']['filename']) )
            elif 'disk' in self.backup_obj:
                bareosfd.DebugMessage(
                    context, 100,
                    "plugin_io: read from disk %s\n" % (self.backup_obj['disk'].alias) )
                try:
                    IOP.buf = bytearray(IOP.count)
                    chunk = self.ovirt.process_download(context, IOP.count)
                    IOP.buf[:] = chunk
                    IOP.status = len(IOP.buf)
                    IOP.io_errno = 0
                except Exception as e:
                    bareosfd.JobMessage(
                        context, bJobMessageType['M_ERROR'],
                        "BareosFdPluginOvirt:plugin_io() Error: %s" % str(e))
                    self.ovirt.end_transfer(context)
                    return bRCs['bRC_Error']
            else:
                bareosfd.JobMessage(
                    context, bJobMessageType['M_ERROR'],
                    "BareosFdPluginOvirt:plugin_io() Unable to read data to backup.")
                return bRCs['bRC_Error']
            return bRCs['bRC_OK']
        elif IOP.func == bIOPS['IO_WRITE']:
            if self.file is not None:
                try:
                    bareosfd.DebugMessage(
                        context, 200,
                        "Writing buffer to file %s\n" %
                        (self.FNAME))
                    self.file.write(IOP.buf)
                    IOP.status = IOP.count
                    IOP.io_errno = 0
                except IOError as msg:
                    IOP.io_errno = -1
                    bareosfd.DebugMessage(context, 100, "Error writing data: " + msg + "\n");
                    return bRCs['bRC_Error']
            elif self.FNAME.endswith('.ovf'):
                self.ovirt.process_ovf(context,IOP.buf)
                IOP.status = IOP.count
                IOP.io_errno = 0
            else:
                self.ovirt.process_upload(context,IOP.buf)
                IOP.status = IOP.count
                IOP.io_errno = 0
            return bRCs['bRC_OK']

    def handle_plugin_event(self, context, event):

        if event == bEventType['bEventEndBackupJob']:
            bareosfd.DebugMessage(
                context, 100,
                "BareosFdPluginOvirt::handle_plugin_event() called with bEventEndBackupJob\n")
            bareosfd.DebugMessage(
                context, 100,
                "removing Snapshot\n")
            self.ovirt.end_vm_backup(context)

        elif event == bEventType['bEventEndFileSet']:
            bareosfd.DebugMessage(
                context, 100,
                "BareosFdPluginOvirt::handle_plugin_event() called with bEventEndFileSet\n")

        elif event == bEventType['bEventStartBackupJob']:
            bareosfd.DebugMessage(
                context, 100,
                "BareosFdPluginOvirt::handle_plugin_event() called with bEventStartBackupJob\n")

            return self.start_backup_job(context)

        elif event == bEventType['bEventStartRestoreJob']:
            bareosfd.DebugMessage(
                context, 100,
                "BareosFdPluginOvirt::handle_plugin_event() called with bEventStartRestoreJob\n")

            return self.start_restore_job(context)

        elif event == bEventType['bEventEndRestoreJob']:
            bareosfd.DebugMessage(
                context, 100,
                "BareosFdPluginOvirt::handle_plugin_event() called with bEventEndRestoreJob\n")
            bareosfd.DebugMessage(
                context, 100,
                "removing Snapshot\n")
            self.ovirt.end_vm_restore(context)
        else:
            bareosfd.DebugMessage(
                context, 100,
                "BareosFdPluginOvirt::handle_plugin_event() called with event %s\n" %
                (event))

        return bRCs['bRC_OK']

    def end_backup_file(self, context):
        if self.ovirt.backup_objects:
            return bRCs['bRC_More']
        else:
            return bRCs['bRC_OK']

    #def end_restore_file(self, context):
    #def restore_object_data(self, context, ROP):

class BareosOvirtWrapper(object):
    '''
    Ovirt wrapper class
    '''
    def __init__(self):

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

        self.backup_objects = None
        self.restore_objects = None

	self.old_new_ids = {}

    def connect_api(self, context, options):

        # ca certificate
        self.ca = options['ca']

        # set server url
        server_url = "https://%s/ovirt-engine/api" % str(options['server'])

        # Create a connection to the server:
        self.connection = sdk.Connection( url=server_url,
                                          username=options['username'],
                                          password=options['password'],
                                          ca_file=self.ca)

        if not self.connection:
            bareosfd.JobMessage(
                context, bJobMessageType['M_FATAL'],
                "Cannot connect to host %s with user %s and ca file %s\n" %
                (options['server'], options['username'],self.ca))
            return False

        # Get a reference to the root service:
        self.system_service = self.connection.system_service()

        # Get the reference to the "vms" service:
        self.vms_service = self.system_service.vms_service()

        if not self.vms_service:
            return False

        # get network profiles
        profiles_service = self.system_service.vnic_profiles_service()
        self.network_profiles = { profile.name : profile.id for profile in profiles_service.list() }

        # Get the reference to the service that we will use to send events to
        # the audit log:
        self.events_service = self.system_service.events_service()

        # In order to send events we need to also send unique integer ids.
        self.event_id = int(time.time())

        bareosfd.DebugMessage(
            context, 100,
            ("Successfully connected to Ovirt Engine API on '%s' with"
             " user %s and ca file %s\n") %
            (options['server'], options['username'],self.ca))

        return True

    def prepare_vm_backup(self, context, options):
        '''
        prepare VM backup:
        - get vm details
        - take snapshot
        - get disk devices
        '''
        bareosfd.DebugMessage(
            context, 100, "BareosOvirtWrapper::prepare_vm_backup: prepare VM to backup\n")

        if not self.get_vm(context, options):
            bareosfd.DebugMessage(
                context, 100, "Error getting details for VM\n")

            return bRCs['bRC_Error']
        else: 
            # Locate the service that manages the virtual machine:
            self.vm_service = self.vms_service.vm_service(self.vm.id)

            # check if vm have snapshosts
            snaps_service = self.vm_service.snapshots_service()
            if len(snaps_service.list()) > 1:
                bareosfd.JobMessage(
                    context, bJobMessageType['M_FATAL'],
                    "Error '%s' already have snapshosts. This is not supported\n" %
                    (self.vm.name))
                return bRCs['bRC_Error']
 
            bareosfd.DebugMessage(
                context, 100, "Start the backup of VM %s\n" %
                (self.vm.name))

            # Save the OVF to a file, so that we can use to restore the virtual
            # machine later. The name of the file is the name of the virtual
            # machine, followed by a dash and the identifier of the virtual machine,
            # to make it unique:
            ovf_data = self.vm.initialization.configuration.data
            ovf_file = '%s-%s.ovf' % (self.vm.name, self.vm.id)
            if self.backup_objects is None:
                self.backup_objects = []

            self.backup_objects.append(
                {
                    'vmname': self.vm.name,
                    'vmid': self.vm.id,
                    'file': { 
                        'data': ovf_data,
                        'filename': ovf_file
                    }
                })

            # create vm snapshots
            self.create_vm_snapshot(context)

            # get vm backup disks from snapshot
            if not self.get_vm_backup_disks(context):
                bareosfd.JobMessage(
                    context, bJobMessageType['M_FATAL'],
                    "Error getting Backup Disks VM %s from snapshot\n" %
                    (self.vm.name))
                return bRCs['bRC_Error']
  
            return bRCs['bRC_OK']

    def get_vm(self, context, options):
        search = None
        if 'uuid' in options:
            search = "uuid=%s" % str(options['uuid'])
        elif 'vm_name' in options:
            search = "name=%s" % str(options['vm_name'])
    
        if search is not None:
            bareosfd.DebugMessage(
                context, 100, "get_vm search vm by '%s'\n" %
                (search))
            res = self.vms_service.list(search=search, all_content=True)
            if len(res) > 0:
                self.vm = res[0];
                return True
        return False

    def create_vm_snapshot(self, context):
        '''
        Creates a snapshot
        '''

        # Create an unique description for the snapshot, so that it is easier
        # for the administrator to identify this snapshot as a temporary one
        # created just for backup purposes:
        snap_description = '%s-backup-%s' % (self.vm.name, uuid.uuid4())
        
        # Send an external event to indicate to the administrator that the
        # backup of the virtual machine is starting. Note that the description
        # of the event contains the name of the virtual machine and the name of
        # the temporary snapshot, this way, if something fails, the administrator
        # will know what snapshot was used and remove it manually.
        self.events_service.add(
            event=types.Event(
                vm=types.Vm(
                  id=self.vm.id,
                ),
                origin=APPLICATION_NAME,
                severity=types.LogSeverity.NORMAL,
                custom_id=self.event_id,
                description=(
                    'Backup of virtual machine \'%s\' using snapshot \'%s\' is '
                    'starting.' % (self.vm.name, snap_description)
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
                description=snap_description,
                persist_memorystate=False,
            ),
        )
        bareosfd.JobMessage(
            context, bJobMessageType['M_INFO'],
                "Sent request to create snapshot '%s', the id is '%s'.\n" %
                (snap.description, snap.id))

        # Poll and wait till the status of the snapshot is 'ok', which means
        # that it is completely created:
        self.snap_service = snaps_service.snapshot_service(snap.id)
        while snap.snapshot_status != types.SnapshotStatus.OK:
            bareosfd.DebugMessage(
                context, 100,
                    "Waiting till the snapshot is created, the status is now '%s'.\n" %
                    snap.snapshot_status)
            time.sleep(1)
            snap = self.snap_service.get()
            time.sleep(1)

        # wait some time until snapshot real complete
        time.sleep(10)

        bareosfd.JobMessage(
            context, bJobMessageType['M_INFO'],
                "'  The snapshot is now complete.\n")

    def get_vm_backup_disks(self, context):

        has_disks = False

        if self.backup_objects is None:
            self.backup_objects = []

        # get snapshot
        snap = self.snap_service.get()

        # Get a reference to the storage domains service:
        storage_domains_service = self.system_service.storage_domains_service()

        # Retrieve the descriptions of the disks of the snapshot:
        snap_disks_service = self.snap_service.disks_service()
        snap_disks = snap_disks_service.list()

        # download disk snaphost
        for snap_disk in snap_disks:
            disk_id = snap_disk.id
            sd_id = snap_disk.storage_domains[0].id

            # Get a reference to the storage domain service in which the disk snapshots reside:
            storage_domain_service = storage_domains_service.storage_domain_service(sd_id)

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
                            'vmname': self.vm.name,
                            'vmid': self.vm.id,
                            'snapshot': disk_snapshot,
                            'disk': snap_disk
                        })
                has_disks = True
        return has_disks

    def get_transfer_service(self, image_transfer):
        # Get a reference to the service that manages the image transfer:
        transfers_service = self.system_service.image_transfers_service()

        # Add a new image transfer:
        transfer = transfers_service.add( image_transfer )

        # Get reference to the created transfer service:
        transfer_service = transfers_service.image_transfer_service(transfer.id)

        while transfer.phase == types.ImageTransferPhase.INITIALIZING:
            time.sleep(1)
            transfer = transfer_service.get()

        return transfer_service

    def get_proxy_connection(self,proxy_url):
        # At this stage, the SDK granted the permission to start transferring the disk, and the
        # user should choose its preferred tool for doing it - regardless of the SDK.
        # In this example, we will use Python's httplib.HTTPSConnection for transferring the data.
        context = ssl.create_default_context()

        # Note that ovirt-imageio-proxy by default checks the certificates, so if you don't have
        # your CA certificate of the engine in the system, you need to pass it to HTTPSConnection.
        context.load_verify_locations(cafile=self.ca)

        return HTTPSConnection(
            proxy_url.hostname,
            proxy_url.port,
            context=context,
        )

    def start_download(self, context, snapshot, disk):

        bareosfd.JobMessage(
            context, bJobMessageType['M_INFO'],
                "Downloading snapshot '%s' of disk '%s'('%s')\n" % (snapshot.id,disk.alias,disk.id))

        self.transfer_service = self.get_transfer_service( types.ImageTransfer(
                                                                snapshot=types.DiskSnapshot(id=snapshot.id),
                                                                direction=types.ImageTransferDirection.DOWNLOAD ))
        transfer = self.transfer_service.get()
        proxy_url = urlparse(transfer.proxy_url)
        self.proxy_connection = self.get_proxy_connection(proxy_url)

        # Set needed headers for downloading:
        transfer_headers = {
            'Authorization': transfer.signed_ticket,
        }

        # Perform the request.
        self.proxy_connection.request(
            'GET',
            proxy_url.path,
            headers=transfer_headers,
        )
        # Get response
        self.response = self.proxy_connection.getresponse()

        # Check the response status:
        if self.response.status >= 300:
            bareosfd.JobMessage(
                context, bJobMessageType['M_ERROR'],
                "Error: %s" % self.response.read())

        self.bytes_to_transf = int(self.response.getheader('Content-Length'))

        bareosfd.JobMessage(
            context, bJobMessageType['M_INFO'],
                "   Transfer disk snapshot of %s bytes\n" % (str(self.bytes_to_transf)))

    def process_download(self,context, chunk_size):

        chunk = ""

        bareosfd.DebugMessage(
            context, 100,
            "process_download(): transfer %s of %s (%s) \n" % 
            (self.bytes_to_transf,self.response.getheader('Content-Length'),chunk_size) )

        if self.bytes_to_transf > 0:

            # Calculate next chunk to read
            to_read = min(self.bytes_to_transf, chunk_size)

            # Read next info buffer
            chunk = self.response.read(to_read)

            if chunk == "":
                bareosfd.DebugMessage(
                    context, 100,
                    "process_download(): Socket disconnected. \n" )
                bareosfd.JobMessage(
                    context, bJobMessageType['M_ERROR'],
                    "process_download(): Socket disconnected." )

                raise RuntimeError("Socket disconnected")

            # Update bytes_to_transf
            self.bytes_to_transf -= len(chunk)

            completed = 1 - (self.bytes_to_transf / float(self.response.getheader('Content-Length')))

            bareosfd.DebugMessage(
                context, 100,
                "process_download(): Completed {:.0%}\n" . format(completed))

        return chunk 

    def prepare_vm_restore(self, context, options):
        if self.connection is None:
            # if not connected yet
            if not self.connect_api(context, options):
                return bRCs['bRC_Error']

        if self.ovf_data is None:
            bareosfd.JobMessage(
                context, bJobMessageType['M_FATAL'],
                "Unable to restore VM. No OVF data. \n" )
            return bRCs['bRC_Error']
        else:
            if 'storage_domain' not in options:
                bareosfd.JobMessage(
                    context, bJobMessageType['M_FATAL'],
                    "No storage domain specified.\n" )
                return bRCs['bRC_Error']

            storage_domain = options['storage_domain']

            # Parse the OVF as XML document:
            self.ovf = lxml.etree.fromstring(self.ovf_data)

            bareosfd.DebugMessage(context, 200, "prepare_vm_restore(): %s\n" % (self.ovf))

            # Find the name of the virtual machine within the OVF:
            vm_name = None
	    if 'vm_name' in options:
                vm_name = options['vm_name']

            if vm_name is None:
		vm_name = self.ovf.xpath(
		    '/ovf:Envelope/Content[@xsi:type="ovf:VirtualSystem_Type"]/Name',
		    namespaces=OVF_NAMESPACES
		)[0].text

            # Find the cluster name of the virtual machine within the OVF:
	    cluster_name = None
            if 'cluster_name' in options:
		cluster_name = options['cluster_name']
	
	    if cluster_name is None:
		# Find the cluster name of the virtual machine within the OVF:
		cluster_name = self.ovf.xpath(
		    '/ovf:Envelope/Content[@xsi:type="ovf:VirtualSystem_Type"]/ClusterName',
		    namespaces=OVF_NAMESPACES
		)[0].text

	    # check if VM exists
	    res = self.vms_service.list(search="name=%s and cluster=%s" % (str(vm_name),str(cluster_name)))
	    if len(res) > 0:
                bareosfd.JobMessage(
                    context, bJobMessageType['M_FATAL'],
                    "VM '%s' exists. Unable to restore.\n" % str(vm_name) )
                return bRCs['bRC_Error']
	    else:

		vm_template = 'Blank'
		if 'vm_template' in options:
		    vm_template = options['vm_template']

		# Add the virtual machine, the transfered disks will be
		# attached to this virtual machine:
		bareosfd.JobMessage(
		    context, bJobMessageType['M_INFO'],
			'Adding virtual machine %s\n' % vm_name)

                vm_type = 'server'
		if 'vm_type' in options:
		    vm_type = options['vm_type']

		self.vm = self.vms_service.add(
		    types.Vm(
			name = vm_name,
                        type = types.VmType( vm_type ),
			template=types.Template(
			    name=vm_template
                        ),
			cluster=types.Cluster(
			    name=cluster_name
                        ),
		    ),
		)

                # Find the network section
                network_elements = ovf.xpath(
                        '/ovf:Envelope/Content[@xsi:type="ovf:VirtualSystem_Type"]/Section[@xsi:type="ovf:VirtualHardwareSection_Type"]/Item[Type="interface"]',
                    namespaces=OVF_NAMESPACES
                )
                for nic in network_elements:
                    # Get nic properties:
                    props = {}
                    for e in nic:
                        key = e.tag
                        value = e.text
                        key = key.replace('{%s}' % OVF_NAMESPACES['rasd'], '')
                        props[key] = value

                    network = props['Connection']
                    if network not in network_profiles:
			bareosfd.JobMessage(
			    context, bJobMessageType['M_INFO'],
				"No network profile found for '%s'\n" % (network) )
                    else:
                        profile_id = network_profiles[network]

                        # Locate the service that manages the network interface cards of the
                        # virtual machine:
                        nics_service = bOvirt.vms_service.vm_service(vm.id).nics_service()

                        # Use the "add" method of the network interface cards service to add the
                        # new network interface card:
                        nics_service.add(
                            types.Nic(
                                name=props['Name'],
                                description=props['Caption'],
                                vnic_profile=types.VnicProfile(
                                    id=profile_id,
                                ),
                            ),
                        )

		# Find the disks section:
		disk_elements = self.ovf.xpath(
		    '/ovf:Envelope/Section[@xsi:type="ovf:DiskSection_Type"]/Disk',
		    namespaces=OVF_NAMESPACES
		)

		if self.restore_objects is None:
		    self.restore_objects = []

		for disk_element in disk_elements:
		    # Get disk properties:
		    props = {}
		    for key, value in disk_element.items():
			key = key.replace('{%s}' % OVF_NAMESPACES['ovf'], '')
			props[key] = value

                    # set storage domain
                    props['storage_domain'] = storage_domain
		    self.restore_objects.append(props)

        return bRCs['bRC_OK']

    def get_vm_disk_by_basename(self, context, fname):

        dirpath = os.path.dirname(fname)
        dirname = os.path.basename(dirpath)
        basename = os.path.basename(fname)
        relname = "%s/%s" % (dirname,basename)

        found = None
        bareosfd.DebugMessage(context, 200, "get_vm_disk_by_basename(): %s %s\n" % (basename,self.restore_objects))
        if self.restore_objects is not None:
            i = 0
            while i<len(self.restore_objects) and found is None:
                obj = self.restore_objects[i]
                key = "%s-%s" % (obj['disk-alias'],obj['fileRef'])
                bareosfd.DebugMessage(context, 200, "get_vm_disk_by_basename(): lookup %s == %s\n" % (basename,key))
                if relname == key and basename == obj['diskId']:
                    old_disk_id = os.path.dirname(obj['fileRef'])

		    new_disk_id = None
		    if old_disk_id in self.old_new_ids:
			new_disk_id = self.old_new_ids[old_disk_id]

		    # get base disks
		    if not obj['parentRef']:
			disk = self.get_or_add_vm_disk(context, obj, new_disk_id)

			if disk is not None:
			    new_disk_id = disk.id
			    self.old_new_ids[old_disk_id] = new_disk_id
			    found = disk
		    else:
			bareosfd.JobMessage(
			    context, bJobMessageType['M_INFO'],
				"The backup have snapshots and only base will be restored\n")
			
                i += 1
        bareosfd.DebugMessage(context, 200, "get_vm_disk_by_basename(): found disk %s\n" % (found))
        return found 

    def get_or_add_vm_disk(self,context, obj, disk_id = None):
        # Create the disks:
        disks_service = self.system_service.disks_service()

        disk = None
        if 'storage_domain' not in obj:
            bareosfd.JobMessage(
                context, bJobMessageType['M_FATAL'],
                "No storage domain specified.\n" )
        else:
            storage_domain = obj['storage_domain']

            # Find the disk we want to download by the id:
            if disk_id:
                disk_service = disks_service.disk_service(disk_id)
                if disk_service:
                    disk = disk_service.get()

            if not disk:
                # Locate the service that manages the disk attachments of the virtual
                # machine:
                disk_attachments_service = self.vms_service.vm_service(self.vm.id).disk_attachments_service()

                # Determine the volume format
                disk_format = types.DiskFormat.RAW
		if 'volume-format' in obj and obj['volume-format'] == 'COW':
		    disk_format = types.DiskFormat.COW

                disk_alias = None
		if 'disk-alias' in obj:
		    disk_alias = obj['disk-alias']

                description = None
		if 'description' in obj:
		    description = obj['description']

                size = 0
		if 'size' in obj:
		    size = int(obj['size']) * 2**30

                actual_size = 0
		if 'actual-size' in obj:
		    actual_size = int(obj['actual-size']) * 2**30

                disk_interface = types.DiskInterface.VIRTIO
		if 'disk-interface' in obj:
		    if obj['disk-interface'] == 'VirtIO_SCSI':
			disk_interface = types.DiskInterface.VIRTIO_SCSI
		    elif obj['disk-interface'] == 'IDE':
			disk_interface = types.DiskInterface.IDE

                disk_bootable = False
                if 'boot' in obj:
                    if obj['boot'] == "true":
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
			    storage_domains=[
				types.StorageDomain(
				    name=storage_domain
				)
			    ]
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
            context, bJobMessageType['M_INFO'],
                "Uploading disk '%s'('%s')\n" % (disk.alias,disk.id))

        self.transfer_service = self.get_transfer_service( types.ImageTransfer( image = types.Image( id = disk.id ),
										direction = types.ImageTransferDirection.UPLOAD ))
        transfer = self.transfer_service.get()
        proxy_url = urlparse(transfer.proxy_url)
        self.proxy_connection = self.get_proxy_connection(proxy_url)

        # Send the request head
        self.proxy_connection.putrequest("PUT", proxy_url.path)
        self.proxy_connection.putheader('Authorization', transfer.signed_ticket)

        #self.bytes_to_transf = int(disk.actual_size) * 2**30
        self.bytes_to_transf = int(disk.actual_size)

        content_range = "bytes %d-%d/%d" % (0, self.bytes_to_transf - 1, self.bytes_to_transf)
        self.proxy_connection.putheader('Content-Range', content_range)
        self.proxy_connection.putheader('Content-Length', "%d" % (self.bytes_to_transf))
        self.proxy_connection.endheaders()

        bareosfd.JobMessage(
            context, bJobMessageType['M_INFO'],
                "   Upload disk of %s bytes\n" % (str(self.bytes_to_transf)))

    def process_ovf(self,context, chunk):
        if self.ovf_data is None:
            self.ovf_data = str(chunk)
        else:
            self.ovf_data = self.ovf_data . str(chunk)

    def process_upload(self,context, chunk):

        bareosfd.DebugMessage(
            context, 100,
            "process_upload(): transfer %s of %s \n" % 
            (self.bytes_to_transf,len(chunk)) )

        self.proxy_connection.send(chunk)

        self.bytes_to_transf -= len(chunk)

        #completed = 1 - (self.bytes_to_transf / float(self.response.getheader('Content-Length')))

        #bareosfd.DebugMessage(
        #    context, 100,
        #    "process_upload(): Completed {:.0%}\n" . format(completed))
   
    def end_transfer(self,context):

        bareosfd.DebugMessage(
            context, 100,
            "end_transfer()\n" )

	# Finalize the session.
        if self.transfer_service is not None:
            self.transfer_service.finalize()

    def end_vm_backup(self, context):

        if self.snap_service:
            snap = self.snap_service.get()

            # Remove the snapshot:
            self.snap_service.remove()
            bareosfd.JobMessage(
                context, bJobMessageType['M_INFO'],
                    'Removed the snapshot \'%s\'.\n' % snap.description)
            
            # Send an external event to indicate to the administrator that the
            # backup of the virtual machine is completed:
            self.events_service.add(
                event=types.Event(
                    vm=types.Vm(
                      id=self.vm.id,
                    ),
                    origin=APPLICATION_NAME,
                    severity=types.LogSeverity.NORMAL,
                    custom_id=self.event_id,
                    description=(
                        'Backup of virtual machine \'%s\' using snapshot \'%s\' is '
                        'completed.' % (self.vm.name, snap.description)
                    ),
                ),
            )
            self.event_id += 1

        if self.connection:
            # Close the connection to the server:
            self.connection.close()

    def end_vm_restore(self, context):

	# Send an external event to indicate to the administrator that the
	# restore of the virtual machine is completed:
	self.events_service.add(
	    event=types.Event(
		vm=types.Vm(
		  id=self.vm.id,
		),
		origin=APPLICATION_NAME,
		severity=types.LogSeverity.NORMAL,
		custom_id=self.event_id,
		description=(
		    'Restore of virtual machine \'%s\' is '
		    'completed.' % (self.vm.name)
		),
	    ),
	)
	self.event_id += 1

        if self.connection:
            # Close the connection to the server:
            self.connection.close()
