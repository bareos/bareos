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
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.

import json
import os
import re
import subprocess


class PVEStorageConfig:
    """Parse Proxmox storage.cfg and build QSD restore targets."""

    STORAGE_CFG = "/etc/pve/storage.cfg"
    CEPH_CONFIG = "/etc/pve/ceph.conf"
    CEPH_PRIVATE_DIR = "/etc/pve/priv/ceph"

    def __init__(self, storage_cfg_path=None):
        self.storage_cfg_path = storage_cfg_path or self.STORAGE_CFG
        self.storages = {}

    def parse_config(self):
        self.storages = {}
        current_storage = None
        with open(self.storage_cfg_path) as storage_cfg:
            for raw_line in storage_cfg:
                line = raw_line.strip()
                if not line or line.startswith("#"):
                    continue

                if raw_line[0].isspace():
                    if current_storage is None:
                        raise ValueError("storage option found before storage type")
                    parts = line.split(None, 1)
                    if len(parts) != 2:
                        raise ValueError(f"invalid storage option: {line}")
                    current_storage["options"][parts[0]] = parts[1].strip()
                    continue

                storage_type, separator, storage_id = line.partition(":")
                if not separator or not storage_id.strip():
                    raise ValueError(f"invalid storage definition: {line}")
                storage_id = storage_id.strip()
                current_storage = {"type": storage_type, "options": {}}
                self.storages[storage_id] = current_storage

    @staticmethod
    def _relative_volume_path(volume):
        if (
            not volume
            or os.path.isabs(volume)
            or ".." in volume.replace("\\", "/").split("/")
        ):
            raise ValueError(f"invalid Proxmox volume name: {volume}")
        return volume

    @staticmethod
    def _file_format(volume):
        if volume.endswith(".qcow2"):
            return "qcow2"
        if volume.endswith(".raw"):
            return "raw"
        raise ValueError(
            f"unsupported file-backed volume format: {volume}; expected .qcow2 or .raw"
        )

    @staticmethod
    def _activate_lv(path):
        if not os.path.exists(path):
            subprocess.run(["/usr/sbin/lvchange", "-ay", path], check=True)

    @staticmethod
    def _parse_monitors(monhost):
        monitors = []
        for endpoint in re.split(r"[,;\s]+", monhost.strip()):
            if not endpoint:
                continue
            if endpoint.startswith("["):
                host_end = endpoint.find("]")
                if host_end == -1:
                    raise ValueError(f"invalid Ceph monitor address: {endpoint}")
                host = endpoint[1:host_end]
                suffix = endpoint[host_end + 1 :]
                if suffix and not suffix.startswith(":"):
                    raise ValueError(f"invalid Ceph monitor address: {endpoint}")
                port = suffix[1:] if suffix else "3300"
            elif endpoint.count(":") == 1:
                host, port = endpoint.rsplit(":", 1)
            else:
                host, port = endpoint, "3300"
            if not host or not port:
                raise ValueError(f"invalid Ceph monitor address: {endpoint}")
            monitors.append({"host": host, "port": port})
        if not monitors:
            raise ValueError("Ceph rbd storage has an empty monhost setting")
        return monitors

    def _rbd_blockdev(self, node_name, storage_id, volume, options):
        if options.get("krbd", "0").lower() in ("1", "yes", "true", "on"):
            raise ValueError(
                f"Ceph storage {storage_id} has krbd enabled; QSD restore "
                "currently requires the librbd backend"
            )

        keyring = os.path.join(self.CEPH_PRIVATE_DIR, storage_id + ".keyring")
        has_keyring = os.path.exists(keyring)
        monhost = options.get("monhost")
        blockdev = {
            "driver": "rbd",
            "node-name": "file_" + node_name,
            "pool": options.get("pool") or "rbd",
            "image": volume,
        }

        if monhost:
            conf = os.path.join(self.CEPH_PRIVATE_DIR, storage_id + ".conf")
            if has_keyring and not os.path.exists(conf):
                raise FileNotFoundError(
                    f"Ceph configuration for storage {storage_id} is missing: {conf}"
                )
            blockdev["server"] = self._parse_monitors(monhost)
            blockdev["auth-client-required"] = ["cephx" if has_keyring else "none"]
            if os.path.exists(conf):
                blockdev["conf"] = conf
        else:
            if not os.path.exists(self.CEPH_CONFIG):
                raise FileNotFoundError(
                    f"Proxmox Ceph configuration is missing: {self.CEPH_CONFIG}"
                )
            blockdev["conf"] = self.CEPH_CONFIG

        namespace = options.get("namespace")
        if namespace:
            blockdev["namespace"] = namespace
        if has_keyring:
            blockdev["user"] = options.get("username") or "admin"
        return blockdev

    def get_qsd_args(self, node_name, pve_diskspec):
        storage_id, separator, volume = pve_diskspec.partition(":")
        if not separator or not storage_id or not volume:
            raise ValueError(f"invalid Proxmox storage volume: {pve_diskspec}")

        try:
            storage = self.storages[storage_id]
        except KeyError as error:
            raise ValueError(f"unknown Proxmox storage: {storage_id}") from error

        volume = self._relative_volume_path(volume)
        storage_type = storage["type"]
        options = storage["options"]
        export_name = "bqmp-" + node_name

        if storage_type == "rbd":
            backing_node = self._rbd_blockdev(node_name, storage_id, volume, options)
            return [
                "--blockdev",
                json.dumps(backing_node, separators=(",", ":")),
                "--blockdev",
                f"driver=raw,node-name={node_name},file=file_{node_name}",
                "--export",
                f"type=nbd,id={export_name},node-name={node_name},"
                f"name={export_name},writable=on",
            ]

        if storage_type == "zfspool":
            pool = options.get("pool")
            if not pool:
                raise ValueError(f"ZFS storage {storage_id} has no pool setting")
            full_path = os.path.join("/dev/zvol", pool, volume)
            backend_driver = "host_device"
            frontend_driver = "raw"
        elif storage_type in ("lvm", "lvmthin"):
            vgname = options.get("vgname")
            if not vgname:
                raise ValueError(f"LVM storage {storage_id} has no vgname setting")
            full_path = os.path.join("/dev", vgname, volume)
            self._activate_lv(full_path)
            backend_driver = "host_device"
            frontend_driver = "raw"
        elif "path" in options:
            full_path = os.path.join(options["path"], "images", volume)
            backend_driver = "file"
            frontend_driver = self._file_format(volume)
        else:
            raise ValueError(
                f"unsupported Proxmox storage type {storage_type} for "
                f"storage {storage_id}"
            )

        return [
            "--blockdev",
            f"driver={backend_driver},node-name=file_{node_name},"
            f"filename={full_path},cache.direct=on",
            "--blockdev",
            f"driver={frontend_driver},node-name={node_name},file=file_{node_name}",
            "--export",
            f"type=nbd,id={export_name},node-name={node_name},"
            f"name={export_name},writable=on",
        ]
