#!/usr/bin/python3

import sys
import os
import fcntl
import logging
import asyncio
import signal
import json
import subprocess
import nbd
import qemu.qmp

import bqmp_nbd

logger = logging.getLogger(__name__)

PULL_MODE = True

lockfile = "/var/lib/bareos/bqmp.state"

CMD_BACKUP  = 1
CMD_INCR    = 2
CMD_RESTORE = 3
CMD_CLEANUP = 4
CMD_RESET   = 5
CMD_TOIMAGE = 6

unclean_shutdown = False

def get_disk_name(name):
    if name.startswith("drive-"):
        return name[len("drive-"):]
    else:
        return name


def get_node_name(name):
    drive_names = [ "scsi", "virtio", "ide", "sata", "efidisk" ]
    for n in drive_names:
        if name.startswith(n):
            return "drive-" + name

    return name


def get_disk_spec(devicename):
    parts = devicename.split(":")
    if len(parts) < 2:
        raise Exception("size spec for disk required")
    if not parts[1].isdigit():
        raise Exception("invalid disk size spec")
    size = int(parts[1])
    diskname = parts[0]
    return (diskname, size)


def guess_disk_driver(name):
    if name.startswith("scsi"):
        return "scsi-hd"
    return "dontknow"


def get_backup_target(name):
    return "bqmp-" + name


def get_restore_target(name):
    return "bqmp-" + name


def get_nbd_export_name(name):
    return "bqmp-" + name


def get_snap_node(name):
    return "bqmp-snap-" + name


def get_snap_export_name(name):
    return "bqmp-snap-" + name


def get_snap_path(uid, name, path):
    return f"{path}/bqmp-{uid}-{name}.qcow2"


def create_snapshot_image(uid, name, path, size):
    file = get_snap_path(uid, name, path)
    args = [ "/usr/bin/qemu-img", "create",
                "-f", "qcow2", file, str(size) ]
    p = subprocess.run(args, capture_output=True)
    if p.returncode == 0:
        return file
    else:
        raise Exception(
            "failed to create snapshot image: {p.stdout} ({p.returncode})"
        )


class VirtualDisk:
    def __init__(self):
        self.disk_name = None
        self.upper_node = None
        self.lower_node = None
        self.size = 0
        self.has_bitmap = False
        self.is_consistent = False
        self.bitmaps = []
        self.backing_file = None
        self.requires_full = False

    def parse_json(self, r):
        nodename = r["node-name"]
        self.upper_node = nodename

        if "size" in r:
            self.size = r["size"]
        if "file" in r:
            self.backing_file = r["file"]
            if self.backing_file.startswith("json:"):
                self.backing_file = None
        if "children" in r:
            children = r["children"]
            if len(children) == 1:
                child = children[0]
                if "node-name" in child:
                    self.lower_node = child["node-name"]
        if "dirty-bitmaps" in r:
            bitmap = None
            self.bitmaps = r["dirty-bitmaps"]
            for b in self.bitmaps:
                if "name" in b:
                    if b["name"] == "bqmp-bitmap":
                        bitmap = b
                        self.has_bitmap = True
                        break
                else:
                    print("wtf:", b)
            if bitmap is not None:
                self.is_consistent = True
                if "inconsistent" in bitmap:
                    if bitmap["inconsistent"] == True:
                        self.is_consistent = False

        if "image" in r:
            self.size = r["image"]["virtual-size"]

        self.disk_name = get_disk_name(nodename)

    def __str__(self):
        return f"<(VirtualDisk) {self.disk_name},{self.upper_node},{self.lower_node}," \
            f"{self.size},{self.backing_file},{self.has_bitmap},{self.is_consistent}>"

    def __repr__(self):
        return self.__str__()

