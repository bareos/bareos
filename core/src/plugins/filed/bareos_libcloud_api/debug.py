import os
import bareosfd
from bareos_fd_consts import bJobMessageType

debuglevel = 100
plugin_context = None


def jobmessage(message_type, message):
    global plugin_context
    if plugin_context != None:
        message = "BareosFdPluginLibcloud [%s]: %s\n" % (os.getpid(), message)
        bareosfd.JobMessage(plugin_context, bJobMessageType[message_type], message)


def debugmessage(level, message):
    global plugin_context
    if plugin_context != None:
        message = "BareosFdPluginLibcloud [%s]: %s\n" % (os.getpid(), message)
        bareosfd.DebugMessage(plugin_context, level, message)


def set_plugin_context(context):
    global plugin_context
    plugin_context = context
