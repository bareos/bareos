from bareos_libcloud_api.process_base import ProcessBase
from bareos_libcloud_api.get_libcloud_driver import get_driver
import io
from libcloud.common.types import LibcloudError
import uuid
from time import sleep


class Worker(ProcessBase):
    def __init__(
        self,
        options,
        worker_id,
        tmp_dir_path,
        message_queue,
        discovered_objects_queue,
        downloaded_objects_queue,
    ):
        super(Worker, self).__init__(
            worker_id, message_queue,
        )
        self.options = options
        self.tmp_dir_path = tmp_dir_path
        self.input_queue = discovered_objects_queue
        self.output_queue = downloaded_objects_queue

    def run_process(self):
        self.driver = get_driver(self.options)
        if self.driver == None:
            self.error_message("Could not load driver")
            return

        finish = False
        while not finish:
            finish = self.__iterate_input_queue()

    def __iterate_input_queue(self):
        while not self.input_queue.empty():
            if self.shutdown_event.is_set():
                return True
            job = self.input_queue.get()
            if job == None: #poison pill
                return True
            if not self.__run_job(job):
                return True
        # try again
        return False

    def __run_job(self, job):
        try:
            obj = self.driver.get_object(job["bucket"], job["name"])
            if obj is None:
                # Object cannot be fetched, an error is already logged
                return True
        except Exception as exception:
            self.worker_exception("Could not download file", exception)
            return False

        stream = obj.as_stream()
        content = b"".join(list(stream))

        size_of_fetched_object = len(content)
        if size_of_fetched_object != job["size"]:
            self.error_message(
                "prefetched file %s: got %s bytes, not the real size (%s bytes)"
                % (job["name"], size_of_fetched_object, job["size"]),
            )
            return False

        buf = io.BytesIO(content)
        if size_of_fetched_object < 1024 * 10:
            job["data"] = buf
        else:
            try:
                tmpfilename = self.tmp_dir_path + "/" + str(uuid.uuid4())
                obj.download(tmpfilename)
                job["data"] = None
                job["index"] = tmpfilename
            except OSError as e:
                self.worker_exception("Could not open temporary file", e)
            except LibcloudError as e:
                self.worker_exception("Error downloading object", e)
            except Exception as e:
                self.worker_exception("Error using temporary file", e)

        self.queue_try_put(self.output_queue, job)

        #success
        return True