class BQMPFacade:

    def __init__(self, selected_disks, snapshot_path, qmp_socket, qga_socket=None, debug=False):
        self.qmp_socket = qmp_socket
        self.qga_socket = qga_socket
        self.debug = debug
        self.qmp = None
        self.qmpga = None
        self.qmp = qemu.qmp.QMPClient("bqmp")
        self.nbd_ipaddr = "127.0.0.1"
        self.nbd_port   = "10789"
        self.quiesce = True
        self.incremental = False
        self.selected_disks = selected_disks
        self.virtual_disks = {}
        self.snapshot_path = snapshot_path
        self.terminating = False

        if self.qga_socket == "none":
            self.qga_socket = None

    async def connect(self):
        await self.qmp.connect(self.qmp_socket)

    async def connect_ga(self):
        if self.qga_socket is None:
            return
        self.qmpga = qemu.qmp.QMPClient("bqmp-ga")
        self.qmpga.await_greeting = False
        self.qmpga.negotiate = False
        await self.qmpga.connect(self.qga_socket)

    async def disconnect(self):
        if self.qmpga is not None:
            await self.qmpga.disconnect()
        await self.qmp.disconnect()

    async def qmp_execute(self, cmd, args = {}):
        if self.debug:
            print("qmp_execute ",cmd,args)
        result = await self.qmp.execute(cmd, args)
        if self.debug:
            print(" result: ", result)
        return result

    async def freeze_guest(self):
        if self.qmpga is None:
            return {}
        return await self.qmpga.execute("guest-fsfreeze-freeze", {})

    async def thaw_guest(self):
        if self.qmpga is None:
            return {}
        return await self.qmpga.execute("guest-fsfreeze-thaw", {})

    async def add_snapshot_image(self, vdisk):
        path = create_snapshot_image(self.nbd_port,
                vdisk.disk_name, self.snapshot_path, vdisk.size)
        snap_node = get_snap_node(vdisk.disk_name)
        return await self.qmp_execute("blockdev-add", {
            "driver": "qcow2",
            "node-name": snap_node,
            "file": {
                "driver": "file",
                "filename": path,
                "aio": "io_uring",
                "cache": { "direct": True, "no-flush": True}
            },
            "backing": vdisk.upper_node,
            "discard": "ignore",
            "cache": { "direct": True, "no-flush": True }
        })

    async def remove_snapshot_image(self, vdisk):
        await self.qmp_execute("blockdev-del", {
            "node-name": get_snap_node(vdisk.disk_name)
        })
        os.unlink(get_snap_path(self.nbd_port, vdisk.disk_name, self.snapshot_path))

    def _add_snap_action(self, vdisk, actions, incremental=False):
        # For incremental backups, clear the bitmap so next time all
        # changes are visible from the point in time the snapshot
        # was taken.
        # Also add a temporary bitmap which will track new changes
        # that occur during the backup. It will be merged with
        # the incremental bitmap after the backup is complete.
        if not incremental:
            actions.append(
                {
                    "type": "block-dirty-bitmap-clear",
                    "data": {
                        "node": vdisk.upper_node,
                        "name": "bqmp-bitmap"
                    }
                })

        actions.append(
            {
                "type": "block-dirty-bitmap-disable",
                "data": {
                    "node": vdisk.upper_node,
                    "name": "bqmp-bitmap"
                }
            })
        actions.append(
            {
                "type": "block-dirty-bitmap-add",
                "data": {
                    "node": vdisk.upper_node,
                    "name": "bqmp-bitmap-tmp"
                }
            })
        actions.append(
                {
                    "type": "blockdev-backup",
                    "data": {
                        "device": vdisk.upper_node,
                        "target": get_snap_node(vdisk.disk_name),
                        "sync": "none",
                        "job-id": get_snap_node(vdisk.disk_name),
                        "on-cbw-error": "break-snapshot",
                    }
                })

    async def add_snapshot_export(self, devicename):
        await self.qmp_execute("block-export-add", {
                    "id": get_snap_node(devicename),
                    "type": "nbd",
                    "node-name": get_node_name(devicename),
                    "name": get_snap_node(devicename),
                    "bitmaps": [ "bqmp-bitmap" ]
                })

    async def remove_snapshot_export(self, devicename):
        await self.qmp_execute("block-export-del", { "id": get_snap_node(devicename) })

    async def cancel_snapshot_backup(self, devicename):
        await self.qmp_execute("block-job-cancel", { "device": get_snap_node(devicename) })

    async def merge_bitmaps_1(self, vdisk):
        await self.qmp_execute("block-dirty-bitmap-enable", {
                    "node": vdisk.upper_node,
                    "name": "bqmp-bitmap"
                })
        await self.qmp_execute("block-dirty-bitmap-merge", {
                    "node": vdisk.upper_node,
                    "target": "bqmp-bitmap",
                    "bitmaps": [ "bqmp-bitmap-tmp" ]
                })
        await self.qmp_execute("block-dirty-bitmap-remove", {
                    "node": vdisk.upper_node,
                    "name": "bqmp-bitmap-tmp"
                })

    async def merge_bitmaps(self, vdisk):
        args = { "actions": [
            {
                "type": "block-dirty-bitmap-clear",
                "data": {
                    "node": vdisk.upper_node,
                    "name": "bqmp-bitmap"
                }
            },
            {
                "type": "block-dirty-bitmap-enable",
                "data": {
                    "node": vdisk.upper_node,
                    "name": "bqmp-bitmap"
                }
            },
            {
                "type": "block-dirty-bitmap-merge",
                "data": {
                    "node": vdisk.upper_node,
                    "target": "bqmp-bitmap",
                    "bitmaps": [ "bqmp-bitmap-tmp" ]
                }
            },
            {
                "type": "block-dirty-bitmap-remove",
                "data": {
                    "node": vdisk.upper_node,
                    "name": "bqmp-bitmap-tmp"
                }
            },
        ]}
        await self.qmp_execute("transaction", args)

    def _add_full_action(self, srcnode, targetnode, actions):
        actions.append(
            {
                "type": "block-dirty-bitmap-clear",
                "data": {
                     "node": srcnode,
                     "name": "bqmp-bitmap"
                }
            })
        actions.append(
            {
                "type": "blockdev-backup",
                "data": {
                     "device": srcnode,
                     "target": targetnode,
                     "sync": "full",
                     "on-cbw-error": "break-snapshot",
                     "job-id": "backup-" + targetnode
                }
            })

    def _add_incr_action(self, srcnode, targetnode, actions):
        actions.append(
            {
                "type": "blockdev-backup",
                "data": {
                     "device": srcnode,
                     "target": targetnode,
                     "sync": "incremental",
                     "on-cbw-error": "break-snapshot",
                     "bitmap": "bqmp-bitmap",
                     "job-id": "backup-" + targetnode
                }
            })

    async def start_snapshot_backup(self, incremental=False):
        args = { "actions": [] }

        for diskname in self.selected_disks:
            vdisk = self.virtual_disks[diskname]
            await self.add_snapshot_image(vdisk)
            await self.setup_bitmap(vdisk.upper_node)
            self._add_snap_action(vdisk, args["actions"], incremental)

        if self.debug:
            print("start_snapshot_backup args:", args)

        await self.freeze_guest()
        result = await self.qmp.execute("transaction", args)
        if self.debug:
            print("start_snapshot_backup result:", result)
        await self.thaw_guest()

    async def pull_snapshot_blocks(self, backing_store, incremental=False):
        await self.start_nbd_server()

        index = 0
        for d in self.selected_disks:
            vdisk = self.virtual_disks[d]
            await self.add_snapshot_export(vdisk.disk_name)
            export = (get_snap_export_name(vdisk.disk_name), vdisk.size)
            nbd_addr = ("127.0.0.1", self.nbd_port)
            bqmp_nbd.read_nbd_image(index, export, nbd_addr,
                "bqmp-bitmap", backing_store, incremental)
            index += 1

    async def end_snapshot_backup(self):
        for d in self.selected_disks:
            vdisk = self.virtual_disks[d]
            await self.cancel_snapshot_backup(vdisk.disk_name)
        await self.wait_snapshot_jobs()
        await asyncio.sleep(3)
        # stopping the nbd server removes all exports
        await self.stop_nbd_server()

        # no transaction
        for d in self.selected_disks:
            vdisk = self.virtual_disks[d]
            #await self.remove_snapshot_export(vdisk.disk_name)
            # delete blockdev
            await self.remove_snapshot_image(vdisk)

        #transaction?
        for d in self.selected_disks:
            vdisk = self.virtual_disks[d]
            await self.merge_bitmaps(vdisk)

    async def abort_snapshot_backup(self):
        for d in self.selected_disks:
            vdisk = self.virtual_disks[d]
            await self.cancel_snapshot_backup(vdisk.disk_name)
        await self.wait_snapshot_jobs()

        # stopping the nbd server removes all exports
        await self.stop_nbd_server()

        # no transaction
        for d in self.selected_disks:
            vdisk = self.virtual_disks[d]
            #await self.remove_snapshot_export(vdisk.disk_name)
            # delete blockdev
            await self.remove_snapshot_image(vdisk)

        #transaction?
        for d in self.selected_disks:
            vdisk = self.virtual_disks[d]
            await self.merge_bitmaps(vdisk)

    async def start_full_backup(self, srcnodes, targetnodes):
        args = { "actions": [] }
        if self.debug:
            print(srcnodes, targetnodes)
        assert len(srcnodes) == len(targetnodes)

        await self.setup_bitmaps(srcnodes)

        for i in range(0,len(srcnodes)):
            srcnode = srcnodes[i]
            targetnode = targetnodes[i][0]
            self._add_full_action(srcnode, targetnode, args["actions"])

        if self.debug:
            print("start_full_backup args:", args)

        result = await self.qmp.execute("transaction", args)
        if self.debug:
            print("blockdev-backup full", result)

    async def setup_bitmaps(self, nodes):
        for n in nodes:
            await self.setup_bitmap(n)

    async def start_incr_backup(self, srcnodes, targetnodes):
        args = {
            "properties": { "completion-mode": "grouped" },
            "actions": []
        }
        if self.debug:
            print(srcnodes, targetnodes)
        assert len(srcnodes) == len(targetnodes)

        await self.setup_bitmaps(srcnodes)
        is_consistent = await self.validate_dirty_bitmaps(srcnodes)

        if not is_consistent:
            raise Exception("invalid dirty bitmap, hypervisor crash?")

        for i in range(0,len(srcnodes)):
            srcnode = srcnodes[i]
            targetnode = targetnodes[i][0]
            disk = self.get_virtual_node(srcnode)
            if disk.requires_full:
                self._add_full_action(srcnode, targetnode, args["actions"])
            else:
                self._add_incr_action(srcnode, targetnode, args["actions"])

        if self.debug:
            print("start_incr_backup args:", args)

        result = await self.qmp.execute("transaction", args)
        if self.debug:
            print("blockdev-backup incr", result)

    async def wait_backup_job(self, disks):
        incomplete_jobs = [ ("backup-bqmp-" + n) for n in disks ]

        while len(incomplete_jobs) > 0:
            event = await self.listener.get()
            dev = event["data"]["device"]
            event_type = event["event"]
            if dev in incomplete_jobs:
                incomplete_jobs.remove(dev)
            if self.terminating and event_type == 'BLOCK_JOB_CANCELLED':
                continue
            if event_type != "BLOCK_JOB_COMPLETED":
                raise Exception(f"qemu block job {dev} failed: {event_type}")

    async def wait_backup_job_old(self, disks):
            while True:
                job_info = await self.qmp_execute("query-block-jobs")
                if job_info is None or len(job_info) == 0:
                    break
                wanted_job = [ job for job in job_info if job["device"].startswith("backup-") ]
                if len(wanted_job) == 0:
                        break
                await asyncio.sleep(3)

    async def wait_snapshot_jobs(self):
            while True:
                job_info = await self.qmp_execute("query-block-jobs")
                if job_info is None or len(job_info) == 0:
                    break
                wanted_job = [ job for job in job_info if job["device"] in self.selected_disks ]
                if len(wanted_job) == 0:
                        break
                await asyncio.sleep(1)

    async def cancel_backup_job(self, force=False):
        self.terminating = True
        job_info = await self.qmp_execute("query-block-jobs")
        if job_info is None or len(job_info) == 0:
            return
        wanted_job = [ job for job in job_info if job["device"].startswith("backup-") ]
        for j in wanted_job:
            await self.qmp_execute("block-job-cancel", { "device": j["device"], "force": force })

    async def create_nbd_target(self, name):
        if self.debug:
            print("create_nbd_target name", name)

        result = await self.qmp_execute("blockdev-add", {
            "driver": "nbd",
            "node-name": get_backup_target(name),
            "export": get_nbd_export_name(name),
            "server": {
                "type": "inet",
                "host": "127.0.0.1",
                "port": self.nbd_port,
            }
        })

    async def destroy_nbd_target(self, name):
        result = await self.qmp_execute("blockdev-del", {
            "node-name": name
        })

    def get_virtual_node(self, diskname):
        return self.virtual_disks[diskname]

    def get_virtual_disk(self, nodename):
        return self.virtual_disks[get_disk_name(nodename)]

    async def add_bitmap(self, nodename, persistent):
        result = await self.qmp_execute("block-dirty-bitmap-add", {
            "node": nodename,
            "name": "bqmp-bitmap",
            "persistent": persistent
        })

    async def setup_bitmap(self, nodename):
        disk = self.get_virtual_disk(nodename)

        if disk.has_bitmap and not disk.is_consistent:
            await self.remove_bitmap(nodename)
            disk.has_bitmap = False
            disk.requires_full = True
            if self.debug:
                print(f"{nodename} has inconsistent bitmap, removing it")
        if not disk.has_bitmap:
            if self.debug:
                print(f"{nodename} has no bitmap, adding it")
            try:
                await self.add_bitmap(nodename, True)
            except qemu.qmp.qmp_client.ExecuteError as e:
                if str(e).endswith("Operation not supported"):
                    await self.add_bitmap(nodename, False)
                else:
                    raise e

            disk.requires_full = True

    async def clear_bitmap(self, nodename):
        disk = self.get_virtual_node(nodename)
        if not disk.has_bitmap:
            await self.setup_bitmap(nodename)
        else:
            result = await self.qmp_execute("block-dirty-bitmap-clear", {
                "node": nodename,
                "name": "bqmp-bitmap"
            })

    async def remove_bitmap(self, nodename):
        result = await self.qmp_execute("block-dirty-bitmap-remove", {
            "node": nodename,
            "name": "bqmp-bitmap"
        })

    async def remove_temp_bitmap(self, nodename):
        result = await self.qmp_execute("block-dirty-bitmap-remove", {
            "node": nodename,
            "name": "bqmp-bitmap-tmp"
        })

    async def add_restore_export(self, name, nodename):
        result = await self.qmp_execute("block-export-add", {
            "type": "nbd",
            "id": get_restore_target(name),
            "node-name": nodename,
            "name": get_restore_target(name),
            "writable": True
        })

    async def delete_restore_export(self, name):
        result = await self.qmp_execute("block-export-del", {
            "id": get_restore_target(name)
        })

    async def start_nbd_server(self):
        result = await self.qmp_execute("nbd-server-start", {
            "addr": {
               "type": "inet",
               "data": {
                  "host": "127.0.0.1",
                  "port": self.nbd_port
               }
            }              
        })

    async def stop_nbd_server(self):
        return await self.qmp_execute("nbd-server-stop")

    async def pause_vm(self):
        return await self.qmp_execute("stop")

    async def continue_vm(self):
        return await self.qmp_execute("cont")

    async def reset_vm(self):
        return await self.qmp_execute("quit")

    async def show_nodes(self):
        result = await self.qmp.execute("query-named-block-nodes", {})
        print(json.dumps(result, indent=2))

    async def find_drive_node(self, name):
        await self.query_virtual_disks()

        diskname = get_disk_name(name)
        if diskname not in self.virtual_disks:
            return (None, 0)

        vdisk = self.virtual_disks[diskname]

        if self.debug:
            print(name, "is", vdisk.lower_node, vdisk.size)

        return (vdisk.lower_node, vdisk.size)

    async def validate_dirty_bitmaps(self, lower_nodes):
        has_bitmap = True
        is_consistent = True
        for n in lower_nodes:
            disk = self.get_virtual_node(n)
            if not disk.has_bitmap:
                has_bitmap = False
            else:
                if not disk.is_consistent:
                    is_consistent = False

        return is_consistent

    async def query_virtual_disks(self, force=False):
        if not force and len(self.virtual_disks) > 0:
            return

        result = await self.qmp.execute("query-named-block-nodes")
        self.virtual_disks = {}
        for r in result:
            if "node-name" in r:
                vdisk = VirtualDisk()
                vdisk.parse_json(r)
                self.virtual_disks[vdisk.disk_name] = vdisk
        # print("virtual_disks:", self.virtual_disks)

    async def query_dirty_bitmaps(self, lower_nodes):
        result = await self.qmp.execute("query-named-block-nodes")
        drive_node = None
        has_bitmap = True
        is_consistent = True

        for r in result:
            if r["node-name"] in lower_nodes:
                if self.debug:
                    print(json.dumps(r, indent=2))
                if "dirty-bitmaps" in r:
                    bitmap = None
                    bitmaps = r["dirty-bitmaps"]
                    for b in bitmaps:
                        if b["name"] == "bqmp-bitmap":
                            bitmap = b
                            break
                    if bitmap is not None:
                        if "inconsistent" in bitmap:
                            if bitmap["inconsistent"] == True:
                                is_consistent = False
                                if self.debug:
                                    print("invalid bitmap", json.dumps(bitmap, indent=2))
                                break
                    else:
                        has_bitmap = False
                else:
                    has_bitmap = False

        return (has_bitmap, is_consistent)

    async def test_query_block(self):
        result = await self.qmp.execute("query-block", {})
        print(json.dumps(result, indent=2))

    async def test_query_nodes(self):
        result = await self.qmp.execute("query-named-block-nodes", {})
        print(json.dumps(result, indent=2))

    async def test_query_jobs(self):
        result = await self.qmp_execute("query-jobs", {})

    async def test_query_exports(self):
        result = await self.qmp_execute("query-block-exports", {})

    async def test_query_bitmaps(self, lower_nodes):
        has_bitmaps, is_consistent = await self.query_dirty_bitmaps(lower_nodes)
        print(f"has_bitmaps: {has_bitmaps} is_consistent: {is_consistent}")


    async def run_backup(self, devices, nbd_targets, incremental):
        self.listener = qemu.qmp.EventListener(('BLOCK_JOB_COMPLETED', 'BLOCK_JOB_CANCELLED',
                        'BLOCK_JOB_ERROR'))
        self.qmp.register_listener(self.listener)

        try:
            await self.freeze_guest()
            devicenodes = [ d for n,d,u in devices ]
            if not incremental:
                await self.start_full_backup(devicenodes, nbd_targets)
            else:
                await self.start_incr_backup(devicenodes, nbd_targets)
        finally:
            await self.thaw_guest()
        try:
            await self.wait_backup_job([get_disk_name(u) for _,_,u in devices])
        finally:
            self.qmp.remove_listener(self.listener)

    async def run_snapshot_backup(self, store_path, incremental=False):
        backing_store = None
        exports = []
        for d in self.selected_disks:
            vdisk = self.virtual_disks[d]
            exports.append((get_snap_export_name(vdisk.disk_name), vdisk.size))

        if store_path is None or store_path == "-":
            backing_store = bqmp_nbd.StdoutBackingStore(exports)
        else:
            backing_store = bqmp_nbd.BackingStore(store_path, exports)
        if not incremental:
            backing_store.zap()

        backing_store.next()
        try:
            await self.start_snapshot_backup(incremental)
            await self.pull_snapshot_blocks(backing_store, incremental)
            await self.end_snapshot_backup()
        except Exception as e:
            await self.abort_snapshot_backup()
            raise e
        finally:
            backing_store.close()

