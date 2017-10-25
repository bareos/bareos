#   BAREOS�� - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2017 Bareos GmbH & Co. KG
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


include_directories(../.. . ../../include ../../stored)


add_library(autoxflate-sd MODULE autoxflate-sd.cc)
# do not prefix with "lib"
set_target_properties(autoxflate-sd PROPERTIES PREFIX "")
INSTALL(TARGETS autoxflate-sd DESTINATION ${plugindir})

add_library(scsicrypto-sd MODULE scsicrypto-sd.cc)
# do not prefix with "lib"
set_target_properties(scsicrypto-sd PROPERTIES PREFIX "")
INSTALL(TARGETS scsicrypto-sd DESTINATION ${plugindir})

add_library(scsitapealert-sd MODULE scsitapealert-sd.cc)
# do not prefix with "lib"
set_target_properties(scsitapealert-sd PROPERTIES PREFIX "")
INSTALL(TARGETS scsitapealert-sd DESTINATION ${plugindir})

add_library(python-sd MODULE python-sd.cc)
# do not prefix with "lib"
set_target_properties(python-sd PROPERTIES PREFIX "")
INSTALL(TARGETS python-sd DESTINATION ${plugindir})


set (PYFILES bareos-sd-class-plugin.py
             bareos_sd_consts.py
             BareosSdPluginBaseclass.py
             bareos-sd.py.template
             BareosSdWrapper.py)

INSTALL(FILES ${PYFILES} DESTINATION ${plugindir})
