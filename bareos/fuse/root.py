"""
"""

from   bareos.fuse.node.directory import Directory
from   bareos.fuse.node import *

class Root(Directory):
    """
    Define filesystem structure of root (/) directory.
    """
    def __init__(self, bsock):
        super(Root, self).__init__(bsock, None)
        self.add_subnode(Jobs(bsock, "jobs"))
        self.add_subnode(Volumes(bsock, "volumes"))
        self.add_subnode(Clients(bsock, "clients"))
