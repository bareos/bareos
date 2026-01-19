#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# pylint: disable=invalid-name
"""This module provides the Incus FD plugin for Bareos"""

import ctypes
import dataclasses
import fcntl
import hashlib
import heapq
import io
import os
import queue
import stat
import subprocess
import sys
import tarfile
import tempfile
import threading
import time

from BareosFdWrapper import * # pylint: disable=import-error,wildcard-import
import bareosfd  # pylint: disable=import-error
import BareosFdPluginBaseclass  # pylint: disable=import-error


SIZE_MULTIPLIERS = {'k': 1_000, 'm': 1_000_000, 'g': 1_000_000_000, 't': 1_000_000_000_000,
                    'ki': 1_024, 'mi': 1_048_576, 'gi': 1_073_741_824, 'ti': 1_073_741_824_000}

TIME_MULTIPLIERS = {'s': 1, 'min': 60, 'h': 3_600, 'd': 86_400}

TAR_BOS = {tarfile.AREGTYPE: bareosfd.FT_REG,      tarfile.REGTYPE:  bareosfd.FT_REG,
           tarfile.LNKTYPE:  bareosfd.FT_LNKSAVED, tarfile.SYMTYPE:  bareosfd.FT_LNK,
           tarfile.CHRTYPE:  bareosfd.FT_SPEC,     tarfile.BLKTYPE:  bareosfd.FT_SPEC,
           tarfile.DIRTYPE:  bareosfd.FT_DIREND,   tarfile.FIFOTYPE: bareosfd.FT_SPEC,
           tarfile.CONTTYPE: bareosfd.FT_REG,      tarfile.GNUTYPE_SPARSE: bareosfd.FT_REG}

BOS_TAR = {bareosfd.FT_LNKSAVED: tarfile.LNKTYPE, bareosfd.FT_REGE: tarfile.REGTYPE,
           bareosfd.FT_REG:      tarfile.REGTYPE, bareosfd.FT_LNK:  tarfile.SYMTYPE,
           bareosfd.FT_DIREND:   tarfile.DIRTYPE}

STAT_TAR = {stat.S_IFIFO: tarfile.FIFOTYPE, stat.S_IFCHR: tarfile.CHRTYPE,
            stat.S_IFDIR: tarfile.DIRTYPE,  stat.S_IFBLK: tarfile.BLKTYPE,
            stat.S_IFREG: tarfile.REGTYPE,  stat.S_IFLNK: tarfile.SYMTYPE}

TAR_STAT = {tarfile.AREGTYPE: stat.S_IFREG, tarfile.REGTYPE:  stat.S_IFREG,
            tarfile.LNKTYPE:  stat.S_IFREG, tarfile.SYMTYPE:  stat.S_IFLNK,
            tarfile.CHRTYPE:  stat.S_IFCHR, tarfile.BLKTYPE:  stat.S_IFBLK,
            tarfile.DIRTYPE:  stat.S_IFDIR, tarfile.FIFOTYPE: stat.S_IFIFO,
            tarfile.CONTTYPE: stat.S_IFREG, tarfile.GNUTYPE_SPARSE: stat.S_IFREG}

TARFILE_MODES = {'bzip2': 'bz2', 'gzip': 'gz', 'lzma': 'xz', 'xz': 'xz', 'zstd': 'zst', 'none': ''}


def populate_tarinfo(tarinfo, pkt):
    """Populate a tarinfo from a Bareos packet"""
    if pkt.type in [bareosfd.FT_LNKSAVED, bareosfd.FT_LNK, bareosfd.FT_DIREND, bareosfd.FT_SPEC]:
        tarinfo.size = 0
    else:
        tarinfo.size = pkt.statp.st_size
    tarinfo.mtime = pkt.statp.st_mtime
    tarinfo.mode = pkt.statp.st_mode % 0o10000
    if pkt.type == bareosfd.FT_SPEC:
        tarinfo.type = STAT_TAR[stat.S_IFMT(pkt.statp.st_mode)]
    else:
        tarinfo.type = BOS_TAR[pkt.type]
    if pkt.type == bareosfd.FT_LNK:
        # XXX: we need to support hard links to properly handle containers
        tarinfo.linkname = pkt.olname
    tarinfo.uid = pkt.statp.st_uid
    tarinfo.gid = pkt.statp.st_gid
    # XXX: we need to support extended attributes to properly handle containers

