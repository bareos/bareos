import time
import dateutil
import dateutil.parser
import datetime


class TestObject:
    def __init__(self):
        self.extra = {}
        self.extra["last_modified"] = "2020-01-02 13:15:16"


class ModificationTime(object):
    def __init__(self):
        self.timezone_delta = datetime.timedelta(seconds=time.timezone)

    def get_mtime(self, obj):
        mtime = dateutil.parser.parse(obj.extra["last_modified"])
        mtime = mtime - self.timezone_delta
        mtime = mtime.replace(tzinfo=None)

        ts = time.mktime(mtime.timetuple())
        return mtime, ts

    def get_last_run(self):
        last_run = datetime.datetime(1970, 1, 1)
        return last_run.replace(tzinfo=None)


if __name__ == "__main__":
    m = ModificationTime()
    print("mtime: ", m.get_mtime(TestObject()))
    print("last run: ", m.get_last_run())