def imgrestore_handler(file, offset, data, length):
    if data is not None:
        file.seek(offset)
        file.write(data)


def restore_to_images(store_path, devicenames, image_path, incr=False):
    backing_store = None
    img_targets = []
    img_files = []
    for d in devicenames:
        diskname, size = get_disk_spec(d)
        imgfile = os.path.join(image_path, diskname + ".raw")
        f = open(imgfile, "w+b")
        img_targets.append((f, size))
        img_files.append(f)

    if store_path is None or store_path == "-":
        backing_store = bqmp_nbd.StdinBackingStore(img_targets)
    else:
        backing_store = bqmp_nbd.BackingStore(store_path, img_targets)

    backing_store.next()
    backing_store.start_dump()
    backing_store.dump(img_files, imgrestore_handler)
    backing_store.close()

    for f, size in img_targets:
        f.truncate(size)
        f.close()


async def cleanup(bqmp, devicenames):
    for name in devicenames:
        vdisk = bqmp.virtual_disks[name]
        try:
            await bqmp.destroy_nbd_target(get_backup_target(name))
        except:
            pass
        try:
            await bqmp.remove_snapshot_export(name)
        except:
            pass
        try:
            await bqmp.cancel_snapshot_backup(name)
        except:
            pass
        try:
            await bqmp.remove_snapshot_image(vdisk)
        except:
            pass
        try:
            await bqmp.remove_temp_bitmap(vdisk.upper_node)
        except:
            pass
    try:
        await bqmp.stop_nbd_server()
    except:
        pass


