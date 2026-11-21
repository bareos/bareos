#!/usr/bin/env python
"""
Bareos QMP Backup NBD Server Module

based on

swiftnbd. server module
Copyright (C) 2013-2016 by Juan J. Martinez <jjm@usebox.net>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
"""

import sys
import os
import struct
import io
import logging
import glob
import signal
import asyncio
import nbd

logger = logging.getLogger(__package__)

class AbortedNegotiationError(IOError):
    pass

BCHUNK_MAGIC = 0x4b4e5842
HEADER_FORMAT = '<LHHQQ'
BCHUNK_OP_DATA = 1
BCHUNK_OP_ZEROES = 12

class BackingStore:
    """
    This BackingStore class will write a bchunks
    file from incoming NBD writes.
    A bchunk file will be written to the specified
    directory. If the directory already contains
    bchunk file, a new file will be added, enabling
    incremental backups.
    The directory can be cleared with the zap()
    method.

    Used for backups.
    """

    def __init__(self, path, exports):
        self.mode = "wb"
        self.dir = path
        self.path = path + "/bqmp-{:02d}.bchunks"
        self.serial = 0
        self.file = None
        self.exports = exports # array of (name, size in bytes) tuples
        self.size = 0
        self.check_path()
        logger.debug("backing store initialized at {}, {} items found.".format(self.dir, self.size))

    # check if there already are chunk files
    def check_path(self):
        chunkfiles = glob.glob(self.dir + "/bqmp-*.bchunks") 
        self.size = len(chunkfiles)
        self.serial = self.size

    # remove any chunk files
    def zap(self):
        chunkfiles = glob.glob(self.dir + "/bqmp-*.bchunks") 
        for f in chunkfiles:
            os.remove(f)
        self.size = 0
        self.serial = 0
        logger.debug("backing store at {} zapped.".format(self.dir))

    # move to next chunk file
    def next(self):
        self.close()
        self.serial += 1
        filename = self.get_filename()
        self.file = open(filename, self.mode)
        logger.debug("opened",filename)

    # move back to first chunk file
    def reset(self):
        self.close()
        self.serial = 0
        self.next()

    def get_filename(self):
        return self.path.format(self.serial)

    # close BackingStore object
    def close(self):
        if self.file is not None:
            self.file.close()
            self.file = None

    def write(self, index, offset, data):
        header_data = struct.pack(HEADER_FORMAT, BCHUNK_MAGIC,
            BCHUNK_OP_DATA, index, offset, len(data))
        self.file.write(header_data)
        self.file.write(data)

    def zero(self, index, offset, length):
        header_data = struct.pack(HEADER_FORMAT, BCHUNK_MAGIC,
            BCHUNK_OP_ZEROES, index, offset, length)
        self.file.write(header_data)

    async def init_async(self):
        pass

    async def close_async(self):
        self.file.close()

    async def write_async(self, index, offset, data):
        self.write(index, offset, data)

    async def zero_async(self, index, offset, length):
        self.zero(index, offset, length)

    async def shutdown_async(self):
        pass

    # switch to dumping chunk data
    def start_dump(self):
        self.close()
        self.mode = "rb"
        self.serial = 0

    # dump contents of current chunk file into raw image
    def dump(self, targets, write_handler):
        header_size = struct.calcsize(HEADER_FORMAT)
        self.next()

        device_count = len(self.exports)

        while True:
            header_data = self.file.read(header_size)
            if len(header_data) == 0:
                break
            magic, op, devno, offset, length = struct.unpack(HEADER_FORMAT, header_data)
            if devno >= device_count:
                raise Exception(f"invalid device number {devno} in chunk")
            size_limit = self.exports[devno][1]
            if magic != BCHUNK_MAGIC or offset > size_limit or length > size_limit:
                raise Exception("error decoding chunk header")
            if op == BCHUNK_OP_DATA:
                data = self.file.read(length)
            else:
                data = None
            write_handler(targets[devno], offset, data, length)


class StdoutBackingStore(BackingStore):
    """
    This BackingStore class will write a bchunks
    stream to stdout from incoming NBD writes.
    Used for backups.
    """
    def __init__(self, exports):
        self.mode = "wb"
        self.exports = exports
        self.file = None

    # move to next chunk file
    def next(self):
        self.file = io.open(sys.stdout.fileno(), 'wb', closefd=False)

    def close(self):
        pass

    def zap(self):
        pass


