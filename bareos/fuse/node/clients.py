"""
Bareos specific Fuse node.
"""

from   bareos.fuse.node.client import Client
from   bareos.fuse.node.directory import Directory

class Clients(Directory):
    def __init__(self, root, name):
        super(Clients, self).__init__(root, name)

    @classmethod
    def get_id(cls, name):
        return "unique"

    def do_update(self):
        data = self.bsock.call(".clients")
        clients = data['clients']
        for i in clients:
            name = i['name']
            self.add_subnode(Client, name)