def populate_pkt(pkt, tarinfo):
    """Populate a Bareos packet from a tarinfo"""
    pkt.statp.st_size = tarinfo.size
    pkt.statp.st_atime = tarinfo.mtime
    pkt.statp.st_ctime = tarinfo.mtime
    pkt.statp.st_mtime = tarinfo.mtime
    pkt.type = TAR_BOS[tarinfo.type]
    pkt.statp.st_mode = tarinfo.mode | TAR_STAT[tarinfo.type]
    if pkt.type == bareosfd.FT_LNK:
        pkt.link = tarinfo.linkname
    pkt.statp.st_uid = tarinfo.uid
    pkt.statp.st_gid = tarinfo.gid
    # XXX: we need to support hard links to properly handle containers
    # XXX: we need to support extended attributes to properly handle containers


class LogQueue:
    """
    Log queue formatting job messages. This works in two times, as sending job messages to the core
    needs to be done from the main thread.
    """
    def __init__(self):
        self.queue = queue.Queue()

    def put(self, line):
        """Put a new log line into the queue"""
        line_lower = line.lower()
        if line_lower.startswith('error'):
            self.queue.put((bareosfd.M_FATAL, line))
        elif line_lower.startswith('warn'):
            self.queue.put((bareosfd.M_WARNING, line))
        else:
            self.queue.put((bareosfd.M_INFO, line))

    def flush(self, rc=bareosfd.bRC_OK):
        """Flush the queue to the core. To be called from the main thread only."""
        try:
            while True:
                message = self.queue.get(False)
                if message[0] == bareosfd.M_ERROR:
                    rc = bareosfd.bRC_Error
                bareosfd.JobMessage(*message)
        except queue.Empty:
            return rc


class DataPipe:
    """
    Intermediary data pipe between the core and the chunk producer. This doesn't completely conform
    to the file API as it handles buffering and all read operations are full, but all the call sites
    expect this precise behavior.
    """
    def __init__(self):
        # The read side of the pipe
        self.r = None
        # The write side of the pipe
        self.w = None
        # The pipe buffer size
        self.size = 1024

    def open(self):
        """Open and tune the pipe"""
        self.r, self.w = os.pipe()
        with open('/proc/sys/fs/pipe-max-size', 'r', encoding='ascii') as file:
            self.size = int(file.read())
        fcntl.fcntl(self.w, fcntl.F_SETPIPE_SZ, self.size)

    def write(self, data, close=False):
        """Write data to the pipe"""
        try:
            if data is None:
                return
            length = len(data)
            offset = 0
            while offset < length:
                written = os.write(self.w, data[offset:offset+self.size])
                offset += written
        finally:
            if close:
                self.close_w()

    def read(self, close=False):
        """Read the data from the pipe"""
        with open(self.r, 'rb', closefd=close) as r:
            return r.read()

    def close_w(self):
        """Close the write side of the pipe"""
        return os.close(self.w)

    def close_r(self):
        """Close the read side of the pipe"""
        return os.close(self.r)