async def main(qmp_path, qga_path, devicenames,
        cmd, store_path, imgrestore_path, snapshot_path):
    debug = False
    if cmd == CMD_CLEANUP:
        debug = True

    if cmd == CMD_TOIMAGE:
        restore_to_images(store_path, devicenames, imgrestore_path)
        return

    if cmd == CMD_RESTORE:
        nbd_targets = []
        try:
            for d in devicenames:
                name,size = get_disk_spec(d)
                nbd_targets.append((get_restore_target(name), int(size)))
        except:
            raise Exception("invalid device spec")

        if imgrestore_path is None:
            raise Exception("no NBD port given")

        nbd_ipaddr = "127.0.0.1"
        nbd_restore_port = imgrestore_path
        bqmp_nbd.restore_to_nbd(store_path, nbd_targets, count, nbd_ipaddr, nbd_restore_port)
        return

    bqmp = BQMPFacade(devicenames, snapshot_path, qmp_path, qga_path, debug=debug)

    try:
        await bqmp.connect()
    except qemu.qmp.protocol.ConnectError as e:
        raise Exception(f"failed to connect to QMP socket at {qmp_path}")

    # stop nbd server if it is running
    try:
        await bqmp.stop_nbd_server()
    except:
        pass

    nbd_targets = []
    devices = []
    for devicename in devicenames:
        upper_device_node = "drive-" + devicename
        lower_node, size = await bqmp.find_drive_node(upper_device_node)
        if lower_node is None:
            raise Exception(f"virtual disk {devicename} not found")
        devices.append( (devicename, lower_node, upper_device_node) )
        if cmd == CMD_RESTORE:
            nbd_targets.append((get_restore_target(devicename), size))
        else:
            nbd_targets.append((get_backup_target(devicename), size))

    if cmd in [ CMD_CLEANUP, CMD_RESET ]:
        await bqmp.test_query_block()
        #await bqmp.test_query_nodes()
        await bqmp.test_query_jobs()
        if cmd == CMD_RESET:
            for name, devicenode, upper_node in devices:
                try:
                    await bqmp.remove_bitmap(devicenode)
                except:
                    pass
        await cleanup(bqmp, devicenames)

    elif cmd in [ CMD_BACKUP, CMD_INCR ]:
        # doing a backup
        incremental = (cmd == CMD_INCR)

        # cleanup in case of an unclean job shutdown
        await cleanup(bqmp, devicenames)

        await bqmp.connect_ga()

        if imgrestore_path is None:
            raise Exception("no NBD port given")
        nbd_ipaddr = "127.0.0.1"
        nbd_port = imgrestore_path
        bqmp.nbd_port = nbd_port

        if PULL_MODE:
            await bqmp.run_snapshot_backup(store_path, incremental)
        else:
            server = await run_nbd_server(bqmp.nbd_port, store_path, nbd_targets, incremental)
            nbdtask = server.task
            logger.debug("nbd server started.")

            loop = asyncio.get_running_loop()
            for s in [ signal.SIGINT, signal.SIGPIPE, signal.SIGTERM, signal.SIGALRM ]:
                loop.add_signal_handler(s, signal_cleanup, s, loop, server, bqmp)

            for devicename in devicenames:
               await bqmp.create_nbd_target(devicename)

            try:
                await bqmp.run_backup(devices, nbd_targets, incremental)
            finally:
                for target in nbd_targets:
                    logger.debug(f"destroy_nbd_target {target}")
                    await bqmp.destroy_nbd_target(target[0])

                nbdtask.cancel()
                try:
                    await nbdtask
                except asyncio.CancelledError:
                    logger.debug("nbd server stopped.")
    else:
        logger.warn("internal error, invalid command")
    try:
        await bqmp.disconnect()
    except EOFError as e:
        pass


