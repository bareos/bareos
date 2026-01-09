#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2026 Bareos GmbH & Co. KG
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

include(BareosPlugins)

if(Python3_FOUND)
  add_library(python-common common.cc)
  target_link_libraries(python-common PUBLIC Python3::Python Bareos::Lib)

  bareos_add_plugin(python3-dir)
  target_sources(python3-dir PRIVATE python-dir.cc module.cc)
  install(TARGETS python3-dir DESTINATION ${plugindir})
  target_link_libraries(
    python3-dir Python3::Python Bareos::Lib fmt::fmt python-common
  )
  target_compile_definitions(
    python3-dir PUBLIC PY3MODDIR=\"${Python3_SITEARCH}\"
  )

  set(PYFILES pyfiles/bareos-dir-class-plugin.py
              pyfiles/BareosDirPluginBaseclass.py pyfiles/BareosDirWrapper.py
  )

  install(FILES ${PYFILES} DESTINATION ${plugindir})
endif()

include(DebugEdit)