# pylint: disable=too-many-instance-attributes
class ChunkCollector:
    """
    Image chunk collector. This reorders unordered chunks, tries to fit them in RAM as much as
    possible, and dumps data to disk in case too many non-consecutive chunks are received.
    :param chunk_size: The size in bytes of a full chunk
    :param max_ram_chunks: The amount of chunks to store in RAM
    :param tmpdir: The optional directory in which to store temporary chunks
    """
    def __init__(self, chunk_size, max_ram_chunks, tmpdir=None):
        self.max_ram_chunks = max_ram_chunks
        self.dir = tmpdir
        # This class performs concurrent data access to mutable structures, so we use a reentrant
        # lock with a wake-up condition
        self.lock = threading.Condition()
        # The IDs of chunks in RAM are stored as a max-heap, for easy eviction (in practice, a
        # min-heap of negative indices)
        self.ram_chunk_ids = []
        # The IDs of chunks on disk are stored as a min-heap, for easy refresh to RAM
        self.disk_chunk_ids = []
        # RAM chunks are simply stored in RAM
        self.ram_chunks = {}
        # For disk chunks, we only store the open temporary file objects
        self.disk_chunk_fds = {}
        # We keep track of the zero chunks, without actually storing them
        self.zero_chunk_ids = set()
        # However, for fast memory copy, we store one full zero chunk
        self.zero_chunk = bytes(chunk_size)
        # To optimize the algorithm, we keep track of the next chunk to send
        self.next_chunk = 0
        # We keep a seekable byte array containing the blob being sent to minimize memmoves
        self.current_blob = bytearray()
        self.cur = 0

    def add(self, chunk_id, data):
        """
        Add a chunk to the collector. Because there can be (potentially many) empty chunks, we
        cannot rely on only the chunk ID to know whether the chunk should stay in RAM, so we move
        the clever bits to the consumer side.
        """
        with self.lock:
            if chunk_id == self.next_chunk:
                self.lock.notify()
            if data == b'':
                self.zero_chunk_ids.add(chunk_id)
            elif len(self.ram_chunks) < self.max_ram_chunks:
                # When the element fits in RAM
                # NOTE: indices are negated so that ram_chunk_ids behaves as a max-heap
                heapq.heappush(self.ram_chunk_ids, -chunk_id)
                self.ram_chunks[chunk_id] = data
            elif chunk_id < -self.ram_chunk_ids[0]:
                # When the element should replace another in RAM
                old_id = -heapq.heapreplace(self.ram_chunk_ids, -chunk_id)
                self.ram_chunks[chunk_id] = data
                self.add(old_id, self.ram_chunks.pop(old_id))
            else:
                # When the element should be saved on disk
                heapq.heappush(self.disk_chunk_ids, chunk_id)
                f = tempfile.TemporaryFile(dir=self.dir)
                f.write(data)
                f.seek(0)
                self.disk_chunk_fds[chunk_id] = f

    def ready(self):
        """Return whether the next chunk is ready for processing"""
        with self.lock:
            return self.next_chunk in self.zero_chunk_ids or self.next_chunk in self.ram_chunks

    def rebalance(self):
        """Fix and rebalance the internal data structures"""
        with self.lock:
            # Reconstruct the potentially corrupted heap
            heapq.heapify(self.ram_chunk_ids)
            # Refresh from disk. If we are lucky, it is still cached.
            if self.disk_chunk_ids:
                chunk_id = heapq.heappop(self.disk_chunk_ids)
                f = self.disk_chunk_fds[chunk_id]
                self.add(chunk_id, f.read())
                f.close()

    def read(self, n):
        """
        Read the next n bytes of data. This never sends an EOF indication, so the consumer has to
        read exactly what it needs. The implementation of tarfile in streaming mode reads exactly
        tarinfo.size bytes.
        """
        size = len(self.current_blob) - self.cur
        if size >= n:
            # In the default case, the data is already in our buffer. To minimize memmoves, we only
            # track and move the current cursor position in this buffer.
            old_cur = self.cur
            self.cur = old_cur + n
            return self.current_blob[old_cur:self.cur]
        blob = bytearray()
        while size < n:
            # In most cases, the loop should only refresh a single already-aligned chunk, but we
            # have no strong guarantee of it and can only hope the costly code paths are not too
            # frequently taken. To minimize memmoves, we defer allocations to self.current_blob as
            # much as possible.
            with self.lock:
                self.lock.wait_for(self.ready)
                if self.next_chunk in self.zero_chunk_ids:
                    blob.extend(self.zero_chunk)
                else:
                    blob.extend(self.ram_chunks.pop(self.next_chunk))
                    self.ram_chunk_ids.remove(-self.next_chunk)
                    self.rebalance()
                self.next_chunk += 1
            size = len(self.current_blob) + len(blob) - self.cur
        data = bytearray(self.current_blob[self.cur:])
        self.cur = 0
        if n == size:
            self.current_blob = bytearray()
            data.extend(blob)
        else:
            self.current_blob = blob[n-size:]
            data.extend(blob[:n-size])
        return data


def _str(value):
    """No-op string option converter"""
    return value.strip()

def _enum(*values):
    """Enum option converter"""
    _values = [value.lower() for value in values]
    def f(value):
        value_lower = value.strip().lower()
        if value_lower in _values:
            return value_lower
        raise ValueError(f'one of "{'", "'.join(_values)}"')
    return f

def _bool(value):
    """Boolean option converter"""
    s = value.strip().lower()
    if s in ['yes', 'y', 'on', 'true', '1']:
        return True
    if s in ['no', 'n', 'off', 'false', '0']:
        return False
    raise ValueError("a valid boolean")

