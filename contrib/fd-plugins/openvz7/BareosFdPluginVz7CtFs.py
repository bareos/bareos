#!/usr/bin/env python
# -*- coding: utf-8 -*-
import os
import stat
import glob
import re
import uuid
import subprocess
import bareosfd
import BareosFdPluginBaseclass
from lockfile import LockFile
import collections


class BareosFdPluginVz7CtFs(BareosFdPluginBaseclass.BareosFdPluginBaseclass):
    '''
    Bareos FD Plugin to backup Virtuozzo container filesystems and metadata
    '''

    def __init__(self, plugindef):
        bareosfd.DebugMessage(
            100,
            "Constructor called in module %s with plugindef=%s\n" %
            (__name__, plugindef))
        events = []
        events.append(bareosfd.bEventEndRestoreJob)
        bareosfd.RegisterEvents(events)
        # Last argument of super constructor is a list of mandatory arguments
        # using mandatory options in python constructor is not working cause options are merged inside plugin_option_parser
        # and not on directzor side
        super(BareosFdPluginVz7CtFs, self).__init__(plugindef)
        self.files = collections.deque()
        # Filled during start_backup_job
        self.cnf_default_excludes = []
        self.excluded_backup_paths = []
        self.prepared = False
        self.disk_descriptor = ""
        self.config_path = ""
        self.fs_path = ""
        self.mount_basedir = "/mnt/bareos"
        self.job_mount_point = ""
        self.base_image = []
        self.verbose = False
        self.snapshot_uuid = ""
        self.snapshot_created = False
        self.blocker = ""
        self.mounted = False
        # default is the restore of a whole container
        self.restore = "ct"

    def handle_plugin_event(self, event):
        '''
        call appropiate function for triggered event
        '''
        if event == bareosfd.bEventEndRestoreJob:
            bareosfd.DebugMessage(
                100,
                "handle_plugin_event called with bareosfd.bEventEndRestoreJob\n")
            return self.end_restore_job()
        elif event == bareosfd.bEventEndBackupJob:
            bareosfd.DebugMessage(
                100,
                "handle_plugin_event called with bareosfd.bEventEndBackupJob\n")
            return self.end_backup_job()
        elif event == bareosfd.bEventStartBackupJob:
            bareosfd.DebugMessage(
                100, "handle_plugin_event() called with bareosfd.bEventStartBackupJob\n")
            return self.start_backup_job()
        elif event == bareosfd.bEventStartRestoreJob:
            bareosfd.DebugMessage(
                100,
                "handle_plugin_event() called with bareosfd.bEventStartRestoreJob\n")
            return self.start_restore_job()
        else:
            bareosfd.DebugMessage(
                100,
                "handle_plugin_event called with noop event %s\n" % (event))
        return bareosfd.bRC_OK

    def check_plugin_options(self):
        '''
        Checks uuid and container name in options, sets some variables
        '''
        if 'name' not in self.options or 'uuid' not in self.options:
            return False
        if 'lockfile' in self.options:
            self.lock = LockFile(self.options['lockfile'])
            self.lock.acquire()
        if 'blocker' in self.options:
            self.blocker = self.options['blocker']
        if not os.path.exists(self.mount_basedir):
            os.mkdir(self.mount_basedir)
        if 'restore' in self.options:
            self.restore = self.options['restore']
        self.job_mount_point = os.path.join(
            self.mount_basedir, self.options['uuid'])
        self.config_path = os.path.join("/vz/private", self.options['uuid'])
        self.disk_descriptor = os.path.join(
            self.config_path, "root.hdd", "DiskDescriptor.xml")
        self.fs_path = os.path.join("/vz/root", self.options['uuid'])
        if 'excluded_backup_paths' in self.options:
            relative_excludes = self.options['excluded_backup_paths'].replace("'","").split(",")
            excludes = []
            for relative_exlcude in relative_excludes:
                excludes.append(os.path.join(self.job_mount_point, relative_exlcude.lstrip('/')))
            self.excluded_backup_paths = excludes
        return True

    def get_cts(self, name=None, uuid=None):
        '''
        Returns a list of hashes containing found container: name,uuid,status
        One may call it with optional parameter "pattern=name" or pattern="uuid"
        '''
        ct_list = []
        if name:
            pattern = name
        elif uuid:
            pattern = uuid
        else:
            pattern = ""
        try:
            ct_list = subprocess.check_output(['/usr/bin/prlctl', 'list', '-o', 'name,uuid,status',
                                               '--vmtype', 'ct', '--all', pattern],
                                               universal_newlines=True)
        except subprocess.CalledProcessError:
            return []
        cts = collections.deque(ct_list.split("\n"))
        cts.pop()
        cts.popleft()
        ct_list = []
        for record in cts:
            cname, cuuid, status = record.split()
            ct_list.append({'uuid': cuuid, 'status': status, 'name': cname})
        bareosfd.DebugMessage(
                        100,
            "Function get_cts() returns {} \n".format(
                str(ct_list)))
        return ct_list

    def list_snapshots(self, ):
        '''
        Returns a list of existing snapshots for a container and returns a list of hashes containing
        uuid of parent snapshot, the snapshot_uuid of the snapshot, its status and the path of delta image
        '''
        try:
            snapshot_list = subprocess.check_output(
                ['/usr/sbin/ploop', 'snapshot-list', self.disk_descriptor],
                universal_newlines=True)
        except subprocess.CalledProcessError:
            return []
        snapshot_list = collections.deque(snapshot_list.split("\n"))
        snapshots = []
        while snapshot_list:
            parentuuid, status, snapshot_uuid, fname = [None, None, None, None]
            line = snapshot_list.popleft()
            if line == "":
                continue
            tokens = line.split()
            if tokens[0] == "PARENT_UUID":
                continue
            if len(tokens) == 4:
                parentuuid, status, snapshot_uuid, fname = tokens[0], tokens[1], tokens[2], tokens[3]
                active = True
            elif len(tokens) == 3:
                parentuuid, snapshot_uuid, fname = tokens[0], tokens[1], tokens[2]
                active = False
            snapshots.append({'parentuuid': parentuuid,
                              'snapshot_uuid': snapshot_uuid.strip("{}"),
                              'fname': fname,
                              'active': active})
        bareosfd.DebugMessage(
                        100,
            "Function list_snapshots: returning {}\n".format(
                str(snapshots)))
        return snapshots

    def add_to_files(self, bpath, excludes):
        '''
        Walks throught a given path topdown and adds recursively all
        files and directories found to self.files
        '''
        for (dirpath, dirnames, filenames) in os.walk(bpath, topdown=True):
            dirnames[:] = [d for d in dirnames if os.path.join(dirpath,d) not in excludes]
            self.files += [os.path.join(dirpath, dir, "") for dir in dirnames]
            filenames[:] = [f for f in filenames if os.path.join(dirpath,f) not in excludes]
            self.files += [os.path.join(dirpath, file) for file in filenames]
        return self.files

    def check_blockers(self, glob_match):
        '''
        Checks if a given glob matches inside a containers filesystem. Return a list
        '''
        if glob_match:
            return glob.glob(os.path.join(self.fs_path, glob_match))
        return []

    def create_snapshot(self, ):
        '''
        Creates a snapshot of a running container and returns an exitcode as int (0 on success)
        '''
        snapshots = ""
        uuidgen = uuid.uuid4()
        if self.verbose:
            bareosfd.DebugMessage(
                100, "Function create_snapshot():\n")
        try:
            snapshots = subprocess.check_output(
                ['/usr/sbin/ploop', 'snapshot', '-u', str(uuidgen), '-F', self.disk_descriptor],
                universal_newlines=True)
        except subprocess.CalledProcessError:
            return False
        # if the snapshot is successfully created we save the UUID
        self.snapshot_uuid = str(uuidgen)
        self.snapshot_created = True
        return True

    def mount_ploop_device(self, ):
        '''
        Mounts a container for backup and sets self.ploop_device
        '''
        pattern = re.compile(r"^Mounted\ .*")
        if not os.path.exists(self.job_mount_point):
            os.mkdir(self.job_mount_point)
        try:
            p_mnt = subprocess.check_output(['/usr/sbin/ploop', 'mount', '-r',
                                             '-c', self.base_image['fname'], '-u', self.snapshot_uuid, '-m',
                                             self.job_mount_point, self.disk_descriptor],
                                             universal_newlines=True)
        except subprocess.CalledProcessError:
            return False
        self.mounted = True
        bareosfd.DebugMessage(300, "ploop mount output: {}\n".format(p_mnt))
        return True


    def start_backup_job(self, ):
        '''
        Check if the container is existing and in backupable state
        '''
        if not self.check_plugin_options():
            bareosfd.JobMessage(
                                bareosfd.M_FATAL,
                "mandatory options uuid and/or name are not specified \n")
            return bareosfd.bRC_Error
        # check state of container
        ct_list = self.get_cts(uuid=self.options['uuid'])
        bareosfd.DebugMessage(
                        100,
            "Function start_backup_job(): ct_list is {} \n".format(
                str(ct_list)))
        if not len(ct_list) == 1:
            bareosfd.JobMessage(
                                bareosfd.M_FATAL,
                "Failed to get name, uuid and status for {}\n".format(
                    self.options['uuid']))
            return bareosfd.bRC_Error
        status = ct_list[0]['status']
        if status not in ['running', 'stopped']:
            bareosfd.JobMessage(
                                bareosfd.M_FATAL,
                "container {} is in status {} - can't do backup \n".format(
                    self.options['name'],
                    status))
            return bareosfd.bRC_Error
        snapshots = self.list_snapshots()
        # keep metadata of the base snapshot that exists always
        self.base_image = snapshots[0]
        # Prevent disk descriptor xml from being backed up
        self.cnf_default_excludes.append(self.disk_descriptor)
        # Prevent base_image from being backed up
        self.cnf_default_excludes.append(self.base_image['fname'])
        if len(snapshots) > 1:
            bareosfd.JobMessage(
                                bareosfd.M_FATAL,
                "Error: found existing snapshots for {} \n".format(
                    self.options['name'],
                    str(snapshots)))
            return bareosfd.bRC_Error
        if os.path.ismount(self.job_mount_point):
            bareosfd.JobMessage(
                                bareosfd.M_FATAL,
                "Error: {} is already mounted \n".format(
                    self.job_mount_point))
            return bareosfd.bRC_Error
        if status == "running":
            blockers = self.check_blockers(self.blocker)
            if blockers:
                bareosfd.JobMessage(
                                        bareosfd.M_FATAL,
                    "Found blocker {} for {}\n".format(
                        blockers,
                        self.options['name']))
                return bareosfd.bRC_Error
            bareosfd.DebugMessage(
                100, "Creating snapshot for {}\n".format(
                    self.options['name']))
            snapshot_created = self.create_snapshot()
            if not snapshot_created:
                bareosfd.JobMessage(
                                        bareosfd.M_FATAL,
                    "Could not create snapshot for {}\n".format(
                        self.options['name']))
                return bareosfd.bRC_Error
            # re read snapshots list to get active snapshot
            snapshots = self.list_snapshots()
            self.base_image = snapshots[0]
            bareosfd.DebugMessage(
                                100,
                "Function start_backup_job(): Status {}, snapshots {} \n".format(
                    status,
                    str(snapshots)))
            # Append the newly created deltafile to exclude list
            delta_disk = snapshots[1]['fname']
            self.cnf_default_excludes.append(delta_disk)
        # if the container is not stopped and the base image is stil active,
        # something went wrong with the created snapshot
        if self.base_image['active'] and not status == "stopped":
            bareosfd.JobMessage(
                                bareosfd.M_FATAL,
                "{} is active but container not stopped, we can't proceed \n".format(
                    self.base_image['fname']))
            return bareosfd.bRC_Error
        # if the container is stopped we use the UUID of the base image
        if status == "stopped":
            self.snapshot_uuid = snapshots[0]['snapshot_uuid']
        bareosfd.DebugMessage(
            100, "Mounting {} to {}\n".format(
                self.base_image['fname'], self.job_mount_point))
        if not self.mount_ploop_device():
            bareosfd.JobMessage(
                                bareosfd.M_FATAL,
                "{} could not be mounted \n".format(
                    snapshots[0]['fname'],
                    self.job_mount_point))
            return bareosfd.bRC_Error
        # Finally we select all config files and the container filessystem
        # to be backed up
        self.add_to_files(self.config_path, self.cnf_default_excludes)
        self.add_to_files(self.job_mount_point, self.excluded_backup_paths)
        return bareosfd.bRC_OK

    def end_backup_job(self, ):
        '''
        Finish the backup: Umount snapshot and merge it
        '''
        bareosfd.DebugMessage(
            100, "end_backup_job() entry point in Python called\n")
        if self.mounted:
            try:
                subprocess.check_output(
                    ['/usr/sbin/ploop', 'umount', '-c', self.base_image['fname'], 
                        '-m', self.job_mount_point, self.disk_descriptor],
                        universal_newlines=True)
            except subprocess.CalledProcessError:
                bareosfd.JobMessage(
                                        bareosfd.M_WARNING,
                    "Cannot unmount base image '{}'\n".format(
                        self.base_image['fname']))
        if self.snapshot_created:
            try:
                # we delete the base_snapshot which results in merging and delteing the delta file
                subprocess.check_call(
                    ['ploop', 'snapshot-delete', '-u', self.snapshot_uuid, self.disk_descriptor])
            except subprocess.CalledProcessError:
                bareosfd.JobMessage(
                                        bareosfd.M_WARNING,
                    "Cannot merge snapshot for CT '{}'\n".format(
                        self.options['name']))
        if self.lock:
            self.lock.release()
        return bareosfd.bRC_OK

    def start_backup_file(self, savepkt):
        '''
        Defines the file to backup and creates the savepkt.
        '''
        if not self.files:
            bareosfd.DebugMessage(
                100,
                "No files to backup\n")
            return bareosfd.bRCs['bareosfd.bRC_Skip']
        # reading file list from beginning to ensure dirs are created before files
        path_to_backup = self.files.popleft()
        possible_link_to_dir = path_to_backup.rstrip('/')
        try:
            osstat = os.lstat(path_to_backup)
        except OSError:
            bareosfd.JobMessage(
                bareosfd.M_ERROR,
                "Cannot backup file '{}'\n".format(path_to_backup))
            return bareosfd.bRCs['bareosfd.bRC_Skip']
        if os.path.islink(possible_link_to_dir):
            savepkt.type = bareosfd.FT_LNK
            savepkt.link = os.readlink(possible_link_to_dir)
            savepkt.no_read = True
            savepkt.fname = possible_link_to_dir
        elif os.path.isdir(path_to_backup):
            # do not try to read a directory as a file
            savepkt.type = bareosfd.FT_DIREND
            savepkt.no_read = True
            savepkt.link = os.path.join(path_to_backup, "")
            savepkt.fname = path_to_backup
        elif stat.S_ISREG(osstat.st_mode):
            savepkt.type = bareosfd.FT_REG
            savepkt.fname = path_to_backup
        else:
            savepkt.type = bareosfd.FT_DELETED
            savepkt.no_read = True
            savepkt.fname = path_to_backup
        statpacket = bareosfd.StatPacket()
        statpacket.st_mode  = osstat.st_mode
        statpacket.st_uid   = osstat.st_uid
        statpacket.st_gid   = osstat.st_gid
        statpacket.st_atime = osstat.st_atime
        statpacket.st_mtime = osstat.st_mtime
        statpacket.st_ctime = osstat.st_ctime
        savepkt.statp = statpacket
        return bareosfd.bRC_OK

    def end_backup_file(self, ):
        '''
        Here we return 'bareosfd.bRC_More' as long as our list files_to_backup is not
        empty and bareosfd.bRC_OK when we are done
        '''
        bareosfd.DebugMessage(
            100,
            "end_backup_file() entry point in Python called\n")
        if self.files:
            return bareosfd.bRC_More
        else:
            return bareosfd.bRC_OK

    def start_restore_job(self, ):
        '''
        Here we decide what kind preparation must be done before the restore
        can start.
        If the whole container should be restored, image preparation in needed
        '''
        bareosfd.DebugMessage(
                        100,
            "ENTERING start_restore_job() entry point in Python called\n")
        if not self.check_plugin_options():
            bareosfd.JobMessage(
                                bareosfd.M_FATAL,
                "mandatory options uuid and/or name are not specified")
            return bareosfd.bRC_Error
        if self.restore == 'ct':
            return self.start_restore_ct_job()
        elif self.restore == 'files':
            return self.start_restore_files_job()
        else:
            bareosfd.JobMessage(
                                bareosfd.M_FATAL,
                "restore type is neither file not ct, cannot prepare restore")
            return bareosfd.bRC_Error

    def start_restore_files_job(self, ):
        '''
         Check if files can be restored on this host
        '''
        ct_list = self.get_cts()
        index = 0
        # check wether ct exists and name and uuid match
        if len(ct_list) == 0:
            bareosfd.JobMessage(
                                bareosfd.M_FATAL,
                "No containers found here - no restore possible\n")
            return bareosfd.bRC_Error

        for ct_hash in ct_list:
            if ct_hash['name'] == self.options['name'] and ct_hash['uuid'].strip(
                    '{}') == self.options['uuid']:
                if not ct_hash['status'] == "running":
                    bareosfd.JobMessage(
                                                bareosfd.M_FATAL,
                        "CT '{}' is in status '{}', no restore possible\n".format(
                            ct_hash['name'],
                            ct_hash['status']))
                    return bareosfd.bRC_Error
                else:
                    break
        else:
            bareosfd.JobMessage(
                                bareosfd.M_FATAL,
                "CT with name {} and UUDI {} was not found, no restore possible\n".format(
                    self.options['name'],
                    self.options['uuid']))
            return bareosfd.bRC_Error

        if not os.path.exists(self.fs_path) and not os.listdir(self.fs_path):
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Cannot list filesystem path for CT '{}' ({}) cause it does not exist or is empty\n".format(
                    self.options['name'], self.fs_path))
            return bareosfd.bRC_Error
        bareosfd.DebugMessage(
            100, "LEAVING start_restore_files_job()\n")
        return bareosfd.bRC_OK

    def start_restore_ct_job(self, ):
        '''
        Check if the CT can be restored on this host
        '''
        ct_list = self.get_cts()
        for ct_hash in ct_list:
            if ct_hash['name'] == self.options['name']:
                bareosfd.JobMessage(
                                        bareosfd.M_FATAL,
                    "CT with name '{}' already existing on this host\n".format(
                        self.options['name']))
                return bareosfd.bRC_Error
            if ct_hash['uuid'].strip('{}') == self.options['uuid']:
                bareosfd.JobMessage(
                                        bareosfd.M_FATAL,
                    "CT with UUID '{}' already existing on this host\n".format(
                        self.options['uuid']))
                return bareosfd.bRC_Error

        if not os.path.exists(self.config_path):
            os.mkdir(self.config_path)
            os.mkdir(os.path.join(self.config_path, 'root.hdd'))
        elif os.path.exists(self.config_path) and os.listdir(self.config_path):
            bareosfd.JobMessage(
                                bareosfd.M_FATAL,
                "Cannot create settings path for CT '{}' ({}) cause it already exists and is not empty\n".format(
                    self.options['name'],
                    self.config_path))
            return bareosfd.bRC_Error

        if not os.path.exists(self.fs_path):
            os.mkdir(self.fs_path)
        elif os.path.exists(self.fs_path) and os.listdir(self.fs_path):
            bareosfd.JobMessage(
                                bareosfd.M_FATAL,
                "Cannot create filesystem path for CT '{}' ({}) cause it already exists and is not empty\n".format(
                    self.options['name'],
                    self.fs_path))
            return bareosfd.bRC_Error

        # create ploop
        image_file = os.path.join(self.config_path, 'root.hdd/root.hds')
        if 'diskspace' in self.options:
            hdd_size_tmp = self.options['diskspace']
        else:
            hdd_size_tmp = 50
        hdd_size_M = int(hdd_size_tmp) * int(1024)
        hdd_size = "{}M".format(hdd_size_M)
        try:
            bareosfd.DebugMessage(100, "CREATING PLOOP")
            ploop_init = subprocess.check_output(
                ['ploop', 'init', '-s', hdd_size, '-f', 'ploop1', '-t', 'ext4', image_file],
                universal_newlines=True)
            for line in ploop_init.split("\n"):
                matched = re.search("Unmounting device", line)
                if matched:
                    self.ploop_device = line.split()[2]
                    bareosfd.DebugMessage(
                        100, "MATCH: {}\n".format(
                            self.ploop_device))
            bareosfd.DebugMessage(100, "CREATED PLOOP\n")
        except subprocess.CalledProcessError:
            bareosfd.JobMessage(
                                bareosfd.M_FATAL,
                "Cannot create ploop file for CT '{}' in {}\n".format(
                    self.options['name'],
                    self.fs_path))
            return bareosfd.bRC_Error

        # mounting ploop
        if not os.path.exists(self.job_mount_point):
            os.mkdir(self.job_mount_point)
        try:
            ploop_mount = subprocess.check_output(
                ['ploop', 'mount', '-d', self.ploop_device, '-m', self.job_mount_point, image_file],
                universal_newlines=True)
            # fixme: need to parse output
        except subprocess.CalledProcessError:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "Cannot mount ploop image of CT '{}' to {}\n".format(
                    self.options['name'], self.fs_path))
            return bareosfd.bRC_Error

        self.prepared = True
        bareosfd.DebugMessage(
            100, "LEAVING start_restore_ct_job () \n")
        return bareosfd.bRC_OK

    def start_restore_file(self, cmd):
        '''
        This is a workaound for wrong order of bareos events during plugin call
        It can happen that this is the first entry point on a restore
        and we missed the bareosfd.bEventStartRestoreJob event because we registerd
        that event after it aready fired. If that is true self.prepared will
        not be set and we call start_restore_job() from here.
        '''
        if not self.prepared:
            return self.start_restore_job()
        return bareosfd.bRC_OK

    def end_restore_job(self, ):
        '''
        The cleanup after a restore job depends on which kind of retore it was.
        For full container restore we need to unmount the images
        '''
        bareosfd.DebugMessage(100, "ENTERING end_restore_job()\n")
        if self.restore == 'file':
            bareosfd.DebugMessage(
                100, "LEAVING end_restore_job because self.restore is 'file'\n")
            return bareosfd.bRC_OK
        elif self.restore == 'ct':
            bareosfd.DebugMessage(
                100, "Calling: self.end_restore_ct_job()")
            return self.end_restore_ct_job()

    def end_restore_ct_job(self, ):
        '''
        Umount the ploop image and register the CT
        '''
        bareosfd.DebugMessage(
            100,
            "end_restore_job() entry point in Python called\n")
        # unmounting ploop
        try:
            ploop_umount = subprocess.check_output(
                ['ploop', 'umount', self.disk_descriptor],
                universal_newlines=True)
        # fixme: need to parse output
        except subprocess.CalledProcessError:
            bareosfd.JobMessage(
                                bareosfd.M_ERROR,
                "Cannot umount ploop image of CT '{}'\n".format(
                    self.options['name']))
            return bareosfd.bRC_Error
        # register CT
        try:
            register = subprocess.check_output(
                ['prlctl', 'register', self.config_path, '--preserve-uuid'],
                universal_newlines=True)
        # fixme: need to parse output
        except subprocess.CalledProcessError:
            bareosfd.JobMessage(bareosfd.M_ERROR,
                                "Cannot register CT '{}'\n".format(
                                self.options['name']))
            return bareosfd.bRC_Error
        bareosfd.DebugMessage(100, "LEAVING end_restore_ct_job () \n")
        return bareosfd.bRC_OK

    def create_file(self, restorepkt):
        '''
        Creates the file to be restored and directory structure.
        '''
        bareosfd.DebugMessage(
            100,
            "create_file() entry point in Python called with %s\n" %
            (restorepkt))
        filename = restorepkt.ofname
        path = os.path.dirname(filename)
        restorepkt.create_status = bareosfd.CF_CORE
        return bareosfd.bRC_OK

    def set_file_attributes(self, restorepkt):
        '''
        copy the necessary file attributes to the restore packet object
        '''
        bareosfd.DebugMessage(
            200,
            "set_file_attributes() entry point in Python called with %s\n" %
            (str(restorepkt)))
        filename = restorepkt.ofname
        statpacket = restorepkt.statp
        if restorepkt.type == bareosfd.FT_REG or restorepkt.type == bareosfd.FT_DIREND:
            os.chown(filename, statpacket.st_uid, statpacket.st_gid)
            os.chmod(filename, statpacket.st_mode)
            os.utime(filename, (statpacket.st_atime, statpacket.st_mtime))
        return bareosfd.bRC_OK
