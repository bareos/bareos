"""
bareosfs root node (top level directory)
"""

from   bareos.fuse.nodefactory import NodeFactory
from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node import *

class Root(Directory):
    """
    Define filesystem structure of root (/) directory.
    """
    def __init__(self, bsock, restoreclient, restorepath):
        self.bsock = bsock
        self.restoreclient = restoreclient
        self.restorepath = restorepath
        super(Root, self).__init__(self, None)
        self.factory = NodeFactory(self)
        self.add_subnode(Jobs, "jobs")
        self.add_subnode(Volumes, "volumes")
        self.add_subnode(Clients, "clients")
