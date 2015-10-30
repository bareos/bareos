"""
Bareos specific Fuse node.
"""

from    bareos.fuse.node.file import File
from    datetime import datetime, timedelta


class Status(File):
    def __init__(self, root, name):
        super(Status, self).__init__(root, name)
        self.cache_timeout = timedelta(seconds=0)
        self.cache_stat_timeout = timedelta(seconds=60)
        self.objects = 0


    @classmethod
    def get_id(cls, name):
        return "unique"

    def do_update(self):
        self.objects = 0
        self.content  = "#\n"
        self.content += "# bareosfs cache\n"
        self.content += "#\n"
        self.content += self.get_status(None, self.root, -1)
        self.content += "#\n"
        self.content += "# number objects: %i\n" % (self.objects)
        self.content += "#\n"


    def get_status(self, name, node, indent):
        result = ""
        if name:
            result = " " * (indent*2) + "%s\n" % (name)
            self.objects += 1
        for i in sorted(node.subnodes.keys()):
            result += self.get_status(i, node.subnodes[i], indent+1)
        return result
