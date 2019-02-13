#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2019 Bareos GmbH & Co. KG
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



MACRO(BareosInstallConfigFiles CONFDIR CONFIGBASEDIRECTORY PLUGINS BACKENDS SRC_DIR)

MESSAGE(STATUS  "BareosInstallConfigFiles called with CONFDIR:${CONFDIR} CONFIGBASEDIRECTORY:${CONFIGBASEDIRECTORY} PLUGINS:${PLUGINS} BACKENDS:${BACKENDS} SRC_DIR:${SRC_DIR}")
MESSAGE(STATUS "CPACK_PACKAGING_INSTALL_PREFIX: ${CPACK_PACKAGING_INSTALL_PREFIX}")
MESSAGE(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
MESSAGE(STATUS "DESTDIR: $ENV{DESTDIR}")

IF (IS_ABSOLUTE ${CONFDIR})
    set (DESTCONFDIR "$ENV{DESTDIR}${CONFDIR}/${CONFIGBASEDIRECTORY}/")
ELSE()
    set (DESTCONFDIR "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${CONFDIR}/${CONFIGBASEDIRECTORY}/")
ENDIF()



MESSAGE(STATUS  "installing configuration ${CONFIGBASEDIRECTORY}  resource files to ${DESTCONFDIR}")

MESSAGE(STATUS  "globbing ${SRC_DIR}/src/defaultconfigs/${CONFIGBASEDIRECTORY}/*")
file(GLOB resourcedirs "${SRC_DIR}/src/defaultconfigs/${CONFIGBASEDIRECTORY}/*")
foreach(resdir ${resourcedirs})
   file(GLOB configfiles "${resdir}/*.conf")
   get_filename_component(resname ${resdir} NAME)
   foreach(configfile ${configfiles})
      get_filename_component(fname ${configfile} NAME)
      if (EXISTS ${DESTCONFDIR}/${resname}/${fname})
         MESSAGE(STATUS "${DESTCONFDIR}/${resname}/${fname} exists")
         MESSAGE(STATUS "rename ${configfile} to ${configfile}.new")
         FILE (RENAME "${configfile}" "${configfile}.new")

         MESSAGE(STATUS "copy ${configfile}.new to ${DESTCONFDIR}/${resname}")
         FILE (INSTALL "${configfile}.new" DESTINATION "${DESTCONFDIR}/${resname}")
         FILE (RENAME "${configfile}.new" "${configfile}")
      else()
         MESSAGE(STATUS "${resname}/${fname} as ${resname}/${fname} (new installation)")
         FILE (COPY "${configfile}" DESTINATION "${DESTCONFDIR}/${resname}")
      endif()
   endforeach()
endforeach()

# add bareos-dir-export for bareos-dir
if (${CONFIGBASEDIRECTORY} STREQUAL "bareos-dir.d")
   file(MAKE_DIRECTORY "${DESTCONFDIR}/../bareos-dir-export/client/")
   file(MAKE_DIRECTORY "${DESTCONFDIR}/counter")
endif()

# add ndmp resource directory for bareos-sd
if (${CONFIGBASEDIRECTORY} STREQUAL "bareos-sd.d")
   file(MAKE_DIRECTORY "${DESTCONFDIR}/ndmp")
endif()



# install configs from sd backends
FOREACH (BACKEND ${BACKENDS})

   MESSAGE(STATUS "install ${CONFIGBASEDIRECTORY} config files for BACKEND ${BACKEND}")
   set(BackendConfigSrcDir "${SRC_DIR}/src/stored/backends/${BACKEND}/${CONFIGBASEDIRECTORY}")

   file(GLOB_RECURSE configfiles  RELATIVE "${BackendConfigSrcDir}" "${BackendConfigSrcDir}/*.conf")
   foreach(configfile ${configfiles})
      get_filename_component(dir   ${configfile} DIRECTORY)
      get_filename_component(fname ${configfile} NAME)

      if (EXISTS ${DESTCONFDIR}/${configfile})
         MESSAGE(STATUS "${configfile} as ${configfile}.new (keep existing)")
         FILE(RENAME "${BackendConfigSrcDir}/${configfile}" "${BackendConfigSrcDir}/${configfile}.new")
         FILE(COPY   "${BackendConfigSrcDir}/${configfile}.new" DESTINATION "${DESTCONFDIR}/${dir}")
         FILE(RENAME "${BackendConfigSrcDir}/${configfile}.new" "${BackendConfigSrcDir}/${configfile}")
      else()
         MESSAGE(STATUS "${configfile} as ${configfile}")
         FILE(COPY "${BackendConfigSrcDir}/${configfile}" DESTINATION "${DESTCONFDIR}/${dir}")
      endif()
   endforeach()

   file(GLOB_RECURSE configfiles RELATIVE "${BackendConfigSrcDir}" "${BackendConfigSrcDir}/*.example")
   foreach(configfile ${configfiles})
      get_filename_component(dir   ${configfile} DIRECTORY)
      #get_filename_component(fname ${configfile} NAME)

      if (EXISTS ${DESTCONFDIR}/${configfile})
         MESSAGE(STATUS "overwriting ${configfile}")
      else()
         MESSAGE(STATUS "${configfile} as ${configfile}")
      endif()

      FILE(COPY "${BackendConfigSrcDir}/${configfile}" DESTINATION "${DESTCONFDIR}/${dir}")
   endforeach()

ENDFOREACH()



# install configs from  fd plugins
FOREACH (PLUGIN ${PLUGINS})
   MESSAGE(STATUS "install config files for PLUGIN ${PLUGIN}")
   file(GLOB resourcedirs "${SRC_DIR}/src/plugins/filed/${PLUGIN}/${CONFIGBASEDIRECTORY}/*")
   foreach(resdir ${resourcedirs})
      file(GLOB configfiles "${resdir}/*.conf*")
      get_filename_component(resname ${resdir} NAME)
      foreach(configfile ${configfiles})
         STRING(REGEX MATCH "\\.in\$" IS_INFILE ${configfile})
         if (NOT "${IS_INFILE}" STREQUAL ".in")
            get_filename_component(fname ${configfile} NAME)
            if (EXISTS ${DESTCONFDIR}/${resname}/${fname})
               MESSAGE(STATUS "${resname}/${fname} as ${resname}/${fname}.new (keep existing)")
               FILE (RENAME "${configfile}" "${configfile}.new")
               FILE (COPY "${configfile}.new" DESTINATION "${DESTCONFDIR}/${resname}")
               FILE (RENAME "${configfile}.new" "${configfile}")
            else()
               MESSAGE(STATUS "${resname}/${fname} as ${resname}/${fname}")
               FILE (COPY "${configfile}" DESTINATION "${DESTCONFDIR}/${resname}")
            endif()
         else()
            MESSAGE(STATUS "skipping .in file ${configfile}:${IS_INFILE}")
         endif()
      endforeach()
   endforeach()
ENDFOREACH()

ENDMACRO(BareosInstallConfigFiles)
