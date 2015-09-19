"""
NodeFactory
"""

from   bareos.fuse.node import *

class NodeFactory(object):
    """
    Creates instances of nodes, when required. directory.
    """
    def __init__(self, root):
        self.logger = logging.getLogger()
        self.root = root
        self.instances = {}

    def get_instance(self, classtype, *args, **kwargs):
        classname = classtype.__name__
        if not self.instances.has_key(classname):
            self.instances[classname] = {}
        id = classtype.get_id(*args, **kwargs)
        if not id:
            self.logger.debug(classname + ": no id defined. Creating without storing")
            instance = classtype(self.root, *args, **kwargs)
            #self.instances[classname][id] = instance
        else:
            if self.instances[classname].has_key(id):
                self.logger.debug(classname + ": reusing " + id)
                instance = self.instances[classname][id]
            else:
                self.logger.debug(classname + ": creating " + id)
                instance = classtype(self.root, *args, **kwargs)
                self.instances[classname][id] = instance
        return instance
