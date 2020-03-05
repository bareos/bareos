#!/usr/bin/python
# -*- coding: utf-8 -*-
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
#
# Author: Alexandre Bruyelles <git@jack.fr.eu.org>
#

import BareosFdPluginBaseclass
import bareosfd
import bareos_fd_consts
import datetime
import dateutil.parser
import io
import itertools
import libcloud
import multiprocessing
import os
import syslog
import time
import traceback

from libcloud.storage.types import Provider
from libcloud.storage.providers import get_driver
from bareos_fd_consts import bRCs, bIOPS

syslog.openlog(__name__, facility=syslog.LOG_LOCAL7)

debug = False


def log(message):
    if debug is True:
        message = "[%s] %s" % (os.getpid(), message)
        syslog.syslog(message)


def error(message):
    message = "[%s] %s" % (os.getpid(), message)
    syslog.syslog(message)


# Print the traceback to syslog
def log_exc():
    for line in traceback.format_exc().split("\n"):
        error(line)


class IterStringIO(io.BufferedIOBase):
    def __init__(self, iterable):
        self.iter = itertools.chain.from_iterable(iterable)

    def read(self, n=None):
        return bytearray(itertools.islice(self.iter, None, n))


def str2bool(data):
    if data == "false" or data == "False":
        return False
    if data == "true" or data == "True":
        return True
    raise Exception("%s: not a boolean" % (data,))


def connect(options):
    driver_opt = dict(options)

    # Some drivers does not support unknown options
    # Here, we remove those used by libcloud and let the rest pass through
    for opt in (
        "buckets_exclude",
        "accurate",
        "nb_prefetcher",
        "prefetch_size",
        "queue_size",
        "provider",
        "buckets_include",
        "debug",
    ):
        if opt in options:
            del driver_opt[opt]

    provider = getattr(Provider, options["provider"])
    driver = get_driver(provider)(**driver_opt)
    return driver


def get_object(driver, bucket, key):
    try:
        return driver.get_object(bucket, key)
    except libcloud.common.types.InvalidCredsError:
        # Something is buggy here, this bug is triggered by tilde-ending objects
        # Our tokens are good, we used then before
        error(
            "BUGGY filename found, see the libcloud bug somewhere : %s/%s"
            % (bucket, key)
        )
        return None


class Prefetcher(object):
    def __init__(self, options, plugin_todo_queue, pref_todo_queue):
        self.options = options
        self.plugin_todo_queue = plugin_todo_queue
        self.pref_todo_queue = pref_todo_queue

    def __call__(self):
        try:
            self.driver = connect(self.options)
            self.__work()
        except:
            log("FATAL ERROR: prefetcher could not connect to driver")
            log_exc()

    def __work(self):
        while True:
            job = self.pref_todo_queue.get()
            if job is None:
                log("prefetcher[%s] : job completed, will quit now" % (os.getpid(),))
                return

            obj = get_object(self.driver, job["bucket"], job["name"])
            if obj is None:
                # Object cannot be fetched, an error is already logged
                continue

            stream = obj.as_stream()
            content = b"".join(list(stream))

            prefetched = len(content)
            if prefetched != job["size"]:
                error(
                    "FATAL ERROR: prefetched file %s: got %s bytes, not the real size (%s bytes)"
                    % (job["name"], prefetched, job["size"])
                )
                return

            data = io.BytesIO(content)

            job["data"] = data
            self.plugin_todo_queue.put(job)