class StdinBackingStore(BackingStore):
    """ This BackingStore class will read a bchunks stream
        from stdin. Used for restores.
    """

    def __init__(self, exports):
        self.mode = "rb"
        self.exports = exports
        self.file = None
        self.size = 1

    # move to next chunk file
    def next(self):
        self.file = io.open(sys.stdin.fileno(), 'rb', closefd=False)

    def close(self):
        pass

    def zap(self):
        pass

    def start_dump(self):
        pass


class AsyncBufferedPipe:
    def __init__(self, maxsize, output):
        self.queue = asyncio.Queue(maxsize=maxsize)
        self.output = output
        self.task = None
        self.task_is_done = False

    async def start(self):
        self.task = asyncio.create_task(self._consumer_task())
        self.task.add_done_callback(self._done_callback)

    async def _stop(self):
        await self.queue.join()
        if self.task is not None:
            self.task.cancel()

    def _done_callback(self, task):
        self.task_is_done = True

    async def _consumer_task(self):
        loop = asyncio.get_running_loop()
        transport, protocol = await loop.connect_write_pipe(
            asyncio.Protocol, self.output)
        high = 16 * 1024 * 1024
        low  = 4 * 1024 * 1024
        transport.set_write_buffer_limits(high=high, low=low)
        writer = transport
        try:
            while True:
                data = await self.queue.get()
                writer.write(data)
                self.queue.task_done()
        except asyncio.CancelledError:
            pass
        finally:
            writer.close()

    async def write(self, data):
        if self.task_is_done:
            return

        await self.queue.put(data)
        # await asyncio.sleep(0)

    async def close(self):
        await self._stop()

    async def shutdown(self):
        await self.queue.shutdown_async(immediate=True)


class AsyncBufferedBackingStore(StdoutBackingStore):
    """
    Extended variant of StdoutBackingStore
    that uses an asyncio queue to buffer
    writes.
    Used for backups.
    """
    def __init__(self, exports):
        self.mode = "wb"
        self.exports = exports
        self.max_buffers = 10
        self.file = io.open(sys.stdout.fileno(), 'wb', closefd=False)
        self.pipe = AsyncBufferedPipe(self.max_buffers, self.file)
        self.lock = asyncio.Lock()

    async def init_async(self):
        await self.pipe.start()

    async def close_async(self):
        await self.pipe.close()

    async def write_async(self, index, offset, data):
        header_data = struct.pack(HEADER_FORMAT, BCHUNK_MAGIC,
            BCHUNK_OP_DATA, index, offset, len(data))
        async with self.lock:
            await self.pipe.write(header_data)
            await self.pipe.write(data)

    async def zero_async(self, index, offset, length):
        header_data = struct.pack(HEADER_FORMAT, BCHUNK_MAGIC,
            BCHUNK_OP_ZEROES, index, offset, length)
        async with self.lock:
            await self.pipe.write(header_data)


class AsyncBackingStore(StdoutBackingStore):
    """
    Extended variant of StdoutBackingStore
    that uses asyncio writes.
    Used for backups.
    """
    def __init__(self, exports):
        self.mode = "wb"
        self.exports = exports
        self.max_buffers = 10
        self.file = None
        self.writer = None
        #self.lock = asyncio.Lock()
        self.write_count = 0

    async def init_async(self):
        loop = asyncio.get_running_loop()
        transport, protocol = await loop.connect_write_pipe(
            lambda: asyncio.StreamReaderProtocol(asyncio.StreamReader()),
            sys.stdout.buffer)
        high = 128 * 1024 * 1024
        low  = 4 * 1024 * 1024
        transport.set_write_buffer_limits(high=high, low=low)
        self.writer = asyncio.StreamWriter(transport, protocol, None, loop)

    async def close_async(self):
        self.writer.close()
        await self.writer.wait_closed()

    async def write_async(self, index, offset, data):
        header_data = struct.pack(HEADER_FORMAT, BCHUNK_MAGIC,
            BCHUNK_OP_DATA, index, offset, len(data))
        self.writer.write(header_data)
        self.writer.write(data)
        await self._drain()

    async def _drain(self):
        self.write_count += 1
        if self.write_count == 4:
            await self.writer.drain()
            self.write_count = 0

    async def zero_async(self, index, offset, length):
        header_data = struct.pack(HEADER_FORMAT, BCHUNK_MAGIC,
            BCHUNK_OP_ZEROES, index, offset, length)
        self.writer.write(header_data)
        await self._drain()


