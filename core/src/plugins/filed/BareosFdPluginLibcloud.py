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
import ConfigParser as configparser
import datetime
import dateutil.parser
import io
import itertools
import libcloud
import multiprocessing
import os
import syslog
import time

from bareos_fd_consts import bRCs, bIOPS, bJobMessageType
from libcloud.storage.types import Provider
from libcloud.storage.providers import get_driver
from sys import version_info

syslog.openlog(__name__, facility=syslog.LOG_LOCAL7)

plugin_context = None

def jobmessage(message_type, message):
    global plugin_context
    if plugin_context != None:
        message = "BareosFdPluginLibcloud [%s]: %s\n" % (os.getpid(), message)
        bareosfd.JobMessage(
            plugin_context,
            bJobMessageType[message_type],
            message
        )

def debugmessage(level, message):
    global plugin_context
    if plugin_context != None:
        message = "BareosFdPluginLibcloud [%s]: %s\n" % (os.getpid(), message)
        bareosfd.DebugMessage(
            plugin_context,
            level,
            message
        )

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
        "nb_worker",
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
        # Our tokens are good, we used them before
        debugmessage(100,
            "BUGGY filename found, see the libcloud bug somewhere : %s/%s"
            % (bucket, key)
        )
        return None


class Worker(object):
    def __init__(self, options, plugin_todo_queue, worker_todo_queue):
        self.options = options
        self.plugin_todo_queue = plugin_todo_queue
        self.worker_todo_queue = worker_todo_queue

    def __call__(self):
        try:
            self.driver = connect(self.options)
            self.__load_object()
        except:
            debugmessage(100, "Worker could not connect to driver")

    def __load_object(self):
        while True:
            job = self.worker_todo_queue.get()
            if job is None:
                debugmessage(100, "worker[%s] : job completed, will quit now" % (os.getpid(),))
                return

            obj = get_object(self.driver, job["bucket"], job["name"])
            if obj is None:
                # Object cannot be fetched, an error is already logged
                continue

            stream = obj.as_stream()
            content = b"".join(list(stream))

            size_of_fetched_object = len(content)
            if size_of_fetched_object != job["size"]:
                debugmessage(100,
                    "prefetched file %s: got %s bytes, not the real size (%s bytes)"
                    % (job["name"], size_of_fetched_object, job["size"])
                )
                return

            data = io.BytesIO(content)

            job["data"] = data
            self.plugin_todo_queue.put(job)