def _size(value):
    """Size option converter"""
    s = value.strip().lower()
    if s.endswith('b'):
        s = s[:-1]
    suffix = s[-2:]
    if suffix in ['ki', 'mi', 'gi', 'ti']:
        s = s[:-2]
    else:
        suffix = s[-1:]
        if suffix in ['k', 'm', 'g', 't']:
            s = s[:-1]
        else:
            suffix = None
    mul = SIZE_MULTIPLIERS.get(suffix, 1)
    try:
        return int(s.strip()) * mul
    except ValueError as e:
        raise ValueError("a valid size in bytes") from e

def _int(value):
    """Integer option converter"""
    try:
        return int(value.strip())
    except ValueError as e:
        raise ValueError("a valid integer") from e

def _time(value):
    """Time option converter"""
    s = value.strip().lower()
    suffix = s[-1:]
    if suffix in ['s', 'h', 'd']:
        s = s[:-1]
    elif s.endswith('min'):
        suffix = 'min'
        s = s[:-3]
    else:
        suffix = None
    mul = TIME_MULTIPLIERS.get(suffix, 1)
    try:
        return int(s.strip()) * mul
    except ValueError as e:
        raise ValueError("a valid time in seconds") from e

def _set(type_):
    """Set option converter"""
    def f(value):
        result = []
        seen = set()
        for v in value.split(':'):
            parsed = type_(v)
            if parsed in seen:
                raise ValueError("a list without duplicates values")
            seen.add(parsed)
            result.append(parsed)
        return result
    return f


class UnknownOptionException(Exception):
    """Raised when using an unknown option"""


class BareosFdIncusOptions:
    """Incus plugin options"""
    def __init__(self):
        self._options = {
            # Common options
            ## Required instance selector
            'instance': self.make_opt(_str),
            ## Optional instance selectors
            'project': self.make_opt(_str, ''),
            'remote': self.make_opt(_str, ''),
            ## Image chunking
            'allow_disk_resize': self.make_opt(_bool, 'no'),
            'chunk_size': self.make_opt(_size, '64MiB'),
            'chunk_id_length': self.make_opt(_int, '8'),
            ## RAM tuning (the queue takes at most chunk_size * [chunk_size + a few metadata])
            'queue_depth': self.make_opt(_int, '8'),
            ## Data compression (lz4 is supported by Incus but not tarfile, zstd is only supported
            ## on Python 3.14+)
            'compression': self.make_opt(_enum(*TARFILE_MODES), 'none'),

            # Backup options
            'backup_poll_timeout': self.make_opt(_time, '1h'),
            'hash': self.make_opt(_enum(*hashlib.algorithms_guaranteed), 'sha256'),
            'hijacked_stat_fields': self.make_opt(_set(_enum('st_ino', 'st_atime', 'st_mtime',
                                                             'st_ctime')),
                                                  'st_ino:st_atime:st_mtime:st_ctime'),

            # Restore options
            'buffer_depth': self.make_opt(_int, '8'),
            'restore_instance': self.make_opt(_str, ''),
            'restore_path': self.make_opt(_str, ''),
            'restore_project': self.make_opt(_str, ''),
            'restore_remote': self.make_opt(_str, ''),
            'restore_storage': self.make_opt(_str, ''),
            'temp_dir': self.make_opt(_str, ''),
        }

    @staticmethod
    def make_opt(type_, default=None):
        """Initialize a plugin option"""
        if default is not None:
            default = type_(default)
        return {'type': type_, 'value': default, 'value_set': False}

    def check(self):
        """Check required options"""
        rc = bareosfd.bRC_OK
        for name, option in self._options.items():
            if not option['value_set'] and option['value'] is None:
                rc = bareosfd.bRC_Error
                bareosfd.JobMessage(bareosfd.M_FATAL, f'Mandatory option "{name}" not defined.\n')
        return rc

    def __contains__(self, key):
        """Check if the given option is set"""
        try:
            return self._options[key]['value_set']
        except KeyError as e:
            raise UnknownOptionException(key) from e

    def __setitem__(self, key, value):
        """Set the given option"""
        try:
            opt = self._options[key]
            opt['value'] = opt['type'](value)
            opt['value_set'] = True
        except KeyError as e:
            raise UnknownOptionException(key) from e
        except ValueError as e:
            msg = f'Invalid value "{value}" for option "{key}". Expected {e.args[0]}.'
            raise ValueError(msg) from e

    def __getitem__(self, key):
        """Get the given option"""
        try:
            return self._options[key]['value']
        except KeyError as e:
            raise UnknownOptionException(key) from e

    def __getattr__(self, key):
        """Get the given option"""
        try:
            return self._options[key]['value']
        except KeyError as e:
            raise UnknownOptionException(key) from e


