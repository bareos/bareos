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
  bareos_add_plugin(python3-sd)
  target_sources(python3-sd PRIVATE python-sd.cc module.cc)
  install(TARGETS python3-sd DESTINATION ${plugindir})
  target_link_libraries(
    python3-sd ${Python3_LIBRARIES} Bareos::Lib python-common
  )
  target_compile_definitions(
    python3-sd PUBLIC PY3MODDIR=\"${Python3_SITEARCH}\"
  )
  target_include_directories(python3-sd PUBLIC ${Python3_INCLUDE_DIRS})

  set(PYFILES pyfiles/bareos-sd-class-plugin.py
              pyfiles/BareosSdPluginBaseclass.py pyfiles/BareosSdWrapper.py
  )

  install(FILES ${PYFILES} DESTINATION ${plugindir})
endif()

include(DebugEdit)