class Writer(object):
    def __init__(self, plugin_todo_queue, pref_todo_queue, last_run, opts, pids):
        self.plugin_todo_queue = plugin_todo_queue
        self.pref_todo_queue = pref_todo_queue
        self.last_run = last_run
        self.options = opts
        self.pids = pids

        self.driver = connect(self.options)
        self.delta = datetime.timedelta(seconds=time.timezone)

    def __call__(self):
        try:
            self.__iterate_over_buckets()
        except:
            log_exc()
        self.__end_job()

    def __iterate_over_buckets(self):
        for bucket in self.driver.iterate_containers():
            if self.options["buckets_include"] is not None:
                if bucket.name not in self.options["buckets_include"]:
                    continue

            if self.options["buckets_exclude"] is not None:
                if bucket.name in self.options["buckets_exclude"]:
                    continue

            log("Backing up bucket \"%s\"" % (bucket.name,))

            self.__generate_jobs_for_bucket_objects(self.driver.iterate_container_objects(bucket))

    def __get_mtime(self, obj):
        mtime = dateutil.parser.parse(obj.extra["last_modified"])
        mtime = mtime - self.delta
        mtime = mtime.replace(tzinfo=None)

        ts = time.mktime(mtime.timetuple())
        return mtime, ts

    def __generate_jobs_for_bucket_objects(self, bucket_iterator):
        for obj in bucket_iterator:
            mtime, mtime_ts = self.__get_mtime(obj)

            job = {
                "name": obj.name,
                "bucket": obj.container.name,
                "data": None,
                "size": obj.size,
                "mtime": mtime_ts,
            }

            object_name = "%s/%s" % (obj.container.name, obj.name)

            if self.last_run > mtime:
                log(
                    "File %s not changed, skipped (%s > %s)"
                    % (object_name, self.last_run, mtime)
                )

                # This object was present on our last backup
                # Here, we push it directly to bareos, it will not be backed again
                # but remembered as "still here" (for accurate mode)
                # If accurate mode is off, we can simply skip that object
                if self.options["accurate"] is True:
                    self.plugin_todo_queue.put(job)

                continue

            log(
                "File %s was changed or is new, backing up (%s < %s)"
                % (object_name, self.last_run, mtime)
            )

            # Do not prefetch large objects
            if obj.size >= self.options["prefetch_size"]:
                self.plugin_todo_queue.put(job)
            else:
                self.pref_todo_queue.put(job)

    def __end_job(self):
        log("__end_job: waiting for prefetchers queue to become empty")
        while True:
            size = self.pref_todo_queue.qsize()
            if size == 0:
                break
            log("__end_job: %s items left in prefetchers queue, waiting" % (size,))
            time.sleep(2)
        log("__end_job: prefetchers queue is empty")

        log("__end_job: I will shutdown all prefetchers now")
        for i in range(0, self.options["nb_prefetcher"]):
            self.pref_todo_queue.put(None)

        # This is the last item ever put on that queue
        # The plugin on the other end will know the backup is completed
        self.plugin_todo_queue.put(None)