@dataclasses.dataclass(frozen=True, slots=True)
class ErrorEntry:
    """Error queue entry"""
    err: str

@dataclasses.dataclass(frozen=True, slots=True)
class RegularEntry:
    """Regular file queue entry"""
    name: str
    data: bytes
    tarinfo: tarfile.TarInfo

@dataclasses.dataclass(frozen=True, slots=True)
class ChunkEntry:
    """Image chunk queue entry"""
    name: str
    data: bytes = b''
    digest: bytes = b''
    size: int = 0


def fail(msg):
    """Fail with the given message"""
    bareosfd.JobMessage(bareosfd.M_FATAL, f'{msg}\n')
    return bareosfd.bRC_Error


def please_open_an_issue(cond):
    """Ask the user to open an issue for an unhandle case"""
    return fail(
        "The Incus FD plugin was asked to perform an operation that has not been implemented. "
        f"Please open an issue indicating that you have encountered the error condition {cond}, "
        "with details about your setup."
    )


@BareosPlugin  # pylint: disable=undefined-variable
class BareosFdIncus(BareosFdPluginBaseclass.BareosFdPluginBaseclass):
    """Plugin main class"""
    def __init__(self, plugindef):
        self.options = BareosFdIncusOptions()
        super().__init__(plugindef)

        # This is needed in order to receive restore end events, to properly cleanup and join
        # running threads and processes. In case it ever gets implemented, we also ask to receive
        # restore start events.
        bareosfd.RegisterEvents([bareosfd.bEventStartRestoreJob, bareosfd.bEventEndRestoreJob])

        self.is_backup = False
        self.is_full = False

        # The incus export/import process
        self.incus_process = None
        # The log queue it fills
        self.log_queue = LogQueue()
        # File processing queue, keeping track of error, regular files and image chunk records
        self.queue = None
        # Intermediate pipe that behaves both like a pipe and a pair of files
        self.data_pipe = DataPipe()

        # Backup-specific variables
        ## The current record being backed up
        self.current_record = None
        ## The precomputed digest for all-zero blocks
        self.zero_digest = None
        ## We keep an open handle to /dev/null to pass to the core when handling zero chunks. We
        ## never close it, but there is no strong need to.
        self.devnull = open('/dev/null', 'rb')  # pylint: disable=consider-using-with

        # Restore-specific variables
        ## The running chunk collectors
        self.collectors = {}
        ## The running chunk collection thread
        self.chunk_io_thread = None
        ## The current image being reconstructed from chunks
        self.current_image = None
        ## The TAR being reconstructed
        self.tar = None
        ## The current file writer
        self.current_writer = None
        ## The thread reading the data pipe into current_writer
        self.restore_io_thread = None

    def check_options(self, mandatory_options=None):
        """Check the options provided by the core"""
        del mandatory_options
        # First, check the minimal Python version. We only support Python ⩾ 3.13.
        if sys.version_info < (3, 13):
            return fail('Python version 3.13 or above is required to run this plugin')
        # Prevent the user from doing very unoptimized tuning
        if self.options.chunk_size % 262_144:
            return fail('"chunk_size" must be a multiple of 256kiB')
        # This option does absolutely nothing, but is a good reminder that this feature would be
        # nice to have if enough users ask for it
        if self.options.allow_disk_resize:
            return fail('Clever handling of disk resize is currently not supported by this plugin')
        # Support for zstd only appeared in Python 3.14
        if sys.version_info < (3, 14) and self.options.compression == 'zstd':
            return fail('ZSTD compression is not supported on your version of Python')
        # Alert the user if the chosen hash algorithm takes more bits than available in the hijacked
        # stat fields
        digest_bits = hashlib.new(self.options.hash, usedforsecurity=False).digest_size * 8
        stat_bits = len(self.options.hijacked_stat_fields) * 64
        if digest_bits > stat_bits:
            bareosfd.JobMessage(
                bareosfd.M_WARNING,
                f'The chosen hash "{self.options.hash}" will be truncated from {digest_bits} to '
                f'{stat_bits} bits to fit the stat struct\n'
            )
        # Restoring on disk is mutually incompatible with setting a restore override
        if self.options.restore_path and any(self.options[f'restore_{k}']
                                             for k in ['instance', 'project', 'remote', 'storage']):
            return fail(
                '"restore_path" is incompatible with either "restore_instance", "restore_project", '
                '"restore_remote", or "remote_storage"'
            )
        return self.options.check()

    # pylint: disable=too-many-return-statements
    def parse_plugin_definition(self, plugindef):
        """Parse the plugin arguments"""
        try:
            rc = super().parse_plugin_definition(plugindef)
            if rc != bareosfd.bRC_OK:
                return rc
        except UnknownOptionException as e:
            return fail(f"Unknown option '{e}' passed to plugin")
        except ValueError as e:
            return fail(f"{e}\n")

        level = chr(self.level)
        if level == 'F':
            self.is_backup = True
            self.is_full = True
            return bareosfd.bRC_OK

        if level == ' ':
            return bareosfd.bRC_OK

        if level in ['I', 'D']:
            self.is_backup = True
            if bareosfd.GetValue(bareosfd.bVarAccurate):
                return bareosfd.bRC_OK
            return fail("Accurate mode is required for Incremental and Differential backups")

        return fail("Only Full/Incremental/Differential backups and Restore are supported")

    def hash(self, data):
        """Compute the digest of the given data"""
        return hashlib.new(self.options.hash, data, usedforsecurity=False).digest()

    def start_backup_job(self):
        """Start a backup job"""
        self.queue = queue.Queue(maxsize=int(self.options.queue_depth))

        if self.options.remote:
            instance = f'{self.options.remote}:{self.options.instance}'
        else:
            instance = self.options.instance
        cmd = ['incus', 'export', instance, '-', '--instance-only', '--quiet', '--compression',
               self.options.compression]
        if self.options.project:
            cmd.extend(['--project', self.options.project])

        bareosfd.JobMessage(bareosfd.M_INFO, f"Executing {' '.join(cmd)}\n")
        # pylint: disable=consider-using-with
        self.incus_process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

        threading.Thread(target=self.process_stdout, args=(self.incus_process.stdout,),
                         daemon=True).start()
        threading.Thread(target=self.process_stderr, args=(self.incus_process.stderr,),
                         daemon=True).start()
        self.zero_digest = self.hash(bytes(self.options.chunk_size))
        return bareosfd.bRC_OK

    def start_restore_job(self):
        """Start a restore job"""
        # TODO: Here, the queue is unbounded. It would be safer to implement eviction to disk, but
        # we consider that the files are small enough for it not to cause problems. Still, it would
        # be an interesting safety mechanism.
        self.queue = queue.Queue()
        if self.options.restore_path:
            bareosfd.JobMessage(bareosfd.M_INFO, f"Restoring into {self.options.restore_path}\n")
            # pylint: disable=consider-using-with
            self.tar = tarfile.open(fileobj=open(self.options.restore_path, 'wb'),
                                    mode=f'w|{TARFILE_MODES[self.options.compression]}',
                                    bufsize=1_048_576)
            return bareosfd.bRC_OK

        remote = self.options.restore_remote or self.options.remote
        instance = self.options.restore_instance or self.options.instance
        project = self.options.project or self.options.restore_project
        cmd = ['incus', 'import']
        if remote:
            cmd.append(f'{remote}:')
        cmd.extend(['-', instance])
        if project:
            cmd.extend(['--project', project])
        if self.options.restore_storage:
            cmd.extend(['--storage', self.options.restore_storage])

        bareosfd.JobMessage(bareosfd.M_INFO, f"Executing {' '.join(cmd)}\n")
        # pylint: disable=consider-using-with
        self.incus_process = subprocess.Popen(cmd, stdin=subprocess.PIPE, stderr=subprocess.PIPE)
        # pylint: disable=consider-using-with
        self.tar = tarfile.open(fileobj=self.incus_process.stdin,
                                mode=f'w|{TARFILE_MODES[self.options.compression]}',
                                bufsize=1_048_576)
        threading.Thread(target=self.process_stderr, args=(self.incus_process.stderr,),
                         daemon=True).start()
        return bareosfd.bRC_OK

    def end_restore_job(self):
        """End a restore job"""
        if self.chunk_io_thread is not None:
            self.chunk_io_thread.join()
        self.queue.shutdown()
        try:
            while True:
                entry = self.queue.get()
                self.tar.addfile(entry.tarinfo, io.BytesIO(entry.data))
        except queue.ShutDown:
            pass
        self.tar.close()
        if self.incus_process is not None:
            # Incus reacts to EOF, the zero-blocks are not enough so we need to close the pipe
            self.incus_process.stdin.close()
            self.incus_process.wait()
        # We are waiting up to the end to be extra sure we catch everything
        return self.log_queue.flush()

    def start_backup_file(self, savepkt):
        """Process a Bareos save packet"""
        # Pull the next entry
        try:
            entry = self.queue.get(timeout=self.options.backup_poll_timeout)
        except queue.Empty:
            return fail(f"No data received from Incus after {self.options.export_start_timeout}s")
        except queue.ShutDown:
            return bareosfd.bRC_Stop

        if isinstance(entry, ErrorEntry):
            self.log_queue.flush()
            return fail(f"incus export failed: {entry.err}")

        # Prepare savepkt
        savepkt.statp = bareosfd.StatPacket()
        savepkt.fname = entry.name

        if isinstance(entry, ChunkEntry):
            savepkt.type = bareosfd.FT_REG
            savepkt.statp.st_size = entry.size
            for (offset, field) in enumerate(self.options.hijacked_stat_fields):
                setattr(savepkt.statp, field,
                        ctypes.c_int64(int.from_bytes(entry.digest[8*offset:8*(offset+1)])).value)
        else:
            populate_pkt(savepkt, entry.tarinfo)

        self.current_record = entry
        return bareosfd.bRC_OK

    def is_chunk(self, fname):
        """Return whether the currently processed file is a chunk"""
        path = os.path.split(fname)
        return len(path) == 2 and path[1].endswith('.chunk')

    def create_file(self, restorepkt):
        """Process a Bareos restore packet"""
        if self.tar is None:
            self.start_restore_job()
        restorepkt.create_status = bareosfd.CF_EXTRACT
        fname = f'backup{restorepkt.ofname.removeprefix(f'@INCUS/{self.options.instance}')}'
        if self.is_chunk(fname):
            # Here, we initialize the chunk collector
            fname, chunk_str = fname.removesuffix('.chunk').rsplit('-', 1)
            chunk_id = int(chunk_str)
            if fname not in self.collectors:
                if self.collectors:
                    # At the time this plugin was written, Incus exposed a single raw image when
                    # performing an image-only export, and this behavior wasn't planned to change.
                    # To simplify the unchunking logic a bit, concurrent unchunking is not handled,
                    # still, if the behavior were to change, we wouldn't have to dramatically change
                    # the code structure.
                    return please_open_an_issue(1)
                self.collectors[fname] = ChunkCollector(self.options.chunk_size,
                                                        self.options.buffer_depth,
                                                        self.options.temp_dir)
            if chunk_id == 0:
                # We consider that receiving the first chunk of data should be the trigger to switch
                # to chunk streaming. This may not be the wisest heuristic, but it certainly is the
                # easiest to implement.
                self.current_image = fname
                tarinfo = tarfile.TarInfo(fname)
                # For later processing, we do not store the chunk size but the whole image's. This
                # should change, were we to implement allow_disk_resize.
                tarinfo.size = restorepkt.statp.st_size
                tarinfo.mtime = int(time.time())
                tarinfo.mode = 0o600
                tarinfo.type = tarfile.REGTYPE
                tarinfo.uid = 0
                tarinfo.gid = 0
                def collect():
                    self.tar.addfile(tarinfo, self.collectors[fname])
                    self.current_image = None
                    self.chunk_io_thread = None
                self.chunk_io_thread = threading.Thread(target=collect, daemon=True)
                self.chunk_io_thread.start()
            def writer():
                self.collectors[fname].add(chunk_id, self.data_pipe.read(True))
            self.current_writer = writer
            return bareosfd.bRC_OK
        tarinfo = tarfile.TarInfo(fname)
        populate_tarinfo(tarinfo, restorepkt)
        if self.current_image is not None:
            # The collector is currently streaming into our TAR, so we have to buffer the data.
            # pylint: disable=function-redefined
            def writer():
                self.queue.put(RegularEntry(fname, self.data_pipe.read(True), tarinfo))
            self.current_writer = writer
            return bareosfd.bRC_OK
        # Here, we can bypass the DataPipe buffering and directly connect to the read pipe
        # pylint: disable=function-redefined
        def writer():
            with open(self.data_pipe.r, 'rb') as r:
                self.tar.addfile(tarinfo, r)
        self.current_writer = writer
        return bareosfd.bRC_OK

    def end_backup_file(self):
        """
        Bareos doesn't complain when we return bRC_Stop after bRC_More, so it's easier to implement
        this way
        """
        return self.log_queue.flush(bareosfd.bRC_More)

    def should_chunk(self, tarinfo):
        """Return whether the currently processed TAR member should be chunked"""
        path = os.path.split(tarinfo.name)
        return (len(path) == 2 and path[0] == 'backup' and path[1].endswith('.img') and
                tarinfo.size > self.options.chunk_size)

    def process_stdout(self, stdout):
        """Parse the incus export tar stream continuously"""
        try:
            # pylint: disable=consider-using-with
            tar = tarfile.open(fileobj=stdout, mode="r|*", bufsize=1_048_576)
        except Exception as e: # pylint: disable=broad-exception-caught
            self.queue.put(ErrorEntry(f"Cannot open export stream as a tar: {e}"))
            return
        root = f"@INCUS/{self.options.instance}"
        # Iterate over all members and chunk disk images
        for tarinfo in tar:
            dst = f'{root}{tarinfo.name.removeprefix('backup')}'
            if tarinfo.isreg():
                f = tar.extractfile(tarinfo)
            else:
                f = None
            if self.should_chunk(tarinfo):
                chunk_id = 0
                while True:
                    data = f.read(self.options.chunk_size)
                    if not data:
                        break
                    name = f"{dst}-{chunk_id:0{self.options.chunk_id_length}d}.chunk"
                    if data.count(0) == len(data):
                        if len(data) == self.options.chunk_size:
                            digest = self.zero_digest
                        else:
                            digest = self.hash(data)
                        self.queue.put(ChunkEntry(name, digest=digest, size=tarinfo.size))
                    else:
                        self.queue.put(ChunkEntry(name, data, self.hash(data), tarinfo.size))
                    chunk_id += 1
                continue
            data = None if f is None else f.read()
            self.queue.put(RegularEntry(dst, data, tarinfo))
        self.queue.shutdown()

    def process_stderr(self, stderr):
        """Process Incus commands' stderr"""
        for line in iter(stderr.readline, b''):
            self.log_queue.put(line.decode('utf-8'))

    def handle_plugin_event(self, event):
        """Add hooks to specific events"""
        if event == bareosfd.bEventCancelCommand:
            if self.incus_process and self.incus_process.poll() is None:
                try:
                    self.incus_process.terminate()
                except Exception: # pylint: disable=broad-exception-caught
                    pass
            if self.queue is not None:
                self.queue.put(ErrorEntry("job cancelled"))
            return bareosfd.bRC_OK
        if event == bareosfd.bEventStartBackupJob:
            return self.start_backup_job()
        if event == bareosfd.bEventStartRestoreJob:
            # NOTE: This event is never received, but in case it ever is, we can handle it
            return self.start_restore_job()
        if event == bareosfd.bEventEndRestoreJob:
            return self.end_restore_job()
        return bareosfd.bRC_OK

    def plugin_io_open(self, iop):
        """Prepare the handle to provide to the core"""
        iop.status = bareosfd.iostat_do_in_core

        if self.is_backup:
            # In the case of backups, we expose a read pipe to the core fed by the popped queue
            # record.
            if self.current_record is None:
                iop.filedes = self.devnull.fileno()
                return bareosfd.bRC_OK
            self.data_pipe.open()
            iop.filedes = self.data_pipe.r
            threading.Thread(target=self.data_pipe.write, args=(self.current_record.data, True),
                             daemon=True).start()
            return bareosfd.bRC_OK

        # In the case of restores, we expose a write pipe to the core that feeds our queue.
        self.data_pipe.open()
        iop.filedes = self.data_pipe.w
        self.restore_io_thread = threading.Thread(target=self.current_writer, daemon=True)
        self.restore_io_thread.start()
        return bareosfd.bRC_OK

    def plugin_io_close(self, iop):
        """Close the handle provided to the core"""
        del iop
        if self.is_backup:
            self.data_pipe.close_r()
        else:
            # This triggers EOF, unblocking the read operation.
            self.data_pipe.close_w()
            # We need to wait for the IO thread to finish, else this can get dangerously racy.
            self.restore_io_thread.join()
        return bareosfd.bRC_OK

    def plugin_io_seek(self, iop):
        """Handled by the core"""
        raise NotImplementedError

    def plugin_io_read(self, iop):
        """Handled by the core"""
        raise NotImplementedError

    def plugin_io_write(self, iop):
        """Handled by the core"""
        raise NotImplementedError