def signal_cleanup(signum, event_loop, server, bqmp):
    if signum == signal.SIGALRM:
        sys.stderr.write(f"bqmp: write backpressure threshold exceeded, shutting down\n")
    else:
        sys.stderr.write(f"bqmp: signal {signum} received, shutting down\n")
    asyncio.create_task(async_cleanup(event_loop, server, bqmp))


async def async_cleanup(event_loop, server, bqmp):
    global unclean_shutdown
    unclean_shutdown = True
    if bqmp is not None:
        await bqmp.cancel_backup_job()
        await asyncio.sleep(3)
        await bqmp.cancel_backup_job(force=True)
    await server.discard()
    server.task.cancel()
    try:
        await server.task
    except asyncio.CancelledError:
        pass


async def run_nbd_server(port, path, nbd_targets, incr):
    ipaddr = "127.0.0.1"
    backing_store = None


    if path is None or path == "-":
        backing_store = bqmp_nbd.StdoutBackingStore(nbd_targets)
        # backing_store = bqmp_nbd.AsyncBackingStore(nbd_targets)
    else:
        backing_store = bqmp_nbd.BackingStore(path, nbd_targets)
        if not incr:
            backing_store.zap()
    await backing_store.init_async()
    backing_store.next()

    server = bqmp_nbd.NbdServer(ipaddr, int(port), backing_store)
    task = server.create_task()

    loop = asyncio.get_running_loop()
    for s in [ signal.SIGHUP, signal.SIGPIPE, signal.SIGTERM ]:
        loop.add_signal_handler(s, signal_cleanup, s, loop, server, None)

    return server


