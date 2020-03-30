from multiprocessing import Process, Queue, Event
from time import sleep
from bareos_libcloud_api.queue_message import InfoMessage
from bareos_libcloud_api.queue_message import ReadyMessage
from bareos_libcloud_api.queue_message import ErrorMessage
from bareos_libcloud_api.queue_message import WorkerException
from bareos_libcloud_api.queue_message import MESSAGE_TYPE


class ProcessBase(Process):
    def __init__(
        self, worker_id, message_queue,
    ):
        super(ProcessBase, self).__init__()
        self.shutdown_event = Event()
        self.message_queue = message_queue
        self.worker_id = worker_id

    def shutdown(self):
        self.shutdown_event.set()

    def wait_for_shutdown(self):
        # print("Waiting for shutdown %d" % self.worker_id)
        self.shutdown_event.wait()
        # print("Waiting for shutdown %d ready" % self.worker_id)

    def info_message(self, level, message):
        self.message_queue.put(InfoMessage(self.worker_id, level, message))

    def ready_message(self):
        self.message_queue.put(ReadyMessage(self.worker_id, ""))

    def error_message(self, message):
        self.message_queue.put(ErrorMessage(self.worker_id, message))

    def worker_exception(self, message, exception):
        self.message_queue.put(WorkerException(self.worker_id, message, exception))
