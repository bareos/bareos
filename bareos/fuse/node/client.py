"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.backups import Backups
from   bareos.fuse.node.directory import Directory

class Client(Directory):
    def __init__(self, bsock, name):
        super(Client, self).__init__(bsock, name)

    def do_update(self):
        self.add_subnode(Backups(self.bsock, "backups", client=self.get_name()))