async def main_wrapper(qmp_path, qga_path, devicenames,
        cmd, store_path, imgrestore_path, snapshot_path):
    try:
        await main(qmp_path, qga_path, devicenames, cmd,
                store_path, imgrestore_path, snapshot_path)
    except Exception as e:
        raise e
        sys.stderr.write(str(e))
        sys.stderr.write('\n')
        sys.exit(3)


if __name__ == "__main__":
    # logging.basicConfig(level=logging.DEBUG)

    if len(sys.argv) < 5:
        sys.stderr.write("invalid arguments\n")
        sys.exit(1)
    command = sys.argv[1]
    qmp_path = sys.argv[2]
    qga_path = sys.argv[3]
    disks = sys.argv[4].split(",")
    store_path = None
    imgrestore_path = None
    snapshot_path = "/var/lib/bareos"

    if len(sys.argv) > 5:
        store_path = sys.argv[5]
    if len(sys.argv) > 6:
        imgrestore_path = sys.argv[6]
    if len(sys.argv) > 7:
        snapshot_path = sys.argv[7]

    if command == "restore":
        op = CMD_RESTORE
    elif command == "cleanup":
        op = CMD_CLEANUP
    elif command == "reset":
        op = CMD_RESET
    elif command == "incr":
        op = CMD_INCR
    elif command == "toimage":
        op = CMD_TOIMAGE
    elif command != "backup":
        sys.stderr.write("invalid arguments\n")
        sys.exit(1)
    else:
        op = CMD_BACKUP

    if op in [ CMD_BACKUP, CMD_INCR ]:
        if (store_path is None  or store_path == "-") and sys.stdout.isatty():
            sys.stderr.write("no storage path given and stdout is a tty\n")
            sys.exit(2)

    count = 0
    asyncio.run(main_wrapper(qmp_path, qga_path,
        disks, op, store_path, imgrestore_path, snapshot_path))
    if unclean_shutdown:
        sys.exit(4)
