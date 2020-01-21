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

macro(
  BareosInstallConfigFiles
  CONFDIR
  CONFIGBASEDIRECTORY
  PLUGINS
  BACKENDS
  SRC_DIR
)

  message(
    STATUS
      "BareosInstallConfigFiles called with CONFDIR:${CONFDIR} CONFIGBASEDIRECTORY:${CONFIGBASEDIRECTORY} PLUGINS:${PLUGINS} BACKENDS:${BACKENDS} SRC_DIR:${SRC_DIR}"
  )
  message(
    STATUS "CPACK_PACKAGING_INSTALL_PREFIX: ${CPACK_PACKAGING_INSTALL_PREFIX}"
  )
  message(STATUS "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
  message(STATUS "DESTDIR: $ENV{DESTDIR}")

  if(IS_ABSOLUTE ${CONFDIR})
    set(DESTCONFDIR "$ENV{DESTDIR}${CONFDIR}/${CONFIGBASEDIRECTORY}/")
  else()
    set(
      DESTCONFDIR
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/${CONFDIR}/${CONFIGBASEDIRECTORY}/"
    )
  endif()

  message(
    STATUS
      "installing configuration ${CONFIGBASEDIRECTORY}  resource files to ${DESTCONFDIR}"
  )

  message(
    STATUS "globbing ${SRC_DIR}/src/defaultconfigs/${CONFIGBASEDIRECTORY}/*"
  )
  file(GLOB resourcedirs
       "${SRC_DIR}/src/defaultconfigs/${CONFIGBASEDIRECTORY}/*")
  foreach(resdir ${resourcedirs})
    file(GLOB configfiles "${resdir}/*.conf")
    get_filename_component(resname ${resdir} NAME)
    foreach(configfile ${configfiles})
      get_filename_component(fname ${configfile} NAME)
      if(EXISTS ${DESTCONFDIR}/${resname}/${fname})
        message(STATUS "${DESTCONFDIR}/${resname}/${fname} exists")
        message(STATUS "rename ${configfile} to ${configfile}.new")
        file(RENAME "${configfile}" "${configfile}.new")

        message(STATUS "copy ${configfile}.new to ${DESTCONFDIR}/${resname}")
        file(
          INSTALL "${configfile}.new"
          DESTINATION "${DESTCONFDIR}/${resname}"
        )
        file(RENAME "${configfile}.new" "${configfile}")
      else()
        message(
          STATUS
            "${resname}/${fname} as ${resname}/${fname} (new installation)"
        )
        file(COPY "${configfile}" DESTINATION "${DESTCONFDIR}/${resname}")
      endif()
    endforeach()
  endforeach()

  # add bareos-dir-export for bareos-dir
  if(${CONFIGBASEDIRECTORY} STREQUAL "bareos-dir.d")
    file(MAKE_DIRECTORY "${DESTCONFDIR}/../bareos-dir-export/client/")
    file(MAKE_DIRECTORY "${DESTCONFDIR}/counter")
    file(MAKE_DIRECTORY "${DESTCONFDIR}/user")
  endif()

  # add ndmp resource directory for bareos-sd
  if(${CONFIGBASEDIRECTORY} STREQUAL "bareos-sd.d")
    file(MAKE_DIRECTORY "${DESTCONFDIR}/ndmp")
  endif()

  # install configs from sd backends
  foreach(BACKEND ${BACKENDS})

    message(
      STATUS
        "install ${CONFIGBASEDIRECTORY} config files for BACKEND ${BACKEND}"
    )
    set(
      BackendConfigSrcDir
      "${SRC_DIR}/src/stored/backends/${BACKEND}/${CONFIGBASEDIRECTORY}"
    )

    file(
      GLOB_RECURSE configfiles
      RELATIVE "${BackendConfigSrcDir}"
      "${BackendConfigSrcDir}/*.conf"
    )
    foreach(configfile ${configfiles})
      get_filename_component(dir ${configfile} DIRECTORY)
      get_filename_component(fname ${configfile} NAME)

      if(EXISTS ${DESTCONFDIR}/${configfile})
        message(STATUS "${configfile} as ${configfile}.new (keep existing)")
        file(RENAME "${BackendConfigSrcDir}/${configfile}"
             "${BackendConfigSrcDir}/${configfile}.new")
        file(
          COPY "${BackendConfigSrcDir}/${configfile}.new"
          DESTINATION "${DESTCONFDIR}/${dir}"
        )
        file(RENAME "${BackendConfigSrcDir}/${configfile}.new"
             "${BackendConfigSrcDir}/${configfile}")
      else()
        message(STATUS "${configfile} as ${configfile}")
        file(
          COPY "${BackendConfigSrcDir}/${configfile}"
          DESTINATION "${DESTCONFDIR}/${dir}"
        )
      endif()
    endforeach()

    file(
      GLOB_RECURSE configfiles
      RELATIVE "${BackendConfigSrcDir}"
      "${BackendConfigSrcDir}/*.example"
    )
    foreach(configfile ${configfiles})
      get_filename_component(dir ${configfile} DIRECTORY)
      # get_filename_component(fname ${configfile} NAME)

      if(EXISTS ${DESTCONFDIR}/${configfile})
        message(STATUS "overwriting ${configfile}")
      else()
        message(STATUS "${configfile} as ${configfile}")
      endif()

      file(
        COPY "${BackendConfigSrcDir}/${configfile}"
        DESTINATION "${DESTCONFDIR}/${dir}"
      )
    endforeach()

  endforeach()

  # install configs from  fd plugins
  foreach(PLUGIN ${PLUGINS})
    message(STATUS "install config files for PLUGIN ${PLUGIN}")
    file(GLOB resourcedirs
         "${SRC_DIR}/src/plugins/filed/${PLUGIN}/${CONFIGBASEDIRECTORY}/*")
    foreach(resdir ${resourcedirs})
      file(GLOB configfiles "${resdir}/*.conf*")
      get_filename_component(resname ${resdir} NAME)
      foreach(configfile ${configfiles})
        string(
          REGEX
            MATCH
            "\\.in\$"
            IS_INFILE
            ${configfile}
        )
        if(NOT "${IS_INFILE}" STREQUAL ".in")
          get_filename_component(fname ${configfile} NAME)
          if(EXISTS ${DESTCONFDIR}/${resname}/${fname})
            message(
              STATUS
                "${resname}/${fname} as ${resname}/${fname}.new (keep existing)"
            )
            file(RENAME "${configfile}" "${configfile}.new")
            file(
              COPY "${configfile}.new"
              DESTINATION "${DESTCONFDIR}/${resname}"
            )
            file(RENAME "${configfile}.new" "${configfile}")
          else()
            message(STATUS "${resname}/${fname} as ${resname}/${fname}")
            file(COPY "${configfile}" DESTINATION "${DESTCONFDIR}/${resname}")
          endif()
        else()
          message(STATUS "skipping .in file ${configfile}:${IS_INFILE}")
        endif()
      endforeach()
    endforeach()
  endforeach()

endmacro(BareosInstallConfigFiles)
