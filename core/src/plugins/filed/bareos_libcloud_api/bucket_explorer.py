from bareos_libcloud_api.process_base import ProcessBase
from bareos_libcloud_api.get_libcloud_driver import get_driver
from bareos_libcloud_api.mtime import ModificationTime


def parse_options_bucket(name, options):
    if name not in options:
        return None
    else:
        buckets = list()
        for bucket in options[name].split(","):
            buckets.append(bucket)
        return buckets


class BucketExplorer(ProcessBase):
    def __init__(
        self,
        options,
        last_run,
        message_queue,
        discovered_objects_queue,
        number_of_workers,
    ):
        super(BucketExplorer, self).__init__(0, message_queue)
        self.options = options
        self.last_run = last_run
        self.discovered_objects_queue = discovered_objects_queue
        self.buckets_include = parse_options_bucket("buckets_include", options)
        self.buckets_exclude = parse_options_bucket("buckets_exclude", options)
        self.number_of_workers = number_of_workers
        self.object_count = 0

    def run(self):
        self.driver = get_driver(self.options)

        if self.driver == None:
            self.error_message("Could not load driver")
            return

        if not self.shutdown_event.is_set():
            self.__iterate_over_buckets()

        for _ in range(self.number_of_workers):
            self.discovered_objects_queue.put(None)

        self.wait_for_shutdown()

    def __iterate_over_buckets(self):
        try:
            for bucket in self.driver.iterate_containers():
                if self.shutdown_event.is_set():
                    break

                if self.buckets_include is not None:
                    if bucket.name not in self.buckets_include:
                        continue

                if self.buckets_exclude is not None:
                    if bucket.name in self.buckets_exclude:
                        continue

                self.info_message(100, 'Backing up bucket "%s"' % (bucket.name,))

                self.__generate_jobs_for_bucket_objects(
                    self.driver.iterate_container_objects(bucket)
                )
        except Exception as exception:
            self.worker_exception("Error while iterating containers", exception)

    def __generate_jobs_for_bucket_objects(self, object_iterator):
        try:
            for obj in object_iterator:
                if self.shutdown_event.is_set():
                    break

                mtime, mtime_ts = ModificationTime().get_mtime(obj)

                job = {
                    "name": obj.name,
                    "bucket": obj.container.name,
                    "data": None,
                    "index": None,
                    "size": obj.size,
                    "mtime": mtime_ts,
                }

                object_name = "%s/%s" % (obj.container.name, obj.name)

                if self.last_run > mtime:
                    self.info_message(
                        100,
                        "File %s not changed, skipped (%s > %s)"
                        % (object_name, self.last_run, mtime),
                    )

                    # This object was present on our last backup
                    # Here, we push it directly to bareos, it will not be backed again
                    # but remembered as "still here" (for accurate mode)
                    # If accurate mode is off, we can simply skip that object
                    if self.options["accurate"] is True:
                        self.queue_try_put(self.discovered_objects_queue, job)

                    continue

                self.info_message(
                    100,
                    "File %s was changed or is new, backing up (%s < %s)"
                    % (object_name, self.last_run, mtime),
                )

                self.queue_try_put(self.discovered_objects_queue, job)

        except Exception as exception:
            self.worker_exception("Error while iterating objects", exception)
