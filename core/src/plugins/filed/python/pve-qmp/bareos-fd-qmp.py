#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2025-2026 Bareos GmbH & Co. KG
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

# silence module-name warning
# pylint: disable=invalid-name
# pylint: enable=invalid-name

"""A Bareos plugin for qemu-based VMs using the QMP protocol"""

import os
import subprocess
import time
import datetime
import tempfile
from os import read, close, pipe, O_NONBLOCK
from fcntl import fcntl, F_SETPIPE_SZ, F_GETFL, F_SETFL, flock, LOCK_SH, LOCK_EX, LOCK_UN
from select import poll, POLLIN

from BareosFdWrapper import *  # noqa # pylint: disable=import-error,wildcard-import
import bareosfd  # pylint: disable=import-error
import BareosFdPluginBaseclass  # pylint: disable=import-error

lockfile = "/var/lib/bareos/bqmp.state"

def read_ports(f):
    ports = [ i.strip() for i in f.readlines() ]
    return ports


def write_ports(f, ports):
    f.seek(0)
    f.truncate()
    for p in ports:
        f.write(p + "\n")


def acquire_port(start_port):
    with open(lockfile,"r+") as f:
        flock(f, LOCK_EX)
        try:
            ports = read_ports(f)
            port = int(start_port)
            while str(port) in ports:
                port += 1

            ports.append(str(port))

            write_ports(f, ports)
        finally:
            flock(f, LOCK_UN)
    return str(port)


def release_port(port):
    with open(lockfile,"r+") as f:
        flock(f, LOCK_EX)
        try:
            ports = read_ports(f)

            if str(port) in ports:
                ports.remove(str(port))

            write_ports(f, ports)
        finally:
            flock(f, LOCK_UN)


class PVEStorageConfig:
    """wrapper for PVE storage configuration"""

    STORAGE_CFG = "/etc/pve/storage.cfg"

    def __init__(self):
        self.storages = {}

    def parse_config(self):
        with open(self.STORAGE_CFG) as f:
            for line in f:
                if len(line) > 0:
                    parts = line.strip().split(" ")
                    if parts[0].endswith(":"):
                        in_subsection = True
                        name = parts[1]
                        storage_type = parts[0][:-1]
                    elif in_subsection:
                        if len(parts) < 2:
                            in_subsection = False
                            continue
                        if parts[0] == "name":
                            name = parts[1]
                        if parts[0] == "path":
                            self.storages[name] = parts[1] + "/images/"
                        if parts[0] == "vgname":
                            self.storages[name] = "/dev/" + parts[1] + "/"

    def _get_full_path(self, storage_name, path):
        if storage_name in self.storages:
            return self.storages[storage_name] + path
        else:
            return path

    def _activate_lv(self, path):
        if not os.path.exists(path):
            subprocess.run([ "/usr/sbin/lvchange", "-ay", path ])

    def _guess_qsd_driver(self, path):
        if path.startswith("/dev/"):
            self._activate_lv(path)
            return ("host_device", "raw")
        elif path.startswith("/"):
            return ("file", "qcow2")
        else:
            return (None, None)

    def get_qsd_args(self, node_name, pve_diskspec):
        parts = pve_diskspec.split(":", 1)
        if len(parts) < 2:
            raise Exception("invalid storage spec")
        storage_type, storage_path = parts
        export_name = "bqmp-" + node_name
        full_path = self._get_full_path(storage_type, storage_path)
        back_driver, front_driver = self._guess_qsd_driver(full_path)
        args = [
                "--blockdev",
                f"driver={back_driver},node-name=file_{node_name},filename={full_path},cache.direct=on",
                "--blockdev",
                f"driver={front_driver},node-name={node_name},file=file_{node_name}",
                "--export", f"type=nbd,id={export_name},node-name={node_name},name={export_name},writable=on"
               ]
        return args


def parse_vol_size_gb(s):
    if s.endswith("G"):
        return int(s[:-1])
    else:
        return 1

