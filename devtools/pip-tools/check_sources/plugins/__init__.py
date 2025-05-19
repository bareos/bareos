#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2025 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

import pkgutil
from importlib import import_module
import logging

logger = logging.getLogger(__name__)


def list_plugins():
    def normalize_name(plugin_name):
        if plugin_name.startswith(__name__ + "."):
            plugin_name = plugin_name[len(__name__) + 1 :]  # noqa: E203
        if plugin_name.endswith("_plugin"):
            plugin_name = plugin_name[:-7]
        return plugin_name

    all_plugins = {}
    for plugin_module in pkgutil.iter_modules(__path__, __name__ + "."):
        # skip packages
        if plugin_module.ispkg:
            continue
        all_plugins[normalize_name(plugin_module.name)] = plugin_module.name
    return all_plugins


def load_plugins(plugin_whitelist=None):
    """Load plugins.

    Lookup all modules in our package, and load them if the match the
    supplied plugins or load all plugins if None specified.
    """
    all_plugins = list_plugins()
    if plugin_whitelist:
        for plugin_name in plugin_whitelist:
            if plugin_name in all_plugins:
                logger.debug("loading plugin '%s'", plugin_name)
                import_module(all_plugins[plugin_name])
            else:
                logger.error("cannot find plugin %s - skipping.", plugin_name)
    else:
        for plugin_shortname, plugin_fullname in all_plugins.items():
            logger.debug("loading plugin %s", plugin_shortname)
            import_module(plugin_fullname)
