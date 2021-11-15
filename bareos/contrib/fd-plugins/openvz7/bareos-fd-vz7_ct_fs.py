# Provided by the Bareos FD Python plugin interface
import bareosfd

# This module contains the wrapper functions called by the Bareos-FD, the
# functions call the corresponding methods from your plugin class
import BareosFdWrapper
# from BareosFdWrapper import parse_plugin_definition, handle_plugin_event, start_backup_file, end_backup_file, start_restore_file, end_restore_file, restore_object_data, plugin_io, create_file, check_file, handle_backup_file  # noqa
from BareosFdWrapper import *  # noqa

# This module contains the used plugin class
import BareosFdPluginVz7CtFs


def load_bareos_plugin(plugindef):
    '''
    This function is called by the Bareos-FD to load the plugin
    We use it to instantiate the plugin class
    '''
    # BareosFdWrapper.bareos_fd_plugin_object is the module attribute that
    # holds the plugin class object
    BareosFdWrapper.bareos_fd_plugin_object = \
        BareosFdPluginVz7CtFs.BareosFdPluginVz7CtFs(
            plugindef)
    return bareosfd.bRC_OK