def parse_vol_size_bytes(sizespec):
    s  = sizespec

    if s.isdigit():
        return int(s)
    else:
        unit = s[-1].upper()
        s = s[:-1]

    if not s.isdigit():
        raise Exception("invalid size spec {sizespec}")

    scaling = 1
    if unit == "P":
        scaling = 1024 * 1024 * 1024 * 1024 * 1024
    elif unit == "T":
        scaling = 1024 * 1024 * 1024 * 1024
    elif unit == "G":
        scaling = 1024 * 1024 * 1024
    elif unit == "M":
        scaling = 1024 * 1024
    elif unit == "K":
        scaling = 1024
    else:
        raise Exception(f"invalid size unit {unit}")

    return int(s) * scaling


def parse_volume_name(volname):
    parts = volname.split(":")
    if len(parts)!=2:
        return (None, None)

    if "qcow2" in parts[1]:
        storage_format = "qcow2"
    else:
        storage_format = "raw"

    return (parts[0], storage_format)


def run_command(cmd):
    bareosfd.DebugMessage(10, f"running command {cmd}\n")

    output = subprocess.run(cmd,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        shell=True, check=False).stdout.decode("utf8")

    bareosfd.DebugMessage(20, f"command output: {output}\n")
    return output


def get_vol_size_bytes(storageparams):
    volsize = -1
    for opt in storageparams.split(","):
        if opt.startswith("size="):
            volsize = parse_vol_size_bytes(opt[len("size="):])
    return volsize


def get_vol_storage(storageparams):
    volsize = -1
    parts = storageparams.split(",", 1)
    return parts[0]


def create_volume(vmid, disk, volspec):
    volsize = 0
    iothread = 0
    parts = volspec.split(",",1)
    storageparams = parts[1]
    volname = parts.pop(0)
    storagename, storageformat = parse_volume_name(volname)
    volsize = -1

    for opt in storageparams.split(","):
        if opt.startswith("size="):
            volsize = parse_vol_size_gb(opt[len("size="):])

    cmd = f"qm set {vmid} --{disk} {storagename}:{volsize},format={storageformat},{storageparams}"
    output = run_command(cmd)


class VMConfig:
    """ represents PVE configuration files"""

    def __init__(self):
        self.lines = []
        self.guestid = 0

    def _get_confpath(self):
        assert self.guestid != 0
        return f"/etc/pve/qemu-server/{self.guestid}.conf"

    def parse_from_str(self, confdata):
        self.lines = [ l + "\n" for l in confdata.split("\n") ]

    def parse(self, filename):
        with open(filename, "r") as f:
            self.lines = f.readlines()

    def parse_from_id(self, guestid):
        self.guestid = guestid
        self.parse(self._get_confpath())

    def save(self, filename=None):
        if filename is None:
            filename = self._get_confpath()
        with open(filename, "w") as f:
            f.writelines(self.lines)

    def save_by_id(self, guestid):
        self.guestid = guestid
        self.save(self._get_confpath())

    def _find_line(self, key):
        search = key.strip() + ":"
        i = 0
        for l in self.lines:
            if l.startswith(search):
                return i
            i += 1
        return -1

    def _split(self, line):
        return [ s.strip() for s in line.split(":",1) ]

    def get(self, key):
        i = self._find_line(key)
        if i < 0:
            return None
        return self._split(self.lines[i])[1].strip()

    def set(self, key, value):
        new_line = f"{key}: {value}\n"
        i = self._find_line(key)
        if i < 0:
            self.lines.append(new_line)
        else:
            self.lines[i] = new_line

    def remove(self, key):
        i = self._find_line(key)
        if i >= 0:
            del self.lines[i]

    def get_multi(self, key):
        result = []
        for l in self.lines:
            if l.startswith(key):
                if l[len(key)].isdigit():
                    result.append(self._split(l))
        return result

    def as_str(self):
        return "".join(self.lines)

