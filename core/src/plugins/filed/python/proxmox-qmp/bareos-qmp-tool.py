#!/usr/bin/env python3
# BAREOS - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2026-2026 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation and included
# in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

import argparse
import asyncio
import json
import re
import sys

import qemu.qmp

BITMAP_NAME = "bqmp-bitmap"
DISK_NODE_NAME = re.compile(r"drive-(?:scsi|virtio|efidisk|ide|sata)\d+")


def parse_vmid(value):
    try:
        vmid = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("VMID must be a positive integer") from error
    if vmid < 1:
        raise argparse.ArgumentTypeError("VMID must be a positive integer")
    return vmid


def collect_bitmap_status(nodes, block_devices):
    nodes_by_name = {
        node["node-name"]: node
        for node in nodes
        if isinstance(node.get("node-name"), str)
    }
    statuses = []
    for block_device in block_devices:
        if block_device.get("removable", False):
            continue
        inserted = block_device.get("inserted") or {}
        node_name = inserted.get("node-name") or block_device.get("device")
        if not isinstance(node_name, str) or not DISK_NODE_NAME.fullmatch(node_name):
            continue

        node = nodes_by_name.get(node_name, {})
        bitmap = next(
            (
                item
                for item in (node.get("dirty-bitmaps") or [])
                if item.get("name") == BITMAP_NAME
            ),
            None,
        )
        inconsistent = bitmap.get("inconsistent", False) if bitmap else None
        statuses.append(
            {
                "disk": node_name[len("drive-") :],
                "node": node_name,
                "present": bitmap is not None,
                "consistent": not inconsistent if bitmap else None,
                "recording": bitmap.get("recording") if bitmap else None,
                "persistent": bitmap.get("persistent") if bitmap else None,
            }
        )
    return sorted(statuses, key=lambda status: status["disk"])


async def query_bitmap_status(socket_path):
    qmp = qemu.qmp.QMPClient("bareos-qmp-tool")
    await qmp.connect(socket_path)
    try:
        nodes = await qmp.execute("query-named-block-nodes")
        block_devices = await qmp.execute("query-block")
        return collect_bitmap_status(nodes, block_devices)
    finally:
        await qmp.disconnect()


def display_value(value):
    if value is None:
        return "unknown"
    return "yes" if value else "no"


def format_status(statuses, vmid, socket_path):
    lines = [
        f"Dirty bitmap status for VM {vmid} ({socket_path}):",
        "DISK       NODE                 BITMAP         CONSISTENT  RECORDING  PERSISTENT",
    ]
    for status in statuses:
        if not status["present"]:
            bitmap_state = "MISSING"
        elif not status["consistent"]:
            bitmap_state = "INCONSISTENT"
        else:
            bitmap_state = "present"
        lines.append(
            f"{status['disk']:<10} {status['node']:<20} "
            f"{bitmap_state:<14} "
            f"{display_value(status['consistent']):<11} "
            f"{display_value(status['recording']):<10} "
            f"{display_value(status['persistent'])}"
        )
    return "\n".join(lines)


def create_parser():
    parser = argparse.ArgumentParser(
        description="Inspect Bareos Proxmox QMP plugin state."
    )
    subcommands = parser.add_subparsers(dest="command", required=True)
    bitmap_status = subcommands.add_parser(
        "bitmap-status", help="show dirty bitmap status for a VM"
    )
    bitmap_status.add_argument("--vmid", required=True, type=parse_vmid)
    bitmap_status.add_argument(
        "--socket",
        help="QMP socket path (default: /var/run/qemu-server/<VMID>.qmp)",
    )
    bitmap_status.add_argument(
        "--json", action="store_true", help="write the status as JSON"
    )
    return parser


def main(argv=None):
    args = create_parser().parse_args(argv)
    socket_path = args.socket or f"/var/run/qemu-server/{args.vmid}.qmp"
    try:
        statuses = asyncio.run(query_bitmap_status(socket_path))
    except (
        OSError,
        qemu.qmp.protocol.ConnectError,
        qemu.qmp.qmp_client.ExecuteError,
    ) as error:
        print(f"QMP query failed: {error}", file=sys.stderr)
        return 2

    if args.json:
        json.dump(
            {"vmid": args.vmid, "socket": socket_path, "disks": statuses},
            sys.stdout,
            indent=2,
            sort_keys=True,
        )
        print()
    elif statuses:
        print(format_status(statuses, args.vmid, socket_path))
    else:
        print("No supported VM disk nodes found.", file=sys.stderr)
        return 1

    return int(
        not statuses
        or any(not status["present"] or not status["consistent"] for status in statuses)
    )


if __name__ == "__main__":
    sys.exit(main())