class NbdServer:
    """
    Class implementing the server.
    """

    # NBD's magic
    NBD_HANDSHAKE = 0x49484156454F5054
    NBD_REPLY = 0x3e889045565a9

    NBD_REQUEST = 0x25609513
    NBD_RESPONSE = 0x67446698

    NBD_OPT_EXPORTNAME = 1
    NBD_OPT_ABORT = 2
    NBD_OPT_LIST = 3

    NBD_REP_ACK = 1
    NBD_REP_SERVER = 2
    NBD_REP_ERR_UNSUP = 2**31 + 1

    NBD_CMD_READ = 0
    NBD_CMD_WRITE = 1
    NBD_CMD_DISC = 2
    NBD_CMD_FLUSH = 3
    NBD_CMD_WRITE_ZEROES = 6

    # fixed newstyle handshake
    NBD_HANDSHAKE_FLAGS = (1 << 0)

    # has flags, supports flush
    NBD_EXPORT_FLAGS = (1 << 0) ^ (1 << 2)
    NBD_RO_FLAG = (1 << 1)
    NBD_FLAG_SEND_WRITE_ZEROES = (1 << 6)

    def __init__(self, addr, port, backing_store):
        global logger
        self.log = logger

        self.addr = addr
        self.port = port
        self.backing_store = backing_store
        self.stats_bytes = 0
        self.stats_cmds = 0
        self.sessions = 0
        self.task = None

    async def nbd_response(self, writer, handle, error=0, data=None):
        writer.write(struct.pack('>LLQ', self.NBD_RESPONSE, error, handle))
        if data:
            writer.write(data)
        await writer.drain()


    async def handler(self, reader, writer):
        """Handle the connection"""
        try:
            host, port = writer.get_extra_info("peername")
            store, container = None, None
            self.log.info("Incoming connection from %s:%s" % (host,port))

            # initial handshake
            writer.write(b"NBDMAGIC" + struct.pack(">QH", self.NBD_HANDSHAKE, self.NBD_HANDSHAKE_FLAGS))
            await writer.drain()

            data = await reader.readexactly(4)
            try:
                client_flag = struct.unpack(">L", data)[0]
            except struct.error:
                raise IOError("Handshake failed, disconnecting")

            # we support both fixed and unfixed new-style handshake
            if client_flag == 0:
                fixed = False
                self.log.warning("Client using new-style non-fixed handshake")
            elif client_flag & 1:
                fixed = True
            else:
                raise IOError("Handshake failed, disconnecting")

            # negotiation phase
            while True:
                header = await reader.readexactly(16)
                try:
                    (magic, opt, length) = struct.unpack(">QLL", header)
                except struct.error as ex:
                    raise IOError("Negotiation failed: Invalid request, disconnecting")

                if magic != self.NBD_HANDSHAKE:
                    raise IOError("Negotiation failed: bad magic number: %s" % magic)

                if length:
                    data = await reader.readexactly(length)
                    if(len(data) != length):
                        raise IOError("Negotiation failed: %s bytes expected" % length)
                else:
                    data = None

                self.log.debug("[%s:%s]: opt=%s, len=%s, data=%s" % (host, port, opt, length, data))

                if opt == self.NBD_OPT_EXPORTNAME:
                    if not data:
                        raise IOError("Negotiation failed: no export name was provided")

                    data = data.decode("utf-8")
                    self.log.info("[%s:%s]: export name %s" % (host, port, data))

                    # we have negotiated an export and it will be used
                    # until the client disconnects
                    store = self.backing_store

                    # find the index of the selected export name
                    index = 0
                    size = -1
                    for name, sizebytes  in store.exports:
                        if name == data:
                            size = sizebytes
                            break
                        index += 1

                    if size < 0:
                        if not fixed:
                            raise IOError("Negotiation failed: unknown export name")

                        writer.write(struct.pack(">QLLL", self.NBD_REPLY, opt, self.NBD_REP_ERR_UNSUP, 0))
                        await writer.drain()
                        continue

                    export_flags = self.NBD_EXPORT_FLAGS
                    if True:
                        export_flags |= self.NBD_FLAG_SEND_WRITE_ZEROES
                    if False:
                        export_flags ^= self.NBD_RO_FLAG
                        self.log.info("[%s:%s] %s is read only" % (host, port, "store.container"))

                    writer.write(struct.pack('>QH', size, export_flags))
                    writer.write(b"\x00"*124)
                    await writer.drain()

                    break

                elif opt == self.NBD_OPT_LIST:
                    for container, size in self.backing_store.exports:
                        writer.write(struct.pack(">QLLL", self.NBD_REPLY, opt, self.NBD_REP_SERVER, len(container) + 4))
                        container_encoded = container.encode("utf-8")
                        writer.write(struct.pack(">L", len(container_encoded)))
                        writer.write(container_encoded)
                        await writer.drain()

                    writer.write(struct.pack(">QLLL", self.NBD_REPLY, opt, self.NBD_REP_ACK, 0))
                    await writer.drain()

                elif opt == self.NBD_OPT_ABORT:
                    writer.write(struct.pack(">QLLL", self.NBD_REPLY, opt, self.NBD_REP_ACK, 0))
                    await writer.drain()

                    raise AbortedNegotiationError()
                else:
                    # we don't support any other option
                    if not fixed:
                        raise IOError("Unsupported option")

                    writer.write(struct.pack(">QLLL", self.NBD_REPLY, opt, self.NBD_REP_ERR_UNSUP, 0))
                    await writer.drain()

            # operation phase
            while True:
                header = await reader.readexactly(28)
                try:
                    (magic, cmdflags, cmd, handle, offset, length) = struct.unpack(">LhhQQL", header)
                except struct.error:
                    raise IOError("Invalid request, disconnecting")
                self.stats_cmds += 1

                if magic != self.NBD_REQUEST:
                    raise IOError("Bad magic number, disconnecting")

                self.log.debug("[%s:%s]: cmd=%s, handle=%s, offset=%s, len=%s" % (host, port, cmd, handle, offset, length))

                if cmd == self.NBD_CMD_DISC:
                    self.log.info("[%s:%s] disconnecting" % (host, port))
                    break

                elif cmd == self.NBD_CMD_WRITE:
                    data = await reader.readexactly(length)
                    if(len(data) != length):
                        raise IOError("%s bytes expected, disconnecting" % length)

                    try:
                        # skip all-zero chunks
                        #if data == b"\x00" * len(data):
                        #    await store.zero_async(index, offset, length)
                        await store.write_async(index, offset, data)
                        self.stats_bytes += len(data)
                    except IOError as ex:
                        self.log.error("[%s:%s] %s" % (host, port, ex))
                        await self.nbd_response(writer, handle, error=ex.errno)
                        continue

                    await self.nbd_response(writer, handle)

                elif cmd == self.NBD_CMD_WRITE_ZEROES:
                    await store.zero_async(index, offset, length)
                    await self.nbd_response(writer, handle)

                elif cmd == self.NBD_CMD_READ:
                    try:
                        #store.seek(offset)
                        #data = store.read(length)
                        pass
                    except IOError as ex:
                        self.log.error("[%s:%s] %s" % (host, port, ex))
                        await self.nbd_response(writer, handle, error=ex.errno)
                        continue

                    await self.nbd_response(writer, handle, data=data)

                elif cmd == self.NBD_CMD_FLUSH:
                    #store.flush()
                    await self.nbd_response(writer, handle)

                else:
                    self.log.warning("[%s:%s] Unknown cmd %s, disconnecting" % (host, port, cmd))
                    break

        except AbortedNegotiationError:
            self.log.info("[%s:%s] Client aborted negotiation" % (host, port))

        except (asyncio.IncompleteReadError, IOError) as ex:
            self.log.error("[%s:%s] %s" % (host, port, ex))

        finally:
            writer.close()
            self.log.info("[%s:%s] Stats: %d commands, %d bytes" % (host, port, self.stats_cmds, self.stats_bytes))
            self.stats_cmds = 0
            self.stats_bytes = 0

    async def discard(self):
        await self.backing_store.shutdown_async()

    async def serve_forever(self):
        """Create and run the asyncio loop"""
        try:
            server = await asyncio.start_server(self.handler, self.addr, self.port)
            await server.serve_forever()
        except asyncio.CancelledError:
            pass

        finally:
            await self.backing_store.close_async()

    def create_task(self):
        self.task = asyncio.create_task(self.serve_forever())
        return self.task


async def run_nbd_server(path, export, ipaddr, port):
        backing_store = BackingStore(path, export)
        server = NbdServer(ipaddr, int(port), backing_store)
        await server.serve_forever()


def nbd_write(target, offset, data, length):
    if data is None:
        target.zero(length, offset)
    else:
        target.pwrite(data, offset)


def restore_to_nbd(path, exports, count, ipaddr, port):
    nbdconns = []
    for export, size in exports:
        nbdconn = nbd.NBD()
        nbdconn.set_export_name(export)
        nbdconn.connect_tcp(ipaddr, port)
        nbdconns.append(nbdconn)

    if path is None or path == "-":
        backing_store = StdinBackingStore(exports)
    else:
        backing_store = BackingStore(path, exports)
    backing_store.start_dump()

    if count == 0:
        count = backing_store.size

    for a in range(0, count):
        backing_store.dump(nbdconns, nbd_write)

    backing_store.close() 

    nbdconn.shutdown()
