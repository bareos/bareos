class MESSAGE_TYPE(object):
    InfoMessage = 1
    ErrorMessage = 2
    WorkerException = 3
    ReadyMessage = 4

    def __setattr__(self, *_):
        raise Exception("class MESSAGE_TYPE is read only")


class QueueMessageBase(object):
    def __init__(self, worker_id, message):
        self.worker_id = worker_id
        self.message_string = message
        self.type = None


class ErrorMessage(QueueMessageBase):
    def __init__(self, worker_id, message):
        QueueMessageBase.__init__(self, worker_id, message)
        self.type = MESSAGE_TYPE.ErrorMessage


class InfoMessage(QueueMessageBase):
    def __init__(self, worker_id, level, message):
        QueueMessageBase.__init__(self, worker_id, message)
        self.type = MESSAGE_TYPE.InfoMessage
        self.level = level


class WorkerException(QueueMessageBase):
    def __init__(self, worker_id, message, exception):
        QueueMessageBase.__init__(self, worker_id, message)
        self.type = MESSAGE_TYPE.WorkerException
        self.exception = exception


class ReadyMessage(QueueMessageBase):
    def __init__(self, worker_id, message):
        QueueMessageBase.__init__(self, worker_id, "")
        self.type = MESSAGE_TYPE.ReadyMessage