class LogPipe:
    """special Pipe object to capture log output from subprocess.Popen()"""

    def __init__(self):
        self._read_fd, self._write_fd = pipe()

        # make reading end non-blocking so we can poll() with timeout
        fcntl(self._read_fd, F_SETFL, fcntl(self._read_fd, F_GETFL) | O_NONBLOCK)
        self._read_poller = poll()
        self._read_poller.register(self._read_fd)

        # Increase pipe buffer size to maximum value.
        # While the core does I/O we won't be called. Therefore we want a large
        # buffer so io_process will not block on a full pipe while logging.
        # This will, of course, affect both ends of the pipe.
        with open("/proc/sys/fs/pipe-max-size", "r", encoding="ascii") as file:
            max_size = int(file.read().rstrip())
            fcntl(self._write_fd, F_SETPIPE_SZ, max_size)

    def __del__(self):
        close(self._write_fd)
        close(self._read_fd)

    def fileno(self):
        """Popen() will call this to retrieve a file descriptor."""
        return self._write_fd

    def readlines(
        self, *, init_timeout=60000, read_timeout=100, block_size=1024, soft_fail=False
    ):
        """Generator emulating normal readlines() behaviour."""
        try:
            data = self.read(block_size, timeout=init_timeout)
        except TimeoutError as e:
            if soft_fail:
                return
            raise TimeoutError("timeout waiting for start of data") from e
        while True:
            newline_pos = data.find(b"\n")
            if newline_pos == -1:
                # data does not contain a newline, so we need more input
                try:
                    # fetch more data
                    data += self.read(block_size, timeout=read_timeout)
                except TimeoutError as e:
                    if len(data) == 0:
                        # timeout on end of line: no more data
                        return
                    raise TimeoutError("timeout waiting for end of line") from e
            else:
                # pick first line from data, leave the rest
                yield data[: newline_pos + 1].decode("utf-8", errors="replace")
                data = data[newline_pos + 1 :]

    def read(self, length, *, timeout=100):
        """read() like call with timeout in milliseconds."""
        res = self._read_poller.poll(timeout)
        if len(res) == 0:
            raise TimeoutError

        # there must be exactly one result and that must have our read_fd as fd
        assert len(res) == 1
        fd, event = res[0]
        assert fd == self._read_fd

        if event & POLLIN:
            return read(self._read_fd, length)
        # some other event happened, probably an error
        raise IOError(f"Unexpected bitmask on poll(): {event}")


class UnknownOptionException(Exception):
    """Raised when using an unknown option."""


class BareosFdProxmoxOptions:
    """Holds plugin options."""

    def __init__(self):
        self._options = {
            "force": self.make_opt(bool, False),
            "guestid": self.make_opt(int),
            "restorepath": self.make_opt(str, "/var/lib/vz/dump"),
            "restoretodisk": self.make_opt(bool, False),
        }

    @staticmethod
    def make_opt(type_, default=None):
        """Create a new option"""
        return {"type": type_, "value": default, "value_set": False}

    def check(self):
        """Check if options are complete."""
        success = True
        for name, option in self._options.items():
            bareosfd.DebugMessage(250, f"Option '{name}': {option}'\n")
            if not option["value_set"] and option["value"] is None:
                success = False
                bareosfd.JobMessage(
                    bareosfd.M_FATAL, f"Mandatory option {name} not defined.\n"
                )
        return success

    def __contains__(self, key):
        try:
            return self._options[key]["value_set"]
        except KeyError as e:
            raise UnknownOptionException(key) from e

    def __setitem__(self, key, value):
        try:
            opt = self._options[key]
            if opt["type"] == str:
                opt["value"] = value
            elif opt["type"] == int:
                try:
                    opt["value"] = int(value, base=10)
                except ValueError as e:
                    raise ValueError(
                        f"invalid value '{value}' for integer option '{key}'"
                    ) from e
            elif opt["type"] == bool:
                tmp = value.strip().lower()
                if tmp in ("yes", "on", "true", "1"):
                    opt["value"] = True
                elif tmp in ("no", "off", "false", "0"):
                    opt["value"] = False
                else:
                    raise ValueError(
                        f"invalid value '{value}' for boolean option '{key}'"
                    )
            opt["value_set"] = True
        except KeyError as e:
            raise UnknownOptionException(key) from e

    def __getitem__(self, key):
        try:
            return self._options[key]["value"]
        except KeyError as e:
            raise UnknownOptionException(key) from e

    def __getattr__(self, key):
        try:
            return self._options[key]["value"]
        except KeyError as e:
            raise UnknownOptionException(key) from e


