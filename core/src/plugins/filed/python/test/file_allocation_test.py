#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2026-2026 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

import collections
import importlib.util
import os
import pathlib
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

import bareosfd

PYTHON_DIR = pathlib.Path(__file__).resolve().parents[1]
ROOT = pathlib.Path(__file__).resolve().parents[6]
sys.path.insert(0, str(PYTHON_DIR / "pyfiles"))


def load_plugin(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    with patch.dict(
        sys.modules,
        {
            "pg8000": types.SimpleNamespace(__version__="1.30.0"),
            "lockfile": types.SimpleNamespace(LockFile=object),
        },
    ):
        spec.loader.exec_module(module)
    return module


LOCAL = load_plugin(
    "allocation_local", PYTHON_DIR / "pyfiles/BareosFdPluginLocalFilesBaseclass.py"
).BareosFdPluginLocalFilesBaseclass
POSTGRES = load_plugin(
    "allocation_postgres", PYTHON_DIR / "postgresql/bareos-fd-postgresql.py"
).BareosFdPluginPostgreSQL
OPENVZ = load_plugin(
    "allocation_openvz",
    ROOT / "contrib/fd-plugins/openvz7/BareosFdPluginVz7CtFs.py",
).BareosFdPluginVz7CtFs


class FileAllocationTest(unittest.TestCase):
    def test_postgresql_virtual_file_unchanged(self):
        plugin = POSTGRES.__new__(POSTGRES)
        plugin.backup_label_filename = "/virtual/backup_label"
        plugin.paths_to_backup = collections.deque([plugin.backup_label_filename])
        plugin.virtual_files = [plugin.backup_label_filename]
        plugin.ref_statp = types.SimpleNamespace(st_mode=0o100600, st_uid=12, st_gid=34)
        plugin.backup_label_data = b"generated label"
        packet = bareosfd.SavePacket()
        with patch.object(bareosfd, "DebugMessage"):
            self.assertEqual(plugin.start_backup_file(packet), bareosfd.bRC_OK)
        self.assertEqual(packet.statp.st_size, len(plugin.backup_label_data))
        self.assertEqual(packet.statp.st_blocks, bareosfd.StatPacket().st_blocks)

    def backup_packet(self, plugin_class, filename):
        plugin = plugin_class.__new__(plugin_class)
        if plugin_class is LOCAL:
            plugin.files_to_backup = [filename]
            plugin._get_next_file_as_str = lambda: filename
        elif plugin_class is POSTGRES:
            plugin.paths_to_backup = collections.deque([filename])
            plugin.virtual_files = []
        else:
            plugin.files = collections.deque([filename])
        packet = bareosfd.SavePacket()
        source_lstat = os.lstat

        def legacy_lstat(path):
            source = source_lstat(path)
            values = {
                name: getattr(source, name)
                for name in dir(source)
                if name.startswith("st_")
            }
            # Isolate allocation from OpenVZ's legacy float timestamp handling.
            for name in ("st_atime", "st_mtime", "st_ctime"):
                values[name] = int(values[name])
            return types.SimpleNamespace(**values)

        with patch.object(bareosfd, "DebugMessage"), patch.object(
            bareosfd, "JobMessage"
        ):
            if plugin_class is OPENVZ:
                with patch("os.lstat", side_effect=legacy_lstat):
                    result = plugin.start_backup_file(packet)
            else:
                result = plugin.start_backup_file(packet)
            self.assertEqual(result, bareosfd.bRC_OK)
        return packet

    def test_filesystem_attributes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            regular = root / "regular"
            regular.write_bytes(b"x" * 131072)
            empty = root / "empty"
            empty.touch()
            sparse = root / "sparse"
            with sparse.open("wb") as stream:
                stream.seek(8 * 1024 * 1024)
                stream.write(b"x")
            link = root / "link"
            link.symlink_to(regular)
            for plugin in (LOCAL, POSTGRES, OPENVZ):
                for path in (regular, empty, sparse, link, root):
                    with self.subTest(plugin=plugin.__name__, path=path.name):
                        packet = self.backup_packet(plugin, str(path))
                        source = path.lstat()
                        self.assertEqual(packet.statp.st_size, source.st_size)
                        self.assertEqual(packet.statp.st_blocks, source.st_blocks)
                        self.assertEqual(packet.statp.st_blksize, source.st_blksize)
                        self.assertEqual(packet.statp.st_mode, source.st_mode)
                        if path == link:
                            self.assertEqual(packet.type, bareosfd.FT_LNK)
                        elif path == root:
                            self.assertEqual(packet.type, bareosfd.FT_DIREND)
                        else:
                            self.assertEqual(packet.type, bareosfd.FT_REG)

    def test_local_files_without_posix_allocation_fields(self):
        source = types.SimpleNamespace(
            st_mode=0o100600,
            st_size=123,
            st_ino=1,
            st_dev=1,
            st_nlink=1,
            st_uid=0,
            st_gid=0,
            st_atime=0,
            st_mtime=0,
            st_ctime=0,
        )
        with patch("os.stat", return_value=source):
            packet = self.backup_packet(LOCAL, "/regular")
        self.assertEqual(packet.statp.st_size, 123)
        self.assertEqual(packet.statp.st_blocks, bareosfd.StatPacket().st_blocks)


if __name__ == "__main__":
    unittest.main()