class JobCreator(object):
    def __init__(self, plugin_todo_queue, worker_todo_queue, last_run, opts, pids):
        self.plugin_todo_queue = plugin_todo_queue
        self.worker_todo_queue = worker_todo_queue
        self.last_run = last_run
        self.options = opts
        self.pids = pids

        self.driver = connect(self.options)
        self.delta = datetime.timedelta(seconds=time.timezone)

    def __call__(self):
        try:
            self.__iterate_over_buckets()
        except:
            debugmessage(100, "Error while iterating over buckets")
        self.__end_job()

    def __iterate_over_buckets(self):
        for bucket in self.driver.iterate_containers():
            if self.options["buckets_include"] is not None:
                if bucket.name not in self.options["buckets_include"]:
                    continue

            if self.options["buckets_exclude"] is not None:
                if bucket.name in self.options["buckets_exclude"]:
                    continue

            debugmessage(100, "Backing up bucket \"%s\"" % (bucket.name,))

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
                debugmessage(100,
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

            debugmessage(100,
                "File %s was changed or is new, backing up (%s < %s)"
                % (object_name, self.last_run, mtime)
            )

            # Do not prefetch large objects
            if obj.size >= self.options["prefetch_size"]:
                self.plugin_todo_queue.put(job)
            else:
                self.worker_todo_queue.put(job)

    def __end_job(self):
        debugmessage(100, "__end_job: waiting for workers queue to become empty")
        while True:
            size = self.worker_todo_queue.qsize()
            if size == 0:
                break
            log("__end_job: %s items left in workers queue, waiting" % (size,))
            time.sleep(2)
        debugmessage(100, "__end_job: workers queue is empty")

        debugmessage(100, "__end_job: I will shutdown all workers now")
        for _ in range(0, self.options["nb_worker"]):
            self.worker_todo_queue.put(None)

        while not self.worker_todo_queue.empty():
            pass

        # notify the plugin that the workers are ready
        self.plugin_todo_queue.put(None)


class BareosFdPluginLibcloud(BareosFdPluginBaseclass.BareosFdPluginBaseclass):
    def __init__(self, context, plugindef):
        global plugin_context
        plugin_context = context
        debugmessage(100, "BareosFdPluginLibcloud called with plugindef: %s" % (plugindef,))

        super(BareosFdPluginLibcloud, self).__init__(context, plugindef)
        super(BareosFdPluginLibcloud, self).parse_plugin_definition(context, plugindef)
        self.__parse_options(context)

        self.last_run = datetime.datetime.fromtimestamp(self.since)
        self.last_run = self.last_run.replace(tzinfo=None)

        # The backup job in process
        # Set to None when the whole backup is completed
        # Restore's path will not touch this
        self.current_backup_job = {}
        self.number_of_objects_to_backup = 0

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
        self.__parse_options_bucket("buckets_include")
        self.__parse_options_bucket("buckets_exclude")

        if "debug" in self.options:
            old = self.options["debug"]
            self.options["debug"] = str2bool(old)

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
        debugmessage(100, "Parse Plugin Definition")
        config_filename = self.options.get("config_file")
        if config_filename:
            if self.__parse_config_file(context, config_filename):
                return bRCs["bRC_OK"]
        jobmessage("M_FATAL", "Could not load configfile %s" % (config_filename))
        return bRCs["bRC_Error"]

    def __parse_config_file(self, context, config_filename):
        """
        Parse the config file given in the config_file plugin option
        """
        debugmessage(100, "parse_config_file(): parse %s\n" % (config_filename))

        self.config = configparser.ConfigParser()

        try:
            if version_info[:3] < (3,2):
                self.config.readfp(open(config_filename))
            else:
                self.config.read_file(open(config_filename))
        except (IOError, OSError) as err:
            debugmessage(100,
                "BareosFdPluginLibcloud: Error reading config file %s: %s\n"
                % (self.options["config_file"], err.strerror),
            )
            return False

        return self.__check_config(context, config_filename)

    def __check_config(self, context, config_filename):
        """
        Check the configuration and set or override options if necessary,
        considering mandatory: username and password in the [credentials] section
        """
        mandatory_sections = ["credentials", "host", "misc"]
        mandatory_options = {}
        mandatory_options["credentials"] = ["username", "password"]
        mandatory_options["host"] = ["hostname", "port", "provider", "tls"]
        mandatory_options["misc"] = ["nb_worker", "queue_size", "prefetch_size"]

        #this maps config file options to libcloud options
        option_map = {"hostname":"host", "port":"port", "provider":"provider",
        "username":"key", "password":"secret"}

        for section in mandatory_sections:
            if not self.config.has_section(section):
                debugmessage(100, "BareosFdPluginLibcloud: Section [%s] missing in config file %s\n"
                % (section, config_filename))
                return False

            for option in mandatory_options[section]:
                if not self.config.has_option(section, option):
                    debugmessage(100, "BareosFdPluginLibcloud: Option [%s] missing in Section %s\n"
                    % (option, section))
                    return False

                value = self.config.get(section, option)

                try:
                    if option == "tls":
                        self.options["secure"] = str2bool(value)
                    elif option == "nb_worker":
                        self.options["nb_worker"] = int(value)
                    elif option == "queue_size":
                        self.options["queue_size"] = int(value)
                    elif option == "prefetch_size":
                        self.options["prefetch_size"] = eval(value)
                    else:
                        self.options[option_map[option]] = value
                except:
                    debugmessage(100, "Could not evaluate: %s in config file %s" % (value, config_filename))
                    return False

        return True

    def start_backup_job(self, context):
        try:
            jobmessage("M_INFO", "Try to connect to %s:%s" % (self.options["host"], self.options["port"]))
            driver = connect(self.options)
            for _ in driver.iterate_containers():
                break
        except:
            jobmessage("M_FATAL", "Could not connect to libcloud driver")
            return bRCs["bRC_Error"]

        jobmessage("M_INFO", "Last backup: %s (ts: %s)" % (self.last_run, self.since))

        self.manager = multiprocessing.Manager()
        self.plugin_todo_queue = self.manager.Queue(maxsize=self.options["queue_size"])
        self.worker_todo_queue = self.manager.Queue(maxsize=self.options["nb_worker"])

        self.worker_pids = list()
        self.workers = list()
        for _ in range(0, self.options["nb_worker"]):
            target = Worker(
                self.options, self.plugin_todo_queue, self.worker_todo_queue
            )
            proc = multiprocessing.Process(target=target)
            proc.start()
            self.worker_pids.append(proc.pid)
            self.workers.append(proc)
        debugmessage(100, "%s worker started" % (len(self.worker_pids),))

        job_generator = JobCreator(
            self.plugin_todo_queue,
            self.worker_todo_queue,
            self.last_run,
            self.options,
            self.worker_pids,
        )
        self.job_generator = multiprocessing.Process(target=job_generator)
        self.job_generator.start()
        self.driver = connect(self.options)
        debugmessage(100, "%s job creator started" % (len(self.worker_pids),))
        return bRCs["bRC_OK"]

    def check_file(self, context, fname):
        # All existing files/objects are passed to bareos
        # If bareos have not seen one, it does not exists anymore
        return bRCs["bRC_Error"]

    def __shutdown_and_join_worker(self):
        if self.number_of_objects_to_backup:
            jobmessage("M_INFO", "Backup completed with %d objects" % self.number_of_objects_to_backup)
        else:
            jobmessage("M_INFO", "No objects to backup")
        for i in self.workers:
            debugmessage(100, "join() for a worker (pid %s)" % (i.pid,))
            i.join()
        debugmessage(100, "Ok, all workers are shut down")

        try:
            self.manager.shutdown()
        except OSError:
            # should not happen!
            pass
        debugmessage(100, "self.manager.shutdown()")

        debugmessage(100, "Join() for the job_generator (pid %s)" % (self.job_generator.pid,))
        self.job_generator.join()
        debugmessage(100, "job_generator is shut down")

    def start_backup_file(self, context, savepkt):
        debugmessage(100, "start_backup_file() called, waiting for worker")
        try:
            while True:
                try:
                    self.current_backup_job = self.plugin_todo_queue.get_nowait()
                    break
                except:
                    time.sleep(0.1)
        except TypeError:
            self.current_backup_job = None

        if self.current_backup_job is None:
            self.__shutdown_and_join_worker()
            savepkt.fname = "empty" # dummy value, savepkt is always checked
            return bRCs["bRC_Skip"]

        filename = "PYLIBCLOUD:/%s/%s" % (self.current_backup_job["bucket"], self.current_backup_job["name"])
        jobmessage("M_INFO", "Backing up %s" % (filename,))

        statp = bareosfd.StatPacket()
        statp.size = self.current_backup_job["size"]
        statp.mtime = self.current_backup_job["mtime"]
        statp.atime = 0
        statp.ctime = 0

        savepkt.statp = statp
        savepkt.fname = filename
        savepkt.type = bareos_fd_consts.bFileType["FT_REG"]

        self.number_of_objects_to_backup += 1

        return bRCs["bRC_OK"]

    def plugin_io(self, context, IOP):
        if self.current_backup_job is None:
            return bRCs["bRC_Error"]
        if IOP.func == bIOPS["IO_OPEN"]:
            # Only used by the 'restore' path
            if IOP.flags & (os.O_CREAT | os.O_WRONLY):
                self.FILE = open(IOP.fname, "wb")
                return bRCs["bRC_OK"]

            # 'Backup' path
            if self.current_backup_job["data"] is None:
                obj = get_object(self.driver, self.current_backup_job["bucket"], self.current_backup_job["name"])
                if obj is None:
                    self.FILE = None
                    return bRCs["bRC_Error"]
                self.FILE = IterStringIO(obj.as_stream())
            else:
                self.FILE = self.current_backup_job["data"]

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
                jobmessage("M_ERROR", "Cannot read from %s : %s" % (IOP.fname, e))
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
                jobmessage("M_ERROR", "Failed to write data: %s" % (msg,))
            return bRCs["bRC_OK"]
        elif IOP.func == bIOPS["IO_CLOSE"]:
            if self.FILE:
                self.FILE.close()
            return bRCs["bRC_OK"]

        return bRCs["bRC_OK"]

    def end_backup_file(self, context):
        if self.current_backup_job is not None:
            return bRCs["bRC_More"]
        else:
            return bRCs["bRC_OK"]