@BareosPlugin  # pylint: disable=undefined-variable
class BareosFdQmp(BareosFdPluginBaseclass.BareosFdPluginBaseclass):
    """Plugin main class"""

    def __init__(self, plugindef):
        bareosfd.DebugMessage(
            100, f"Constructor called in module {__name__} with plugindef={plugindef}\n"
        )

        self.options = BareosFdProxmoxOptions()
        super().__init__(plugindef)

        self.file = None
        self.io_process = None
        self.log_pipe = LogPipe()
        self.conf_backup_done = False
        self.conf_ro_done = False
        self.conffile = None
        self.vmconfig = None
        self.vmconfig_first_ro = None
        self.vmconfig_last_ro = None
        self.disk_names = None
        self.vm_was_running = False
        self.qsd_process = None
        self.restore_nbd_port = "10809"

    def check_options(self, mandatory_options=None):
        """Check plugin options for completeness."""
        del mandatory_options
        return self.options.check()

    def parse_plugin_definition(self, plugindef):
        """
        Parses the plugin arguments
        """
        bareosfd.DebugMessage(
            100, f"parse_plugin_definition() was called in module {__name__}\n"
        )
        try:
            if not super().parse_plugin_definition(plugindef):
                return bareosfd.bRC_Error
        except UnknownOptionException as e:
            bareosfd.JobMessage(
                bareosfd.M_FATAL, f"Unknown option '{e}' passed to plugin\n"
            )
            return bareosfd.bRC_Error

        bareosfd.DebugMessage(100, f"self.options are: = {self.options}\n")
        # Full ("F") or Restore (" ")
        if chr(self.level) not in "FI ":
            bareosfd.DebugMessage(10, f"unsupported level: {self.level}\n")
            bareosfd.JobMessage(
                bareosfd.M_FATAL, "Only Full and Incremental backups are currently supported\n"
            )
            return bareosfd.bRC_Error

        return bareosfd.bRC_OK

    def _get_virtual_path(self):
        guest_name = str(self.options.guestid)
        return f"@BQMP/{guest_name}/"

    def _build_cmd(self, op):
        if self.disk_names is None:
            bareosfd.DebugMessage(10, f"No disk devices found in VM configuration\n")
            bareosfd.JobMessage(
                bareosfd.M_FATAL, "No disk devices found in VM configuration\n"
            )
            return bareosfd.bRC_Error

        guest = str(self.options.guestid)

        if op == "restore":
            disk_args = []
            if self.options.restoretodisk:
                imgrestore_path = self.options.restorepath
            else:
                imgrestore_path = self.restore_nbd_port

            for d in self.disk_names:
                diskconf = self.vmconfig.get(d)
                size = get_vol_size_bytes(diskconf)
                disk_args.append(f"{d}:{size}")
        else:
            imgrestore_path = "none"
            disk_args = self.disk_names

        qmppath = f"/var/run/qemu-server/{guest}.qmp"
        qgapath = f"/var/run/qemu-server/{guest}.qga"
        if not self._ga_is_running():
            qgapath = "none"

        return (
            "/usr/lib/bareos/plugins/bqmp.py",
            op, qmppath, qgapath,
            ",".join(disk_args),
            "-",
            imgrestore_path
        )

    def _read_vm_config(self):
        vmconf = VMConfig()
        vmconf.parse(self.conffile)
        self.vmconfig = vmconf

    def _get_vm_disk_devices(self, vmconf):
        result = []

        device_types = [ "scsi", "virtio", "efidisk", "ide", "sata" ]
        devices = []
        for d in device_types:
            devices.extend(vmconf.get_multi(d))
        if devices is not None:
            for devname, devparams in devices:
                if not "media=cdrom" in devparams:
                   result.append((devname, devparams)) 
        return result

    def _get_vm_disk_names(self, vmconf):
        return [ name for name, _ in self._get_vm_disk_devices(vmconf) ]

    def _set_vm_disk_names(self):
        self.disk_names = self._get_vm_disk_names(self.vmconfig)

    def _get_vmconf_path(self, guestid):
        guest_name = str(guestid)
        return f"/etc/pve/qemu-server/{guest_name}.conf"

    def _start_vm(self, start_paused=False):
        run_command(f"qm start {self.options.guestid}")
        if start_paused:
            run_command(f"qm suspend {self.options.guestid}")

    def _stop_vm(self):
        run_command(f"qm stop {self.options.guestid}")
        run_command(f"qm wait {self.options.guestid}")

    def _vm_is_running(self):
        output = run_command(f"qm status {self.options.guestid}")
        return "running" in output

    def _ga_is_running(self):
        output = run_command(f"qm guest cmd {self.options.guestid} ping")
        return "not running" not in output and "No QEMU guest agent" not in output

    def _lock_vm(self):
        with open(self._get_vmconf_path(self.options.guestid), "a") as f:
            f.write("lock: Restore in progress\n")

    def _unlock_vm(self):
        run_command(f"qm unlock {self.options.guestid}")

    def _recreate_vm(self):
        if os.path.isfile(self._get_vmconf_path(self.options.guestid)):
            # vm already exists, stop it if running
            if self._vm_is_running():
                self._stop_vm()
                self.vm_was_running = True
            # restore vm config file
            #self.vmconfig.save_by_id(self.options.guestid)
            self._lock_vm()
            return

        bareosfd.DebugMessage(100,
            f"vm {self.options.guestid} does not exist, creating it\n")
        bareosfd.JobMessage(bareosfd.M_INFO,
            f"vm {self.options.guestid} does not exist, creating it\n")

        # config file does not exist, so we recreate it
        # from the restore object,
        # but we also need to recreate the empty disks
        # before restoring to them
        self.vmconfig = self.vmconfig_last_ro
        devices = self._get_vm_disk_devices(self.vmconfig)

        # first we remove all disk devices
        for device in self.disk_names:
            self.vmconfig.remove(device)

        self.vmconfig.save_by_id(self.options.guestid)
        time.sleep(1) # give pve time to recognize the file

        # then we recreate the disks, which adds
        # them to the configuration file again
        for device, volspec in devices:
            bareosfd.DebugMessage(100,
                f"creating empty volume {device} {volspec}\n")
            create_volume(self.options.guestid, device, volspec)

        # read back vm config with created volumes
        self.vmconfig.parse_from_id(self.options.guestid)

        self._lock_vm()

    def _start_backup_config_ro(self, savepkt):
        """
        Store the VM config as a restore object.
        The configuration file must have already been
        read when this method is called.
        """
        configdata = self.vmconfig.as_str()
        savepkt.type = bareosfd.FT_RESTORE_FIRST
        savepkt.fname = self.file_to_backup + ".ro"
        savepkt.object_name = savepkt.fname
        savepkt.object = bytearray(configdata.encode("utf-8"))
        savepkt.object_len = len(savepkt.object)
        savepkt.object_index = 0
        savepkt.statp = bareosfd.StatPacket()

        bareosfd.DebugMessage(100,
            f"creating restore object {savepkt.fname} from {self.conffile}\n")

        return bareosfd.bRC_OK

    def _start_backup_config(self, savepkt):
        """
        Defines the file to backup and creates the savepkt.
        """
        guest_name = str(self.options.guestid)
        self.file_to_backup = self._get_virtual_path() + str(self.options.guestid) + ".conf"
        self.conffile = self._get_vmconf_path(guest_name)

        if self.vmconfig is None:
            if not self._vm_is_running():
                bareosfd.JobMessage(
                    bareosfd.M_FATAL,
                    f"vm {guest_name} does not exist or is not running."
                )
                return bareosfd.bRC_Error

            self._read_vm_config()
            self._set_vm_disk_names()

        if not self.conf_ro_done:
            return self._start_backup_config_ro(savepkt)

        if self.vmconfig_last_ro is not None:
            result = self._check_config_changes()
            if result != bareosfd.bRC_OK:
                return result

        bareosfd.DebugMessage(100,
            f"backing up {self.file_to_backup} from {self.conffile}\n")

        mystatp = bareosfd.StatPacket()
        try:
            statp = os.stat(self.conffile)
        except Exception as e:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                'Could net get stat-info for file %s: "%s"' % (self.conffile, e),
            )
            return bareosfd.bRC_Error

        mystatp.st_mode = statp.st_mode
        mystatp.st_ino = statp.st_ino
        mystatp.st_dev = statp.st_dev
        mystatp.st_nlink = statp.st_nlink
        mystatp.st_uid = statp.st_uid
        mystatp.st_gid = statp.st_gid
        mystatp.st_size = statp.st_size
        mystatp.st_atime = int(statp.st_atime)
        mystatp.st_mtime = int(statp.st_mtime)
        mystatp.st_ctime = int(statp.st_ctime)

        savepkt.fname = self.file_to_backup
        if os.path.isfile(self.conffile):
            savepkt.type = bareosfd.FT_REG
        else:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "VM configuration file has invalid type %s" % (self.conffile),
            )
            return bareosfd.bRC_Error

        level = chr(self.level)
        if False and level == "I":
            # fail the job if the configuration file has changed
            last_job_time = bareosfd.GetValue(bareosfd.bVarSinceTime)
            if mystatp.st_mtime > last_job_time:
                bareosfd.JobMessage(
                    bareosfd.M_ERROR,
                    "VM configuration has changed, please run a full backup."
                )
                return bareosfd.bRC_Error

        savepkt.statp = mystatp
        bareosfd.DebugMessage(150, f"file statpx {str(savepkt.statp)}\n")

        return bareosfd.bRC_OK

    def _end_config_ro(self):
        bareosfd.DebugMessage(100, "end_config_ro() called\n")
        self.conf_ro_done = True
        return bareosfd.bRC_More

    def _end_backup_config(self):
        bareosfd.DebugMessage(100, "end_backup_config() called\n")
        self.conf_backup_done = True
        return bareosfd.bRC_More

    def start_backup_file(self, savepkt):
        """
        Defines the file to backup and creates the savepkt.
        """
        bareosfd.DebugMessage(100, f"{__name__}:start_backup_file() called\n")

        if not self.conf_backup_done:
            return self._start_backup_config(savepkt)

        guest_id = self.options.guestid

        guest_name = str(guest_id)
        backup_ts = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        level = chr(self.level)
        write_started = True

        if level == 'F':
            op = "backup"
        elif level == 'I':
            op = "incr"
        else:
            assert False

        cmd = self._build_cmd(op)
        bareosfd.JobMessage(bareosfd.M_INFO, f"Executing {' '.join(cmd)}\n")
        self.io_process = subprocess.Popen(  # pylint: disable=consider-using-with
            cmd,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE,
            stderr=self.log_pipe,
        )


        if not self._check_io_process():
            return bareosfd.bRC_Error

        filename = (
            self._get_virtual_path() + f"disks-{guest_id}-{backup_ts}-{level}.bchunks"
        )

        bareosfd.JobMessage(
            bareosfd.M_INFO,
            f"Backing up VM {guest_name} disks with level {level} to virtual file {filename}\n",
        )

        # create a regular stat packet
        savepkt.statp = bareosfd.StatPacket()
        savepkt.type = bareosfd.FT_REG
        savepkt.fname = filename

        return bareosfd.bRC_OK

    def _prepare_nbd_restore(self):
        restore_nbd_port = self.restore_nbd_port

        storage_cfg = PVEStorageConfig()
        storage_cfg.parse_config()
        args = [ "/usr/bin/qemu-storage-daemon",
             "--nbd-server",
            f"addr.type=inet,addr.host=127.0.0.1,addr.port={self.restore_nbd_port}"
        ]

        for d in self.disk_names:
            diskconf = self.vmconfig.get(d)
            storage = get_vol_storage(diskconf)
            args.extend(storage_cfg.get_qsd_args(d, storage))

        bareosfd.DebugMessage(100, f"running qemu-storage-daemon: {" ".join(args)} ")

        self.qsd_process = subprocess.Popen(args)
         # give qemu storage daemon chance to start
        time.sleep(1)
        # TODO: check if port is open instead of just sleeping
        bareosfd.JobMessage(
            bareosfd.M_INFO,
            f"Started qemu-storage-daemon at NBD port {self.restore_nbd_port} " \
            f"pid {self.qsd_process.pid}\n",
        )

    def _cleanup_nbd_restore(self):
        self.qsd_process.terminate()
        self.qsd_process.wait()
        self.qsd_process = None
        release_port(self.restore_nbd_port)

    def create_file(self, restorepkt):
        """Create file during restore"""
        bareosfd.DebugMessage(100, f"create_file() called with {restorepkt}\n")

        restorepkt.create_status = bareosfd.CF_EXTRACT

        basename = os.path.basename(restorepkt.ofname)
        bareosfd.JobMessage(bareosfd.M_INFO, f"Restoring {basename}\n")

        if restorepkt.ofname.endswith('.conf'):
            return bareosfd.bRC_OK

        elif restorepkt.ofname.endswith('.bchunks'):
            # Start the external process if it is not
            # already started. The pipe will be
            # reused for multiple bchunk files
            # if there are incremental jobs to be
            # restored.
            if self.io_process is None:
                # use vm config from last restore object
                self.vmconfig = self.vmconfig_last_ro
                self._set_vm_disk_names()

                if self.options.restoretodisk:
                    cmd = self._build_cmd("toimage")
                else:
                    self.restore_nbd_port = acquire_port(self.restore_nbd_port)
                    self._recreate_vm()
                    self._prepare_nbd_restore()
                    cmd = self._build_cmd("restore")

                bareosfd.JobMessage(bareosfd.M_INFO, f"Executing {' '.join(cmd)}\n")
                self.io_process = subprocess.Popen(  # pylint: disable=consider-using-with
                    cmd,
                    stdin=subprocess.PIPE,
                    stdout=self.log_pipe,
                    stderr=self.log_pipe,
                )

                self._despool_log(init_timeout=100)
        else:
            bareosfd.JobMessage(
                bareosfd.M_FATAL,
                "invalid restore file (need .conf or .bchunks)",
            )
            return bareosfd.bRC_Error

        return bareosfd.bRC_OK

    def plugin_io_open(self, iop):
        """Open file for backup or restore"""
        iop.status = bareosfd.iostat_do_in_core

        if chr(self.level) in "IF":
            if not self.conf_backup_done:
                self.file = open(self.conffile, "rb")
                iop.filedes = self.file.fileno()
            else:
                # backup from io_process' stdout
                iop.filedes = self.io_process.stdout.fileno()
        elif chr(self.level) == " " and self.options.restoretodisk:
            # restore config to a file
            if iop.fname.endswith(".conf"):
                filename = f"{self.options.restorepath}/{os.path.basename(iop.fname)}"
                self.conffile = filename
                self.file = open(filename, "wb")  # pylint: disable=consider-using-with
                iop.filedes = self.file.fileno()

                bareosfd.DebugMessage(100, f"restoring config to file {filename}\n")
            else:
                # restore to io_process' stdin
                iop.filedes = self.io_process.stdin.fileno()
        elif chr(self.level) == " ":
            if iop.fname.endswith(".conf"):
                # restore config to temporary file
                self.file = tempfile.NamedTemporaryFile("wb", prefix="bqmp",
                    delete=False, delete_on_close=False)
                self.conffile = self.file.name
                bareosfd.DebugMessage(100, f"restoring config to file {self.conffile}\n")
                iop.filedes = self.file.fileno()
            else:
                # restore to io_process' stdin
                iop.filedes = self.io_process.stdin.fileno()
        else:
            # other levels already caught at job start
            assert False

        return bareosfd.bRC_OK

    def plugin_io_close(self, iop):
        """Close file for backup or restore"""
        if iop.fname is not None and iop.fname.endswith(".conf"):
            self.file.close()
        return bareosfd.bRC_OK

    def plugin_io_seek(self, iop):
        """Seek in file, handled by core"""
        raise NotImplementedError

    def plugin_io_read(self, iop):
        """Read a block of data for backup, handled by core"""
        raise NotImplementedError

    def plugin_io_write(self, iop):
        """Write a block of data to restore, handled by core"""
        raise NotImplementedError

    def end_backup_file(self):
        """Called after all data was read"""
        # if we have not created the restore object,
        # do that and return.
        if not self.conf_ro_done:
            return self._end_config_ro()

        # if we have not backed up the vm config yet,
        # do that and return.
        if not self.conf_backup_done:
            return self._end_backup_config()

        self._despool_log(init_timeout=0)

        if not self._wait_for_io_process():
            return bareosfd.bRC_Error
        bareosfd.DebugMessage(100, "end_backup_file(): returning bRC_OK\n")
        return bareosfd.bRC_OK

    def end_restore_file(self):
        """Called after all data was written"""
        if self.options.restoretodisk:
            return bareosfd.bRC_OK

        if self.io_process:
            # do not close the pipe on restore,
            # it might be reused for an incremental
            # bchunks file
            self._despool_log(init_timeout=0)
        else:
            self.file.close()

        bareosfd.DebugMessage(100, "end_restore_file(): returning bRC_OK\n")
        return bareosfd.bRC_OK

    def restore_object_data(self, rop):
        """
        Note:
        This is called in two cases:
        - on diff/inc backup (should be called only once)
        - on restore (for every job id being restored)
        But at the time it is called, it is not possible
        to distinguish which of them it is, because job type
        is "I" until the bEventStartBackupJob event
        """
        ro_suffix = ".conf.ro"
        if not rop.object_name.endswith(ro_suffix):
            bareosfd.JobMessage(bareosfd.M_FATAL,
                f"invalid restore object {rop.object_name}"
            )
            return bareosfd.bRC_Error

        configdata = rop.object.decode()

        guestid = os.path.basename(rop.object_name)
        guestid = guestid[:-len(ro_suffix)]
        if not guestid.isdigit():
            bareosfd.JobMessage(bareosfd.M_FATAL,
                f"invalid restore object {rop.object_name}"
            )
            return bareosfd.bRC_Error

        vmconfig = VMConfig()
        vmconfig.parse_from_str(configdata)

        self.vmconfig_last_ro = vmconfig
        if self.vmconfig_first_ro is None:
            self.vmconfig_first_ro = vmconfig

        return bareosfd.bRC_OK

    def _check_config_changes(self):
        failed = False

        if self.vmconfig_last_ro is None:
            return bareosfd.bRC_OK

        old_disks = self._get_vm_disk_names(self.vmconfig_last_ro)
        new_disks = self._get_vm_disk_names(self.vmconfig)

        # check if vm configuration has changed:
        # - removed disks
        # - disk parameters changed
        # - boot configuration changed
        failed = failed or len(old_disks) > len(new_disks)
        for name in old_disks:
            old_params = self.vmconfig_last_ro.get(name)
            new_params = self.vmconfig.get(name)
            failed = failed or new_params is None or old_params != new_params

        oldbios = self.vmconfig_last_ro.get("bios")
        newbios = self.vmconfig.get("bios")
        failed = failed or oldbios != newbios

        if not failed:
            return bareosfd.bRC_OK

        bareosfd.JobMessage(bareosfd.M_FATAL,
            "VM configuration has changed, please run a full backup."
        )
        return bareosfd.bRC_Error

    def end_job(self):
        bareosfd.DebugMessage(100, "end_job() called\n")
        # on a restore job, close the pipe
        # and wait for the external process to terminate
        if chr(self.level) == " ":
            # tell the process we're done writing
            if self.io_process is not None:
                self.io_process.stdin.close()
                self._despool_log(init_timeout=0)

                if not self._wait_for_io_process():
                    return bareosfd.bRC_Error

            if not self.options.restoretodisk:
                self._cleanup_nbd_restore()
                # remove temporary config file
                os.unlink(self.conffile)
                self._unlock_vm()
                # start vm if it was running
                if self.vm_was_running:
                    self._start_vm()

        bareosfd.DebugMessage(100, "end_job(): returning bRC_OK\n")
        return bareosfd.bRC_OK

    def get_acl(self, acl):
        """Returns no ACL."""
        del acl
        return bareosfd.bRC_OK

    def set_acl(self, acl):
        """Set ACL provided by core. Not supported in this plugin."""
        raise NotImplementedError

    def get_xattr(self, xattr):
        """Returns no extended attributes."""
        del xattr
        return bareosfd.bRC_OK

    def set_xattr(self, xattr):
        """Set extended attributes provided by core. Not supported in this plugin."""
        raise NotImplementedError

    def _despool_log(self, *, init_timeout=0, read_timeout=100):
        """fetches all log lines from the LogPipe (if any) and emits JobMessages."""
        for line in self.log_pipe.readlines(
            init_timeout=init_timeout, read_timeout=read_timeout, soft_fail=True
        ):
            if line.startswith("ERROR:"):
                bareosfd.JobMessage(bareosfd.M_ERROR, line)
            elif line.startswith("WARNING:"):
                bareosfd.JobMessage(bareosfd.M_WARNING, line)
            else:
                bareosfd.JobMessage(bareosfd.M_INFO, line)

    def _check_io_process(self):
        ret = self.io_process.poll()
        if ret is None or ret == 0:
            return True  # still running

        bareosfd.JobMessage(
            bareosfd.M_FATAL,
            f"'{' '.join(self.io_process.args)}' exited with code {ret}\n",
        )
        return False

    def _wait_for_io_process(self, timeout=10):
        bareosfd.JobMessage(bareosfd.M_INFO, "waiting for command to finish\n")
        while self.io_process.returncode is None:
            self._despool_log()
            try:
                self.io_process.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                pass
        return self._check_io_process()