class BareosFdPluginLibcloud(BareosFdPluginBaseclass.BareosFdPluginBaseclass):
    def __init__(self, context, plugindef):
        log("BareosFdPluginLibcloud called with plugindef: %s" % (plugindef,))

        super(BareosFdPluginLibcloud, self).__init__(context, plugindef)
        super(BareosFdPluginLibcloud, self).parse_plugin_definition(context, plugindef)
        self.__parse_options(context)

        self.last_run = datetime.datetime.fromtimestamp(self.since)
        self.last_run = self.last_run.replace(tzinfo=None)

        # We force an action to the backend, to test our params
        # If we can get anything, then it is ok
        # Else, an exception will be raised, and the job will fail
        driver = connect(self.options)
        for _ in driver.iterate_containers():
            break

        # The job in process
        # Setto None when the whole backup is completed
        # Restore's path will not touch this
        self.job = {}

        log("Last backup: %s (ts: %s)" % (self.last_run, self.since))

    def __parse_options_bucket(self, name):
        if name not in self.options:
            self.options[name] = None
        else:
            buckets = list()
            for bucket in self.options[name].split(","):
                buckets.append(bucket)
            self.options[name] = buckets

    def __parse_opt_int(self, name):
        if name not in self.options:
            return

        value = self.options[name]
        self.options[name] = int(value)

    def __parse_options(self, context):
        # Set our default values
        if "nb_prefetcher" not in self.options:
            self.options["nb_prefetcher"] = 24
        if "queue_size" not in self.options:
            self.options["queue_size"] = 1000
        if "prefetch_size" not in self.options:
            self.options["prefetch_size"] = 10 * 1024 * 1024
        self.__parse_options_bucket("buckets_include")
        self.__parse_options_bucket("buckets_exclude")

        # Do a couple of sanitization
        if "secure" in self.options:
            old = self.options["secure"]
            self.options["secure"] = str2bool(old)

        self.__parse_opt_int("port")
        self.__parse_opt_int("nb_prefetcher")
        self.__parse_opt_int("prefetch_size")
        self.__parse_opt_int("queue_size")
        self.__parse_opt_int("prefetch_size")

        if "debug" in self.options:
            old = self.options["debug"]
            self.options["debug"] = str2bool(old)

            # Setup debugging
            if self.options["debug"] is True:
                global debug
                debug = True

        accurate = bareos_fd_consts.bVariable["bVarAccurate"]
        accurate = bareosfd.GetValue(context, accurate)
        if accurate is None or accurate == 0:
            self.options["accurate"] = False
        else:
            self.options["accurate"] = True

    def parse_plugin_definition(self, context, plugindef):
        return bRCs["bRC_OK"]

    def start_backup_job(self, context):
        self.manager = multiprocessing.Manager()
        self.plugin_todo_queue = self.manager.Queue(maxsize=self.options["queue_size"])
        self.pref_todo_queue = self.manager.Queue(maxsize=self.options["nb_prefetcher"])

        self.pf_pids = list()
        self.prefetchers = list()
        for i in range(0, self.options["nb_prefetcher"]):
            target = Prefetcher(
                self.options, self.plugin_todo_queue, self.pref_todo_queue
            )
            proc = multiprocessing.Process(target=target)
            proc.start()
            self.pf_pids.append(proc.pid)
            self.prefetchers.append(proc)
        log("%s prefetcher started" % (len(self.pf_pids),))

        writer = Writer(
            self.plugin_todo_queue,
            self.pref_todo_queue,
            self.last_run,
            self.options,
            self.pf_pids,
        )
        self.writer = multiprocessing.Process(target=writer)
        self.writer.start()
        self.driver = connect(self.options)

    def check_file(self, context, fname):
        # All existing files are passed to bareos
        # If bareos have not seen one, it does not exists anymore
        return bRCs["bRC_Error"]

    def start_backup_file(self, context, savepkt):
        try:
            while True:
                try:
                    self.job = self.plugin_todo_queue.get_nowait()
                    break
                except:
                    size = self.plugin_todo_queue.qsize()
                    log("start_backup_file: queue size: %s" % (size,))
                    time.sleep(0.1)
        except TypeError:
            self.job = None

        if self.job is None:
            log("End of queue found, backup is completed")
            for i in self.prefetchers:
                log("join() for a prefetcher (pid %s)" % (i.pid,))
                i.join()
            log("Ok, all prefetchers are shut down")

            try:
                self.manager.shutdown()
            except OSError:
                # should not happen!
                pass
            log("self.manager.shutdown()")

            log("Join() for the writer (pid %s)" % (self.writer.pid,))
            self.writer.join()
            log("writer is shut down")

            # savepkt is always checked, so we fill it with a dummy value
            savepkt.fname = "empty"
            return bRCs["bRC_Skip"]

        filename = "%s/%s" % (self.job["bucket"], self.job["name"])
        log("Backing up %s" % (filename,))

        statp = bareosfd.StatPacket()
        statp.size = self.job["size"]
        statp.mtime = self.job["mtime"]
        statp.atime = 0
        statp.ctime = 0

        savepkt.statp = statp
        savepkt.fname = filename
        savepkt.type = bareos_fd_consts.bFileType["FT_REG"]

        return bRCs["bRC_OK"]

    def plugin_io(self, context, IOP):
        if self.job is None:
            return bRCs["bRC_Error"]
        if IOP.func == bIOPS["IO_OPEN"]:
            # Only used by the 'restore' path
            if IOP.flags & (os.O_CREAT | os.O_WRONLY):
                self.FILE = open(IOP.fname, "wb")
                return bRCs["bRC_OK"]

            # 'Backup' path
            if self.job["data"] is None:
                obj = get_object(self.driver, self.job["bucket"], self.job["name"])
                if obj is None:
                    self.FILE = None
                    return bRCs["bRC_Error"]
                self.FILE = IterStringIO(obj.as_stream())
            else:
                self.FILE = self.job["data"]

        elif IOP.func == bIOPS["IO_READ"]:
            IOP.buf = bytearray(IOP.count)
            IOP.io_errno = 0
            if self.FILE is None:
                return bRCs["bRC_Error"]
            try:
                buf = self.FILE.read(IOP.count)
                IOP.buf[:] = buf
                IOP.status = len(buf)
                return bRCs["bRC_OK"]
            except IOError as e:
                log("Cannot read from %s : %s" % (IOP.fname, e))
                IOP.status = 0
                IOP.io_errno = e.errno
                return bRCs["bRC_Error"]

        elif IOP.func == bIOPS["IO_WRITE"]:
            try:
                self.FILE.write(IOP.buf)
                IOP.status = IOP.count
                IOP.io_errno = 0
            except IOError as msg:
                IOP.io_errno = -1
                error("Failed to write data: %s" % (msg,))
            return bRCs["bRC_OK"]
        elif IOP.func == bIOPS["IO_CLOSE"]:
            if self.FILE:
                self.FILE.close()
            return bRCs["bRC_OK"]

        return bRCs["bRC_OK"]

    def end_backup_file(self, context):
        if self.job is not None:
            return bRCs["bRC_More"]
        else:
            return bRCs["bRC_OK"]
