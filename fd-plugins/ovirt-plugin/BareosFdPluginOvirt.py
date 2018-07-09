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

        self.ovirt = BareosOvirtWrapper()

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

            bareosfd.DebugMessage(
                context, 100,
                "BareosFdPluginOvirt:start_backup_file() backup disk file '%s' of VM '%s'\n" % (disk.alias,self.backup_obj['vmname']))

            savepkt.type = bFileType['FT_REG']
            savepkt.fname = "/VMS/%s-%s/%s-%s" % (self.backup_obj['vmname'],self.backup_obj['vmid'],disk.alias,disk.id)

            try:
                self.ovirt.start_transfer(context, self.backup_obj['snapshot'], disk)
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

    #def create_file(self, context, restorepkt):
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
        #TODO restore to VM

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

        if IOP.func == bIOPS['IO_OPEN']:
            self.FNAME = IOP.fname
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
                try:
                    IOP.buf = self.ovirt.process_transfer(context, IOP.count)
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
            return bRCs['bRC_OK']

    def handle_plugin_event(self, context, event):

        self.jobType = chr(bareosfd.GetValue(context, bVariable['bVarType']))
        bareosfd.DebugMessage(
            context, 100,
            "BareosFdPluginOvirt::handle_plugin_event() jobType: %s\n" %
            (self.jobType))

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
        self.storage_domains_service = None
        self.events_service = None

        self.vm = None
        self.vm_service = None
        self.snap_service = None
        self.backup_objects = None
        self.transfer_service = None
        self.event_id = None

        self.bytes_to_read = None

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

        # Get a reference to the storage domains service:
        self.storage_domains_service = self.system_service.storage_domains_service()

        # Get the reference to the service that we will use to send events to
        # the audit log:
        self.events_service = self.system_service.events_service()

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
            bareosfd.DebugMessage(
                context, 100, "Start the backup of VM %s\n" %
                (self.vm.name))

            # In order to send events we need to also send unique integer ids.
            self.event_id = int(time.time())

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

            # Locate the service that manages the virtual machine:
            self.vm_service = self.vms_service.vm_service(self.vm.id)
            
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
        elif 'vmname' in options:
            search = "name=%s" % str(options['vmname'])
    
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
                    "Waiting till the snapshot is created, the satus is now '%s'.\n" %
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

        # Retrieve the descriptions of the disks of the snapshot:
        snap_disks_service = self.snap_service.disks_service()
        snap_disks = snap_disks_service.list()

        # download disk snaphost
        for snap_disk in snap_disks:
            disk_id = snap_disk.id
            sd_id = snap_disk.storage_domains[0].id

            # Get a reference to the storage domain service in which the disk snapshots reside:
            storage_domain_service = self.storage_domains_service.storage_domain_service(sd_id)

            # Get a reference to the disk snapshots service:
            disk_snapshot_service = storage_domain_service.disk_snapshots_service()

            # Get a list of disk snapshots by a disk ID
            all_disk_snapshots = disk_snapshot_service.list()
        
            # Filter disk snapshots list by disk id
            disk_snapshots = [s for s in all_disk_snapshots if s.disk.id == disk_id and s.snapshot.id == snap.id]
        
            # Download disk snapshot
            if len(disk_snapshots) > 0:
                self.backup_objects.append(
                    {
                        'vmname': self.vm.name,
                        'vmid': self.vm.id,
                        'snapshot': disk_snapshots[0],
                        'disk': snap_disk
                    })
                has_disks = True
        return has_disks

    def get_transfer_service(self, disk_snapshot_id):
        # Get a reference to the service that manages the image transfer:
        transfers_service = self.system_service.image_transfers_service()

        # Add a new image transfer:
        transfer = transfers_service.add(
            types.ImageTransfer(
                snapshot=types.DiskSnapshot(id=disk_snapshot_id),
                direction=types.ImageTransferDirection.DOWNLOAD,
            )
        )

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

    def start_transfer(self, context, snapshot, disk):

        bareosfd.JobMessage(
            context, bJobMessageType['M_INFO'],
                "Downloading snapshot '%s' of disk '%s'('%s')" % (snapshot.id,disk.alias,disk.id))

        self.transfer_service = self.get_transfer_service(snapshot.id)
        transfer = self.transfer_service.get()
        proxy_url = urlparse(transfer.proxy_url)
        proxy_connection = self.get_proxy_connection(proxy_url)

        # Set needed headers for downloading:
        transfer_headers = {
            'Authorization': transfer.signed_ticket,
        }

        # Perform the request.
        proxy_connection.request(
            'GET',
            proxy_url.path,
            headers=transfer_headers,
        )
        # Get response
        self.response = proxy_connection.getresponse()

        # Check the response status:
        if self.response.status >= 300:
            bareosfd.JobMessage(
                context, bJobMessageType['M_ERROR'],
                "Error: %s" % self.response.read())

        self.bytes_to_read = int(self.response.getheader('Content-Length'))

        bareosfd.JobMessage(
            context, bJobMessageType['M_INFO'],
                "   Transfer disk snapshot of %s bytes\n" % (str(self.bytes_to_read)))

    def process_transfer(self,context, chunk_size):
        chunk = "" 

        bareosfd.DebugMessage(
            context, 100,
            "process_transfer(): transfer %s of %s (%s) \n" % 
            (self.bytes_to_read,self.response.getheader('Content-Length'),chunk_size) )

        if self.bytes_to_read > 0:

            # Calculate next chunk to read
            to_read = min(self.bytes_to_read, chunk_size)

            # Read next info buffer
            chunk = self.response.read(to_read)

            if chunk == "":
                bareosfd.DebugMessage(
                    context, 100,
                    "process_transfer(): Socket disconnected. \n" )
                bareosfd.JobMessage(
                    context, bJobMessageType['M_ERROR'],
                    "process_transfer(): Socket disconnected." )

                raise RuntimeError("Socket disconnected")

            # Update bytes_to_read
            self.bytes_to_read -= len(chunk)

            completed = 1 - (self.bytes_to_read / float(self.response.getheader('Content-Length')))

            bareosfd.DebugMessage(
                context, 100,
                "process_transfer(): Completed {:.0%}\n" . format(completed))

        return chunk 

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
                    'Removed the snapshot \'%s\'.' % snap.description)
            
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
