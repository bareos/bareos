"""
bareosfs root node (top level directory)
"""

import bareos.fuse.exceptions
from   bareos.fuse.nodefactory import NodeFactory
from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node import *

class Root(Directory):
    """
    Define filesystem structure of root (/) directory.
    """
    def __init__(self, bsock, restoreclient, restorejob, restorepath):
        self.bsock = bsock
        self.restoreclient = restoreclient
        if restoreclient:
            data = self.bsock.call(".clients")
            if not restoreclient in [item['name'] for item in data['clients']]:
                raise bareos.fuse.RestoreClientUnknown(restoreclient)
        self.restorejob = restorejob
        if restorejob:
            data = self.bsock.call(".jobs")
            if not restorejob in [item['name'] for item in data['jobs']]:
                raise bareos.fuse.RestoreJobUnknown(restorejob)
        self.restorepath = restorepath
        super(Root, self).__init__(self, None)
        self.factory = NodeFactory(self)
        self.add_subnode(Jobs, "jobs")
        self.add_subnode(VolumeList, "volumes")
        self.add_subnode(Pools, "pools")
        self.add_subnode(Clients, "clients")
        self.add_subnode(Status, ".bareosfs-status.txt")
