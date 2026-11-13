#!/usr/bin/python3

import sys
import os
import fcntl
import logging
import asyncio
import json
import nbd
import qemu.qmp

import bqmp_nbd

logger = logging.getLogger(__name__)

lockfile = "/var/lib/bareos/bqmp.state"

CMD_BACKUP  = 1
CMD_INCR    = 2
CMD_RESTORE = 3
CMD_CLEANUP = 4
CMD_RESET   = 5
CMD_TOIMAGE = 6


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
        fcntl.flock(f, fcntl.LOCK_EX)
        try:
            ports = read_ports(f)
            port = int(start_port)
            while str(port) in ports:
                port += 1

            ports.append(str(port))

            write_ports(f, ports)
        finally:
            fcntl.flock(f, fcntl.LOCK_UN)
    return str(port)


def release_port(port):
    with open(lockfile,"r+") as f:
        fcntl.flock(f, fcntl.LOCK_EX)
        try:
            ports = read_ports(f)

            if str(port) in ports:
                ports.remove(str(port))

            write_ports(f, ports)
        finally:
            fcntl.flock(f, fcntl.LOCK_UN)


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
                if b["name"] == "bqmp-bitmap":
                    bitmap = b
                    self.has_bitmap = True
                    break
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

    def __init__(self, qmp_socket, qga_socket=None, debug=False):
        self.qmp_socket = qmp_socket
        self.qga_socket = qga_socket
        self.debug = debug
        self.qmp = None
        self.qmpga = None
        self.qmp = qemu.qmp.QMPClient("bqmp")
        self.nbd_ipaddr = "127.0.0.1"
        self.nbd_port   = "10789"
        self.nbd_restore_port = "10809"
        self.quiesce = True
        self.incremental = False
        self.virtual_disks = {}

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
        result = await self.qmp.execute(cmd, args)
        if self.debug:
            print(cmd, args, result)
        return result

    async def freeze_guest(self):
        if self.qmpga is None:
            return {}
        return await self.qmpga.execute("guest-fsfreeze-freeze", {})

    async def thaw_guest(self):
        if self.qmpga is None:
            return {}
        return await self.qmpga.execute("guest-fsfreeze-thaw", {})

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
                     "sync": "top",
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

    async def wait_backup_job(self):
        while True:
            job_info = await self.qmp_execute("query-block-jobs")
            if job_info is None or len(job_info) == 0:
                break
            wanted_job = [ job for job in job_info if job["device"].startswith("backup-") ]
            if len(wanted_job) == 0:
                    break
            await asyncio.sleep(3)

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
        return self.virtual_disks[get_node_name(diskname)]

    def get_virtual_disk(self, nodename):
        return self.virtual_disks[get_disk_name(nodename)]

    async def add_bitmap(self, nodename, persistent):
        result = await self.qmp_execute("block-dirty-bitmap-add", {
            "node": nodename,
            "name": "bqmp-bitmap",
            "persistent": persistent
        })

    async def setup_bitmap(self, nodename):
        disk = self.get_virtual_node(nodename)

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
        self.nbd_restore_port = acquire_port(self.nbd_restore_port)
        result = await self.qmp_execute("nbd-server-start", {
            "addr": {
               "type": "inet",
               "data": {
                  "host": "127.0.0.1",
                  "port": self.nbd_restore_port
               }
            }              
        })

    async def stop_nbd_server(self):
        release_port(self.nbd_restore_port)
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


async def run_backup(bqmp, devices, nbd_targets, incremental):
    try:
        devicenodes = [ d for n,d,u in devices ]
        if not incremental:
            await bqmp.start_full_backup(devicenodes, nbd_targets)
        else:
            await bqmp.start_incr_backup(devicenodes, nbd_targets)
    finally:
        await bqmp.thaw_guest()
    await bqmp.wait_backup_job()


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


async def main(qmp_path, qga_path, devicenames,
        cmd, store_path, imgrestore_path):
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

    bqmp = BQMPFacade(qmp_path, qga_path, debug=debug)
    try:
        await bqmp.connect()
    except qemu.qmp.protocol.ConnectError as e:
        raise Exception(f"failed to connect to QMP socket at {qmp_path}")

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
        #await bqmp.test_query_block()
        await bqmp.test_query_nodes()
        await bqmp.test_query_jobs()
        try:
            await bqmp.stop_nbd_server()
        except:
            pass

        if cmd == CMD_RESET:
            for name, devicenode, upper_node in devices:
                try:
                    await bqmp.remove_bitmap(devicenode)
                except:
                    pass

        for name in devicenames:
            try:
                await bqmp.destroy_nbd_target(get_backup_target(name))
            except:
                pass

    elif cmd in [ CMD_BACKUP, CMD_INCR ]:
        incremental = (cmd == CMD_INCR)
        # doing a backup
        await bqmp.connect_ga()

        bqmp.nbd_port = acquire_port(bqmp.nbd_port)
        nbdtask = await run_nbd_server(bqmp.nbd_port, store_path, nbd_targets, incremental)

        logger.debug("nbd server started.")

        for devicename in devicenames:
            await bqmp.create_nbd_target(devicename)
 
        await bqmp.freeze_guest()

        try:
            await run_backup(bqmp, devices, nbd_targets, incremental)
        finally:
            for target in nbd_targets:
                logger.debug(f"destroy_nbd_target {target}")
                await bqmp.destroy_nbd_target(target[0])

            nbdtask.cancel()
            try:
                await nbdtask
            except asyncio.CancelledError:
                logger.debug("nbd server stopped.")
            release_port(bqmp.nbd_port)
    else:
        logger.warn("internal error, invalid command")
    try:
        await bqmp.disconnect()
    except EOFError as e:
        pass


async def run_nbd_server(port, path, nbd_targets, incr):
    ipaddr = "127.0.0.1"
    backing_store = None

    if path is None or path == "-":
        backing_store = bqmp_nbd.StdoutBackingStore(nbd_targets)
    else:
        backing_store = bqmp_nbd.BackingStore(path, nbd_targets)
        if not incr:
            backing_store.zap()

    backing_store.next()

    server = bqmp_nbd.NbdServer(ipaddr, int(port), backing_store)
    task = asyncio.create_task(server.serve_forever())
    return task


async def main_wrapper(qmp_path, qga_path, devicenames,
        cmd, store_path, imgrestore_path):
    try:
        await main(qmp_path, qga_path, devicenames, cmd, store_path, imgrestore_path)
    except Exception as e:
        raise(e)
        sys.stderr.write(str(e))
        sys.stderr.write('\n')
        sys.exit(3)


if __name__ == "__main__":
    if len(sys.argv) < 5:
        sys.stderr.write("invalid arguments\n")
        sys.exit(1)
    command = sys.argv[1]
    qmp_path = sys.argv[2]
    qga_path = sys.argv[3]
    disks = sys.argv[4].split(",")
    store_path = None
    imgrestore_path = None
    if len(sys.argv) > 5:
        store_path = sys.argv[5]
    if len(sys.argv) > 6:
        imgrestore_path = sys.argv[6]

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
        if store_path is None and sys.stdout.isatty():
            sys.stderr.write("no storage path given and stdout is a tty\n")
            sys.exit(2)

    count = 0
    asyncio.run(main_wrapper(qmp_path, qga_path,
        disks, op, store_path, imgrestore_path))
