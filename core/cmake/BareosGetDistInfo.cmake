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


#get DIST info in format "$DISTNAME;$DISTVER;$OBS_PROJECT;$OBS_DISTRIBUTION;$OBS_ARCH;$OBS_SRCMD5"

execute_process(COMMAND ${PROJECT_SOURCE_DIR}/cmake/distname.sh
      OUTPUT_VARIABLE DISTINFOTXT
      OUTPUT_STRIP_TRAILING_WHITESPACE)

set(DISTINFO ${DISTINFOTXT})


list(LENGTH DISTINFO DISTINFO_LENGTH)


list(GET DISTINFO 0 DISTNAME)
set( ENV{DISTNAME} ${DISTNAME} )

list(GET DISTINFO 1 DISTVER)
set( ENV{DISTVER} "${DISTVER}" )


if (${DISTINFO_LENGTH} GREATER 2)
   SET(IS_BUILD_ON_OBS 1)

   list(GET DISTINFO 2 OBS_PROJECT)
   set( ENV{OBS_PROJECT} "${OBS_PROJECT}" )

   list(GET DISTINFO 3 OBS_DISTRIBUTION)
   set( ENV{OBS_DISTRIBUTION} "${OBS_DISTRIBUTION}" )

   list(GET DISTINFO 4 OBS_ARCH)
   set( ENV{OBS_ARCH} "${OBS_ARCH}" )

   list(GET DISTINFO 5 OBS_SRCMD5)
   set( ENV{OBS_SRCMD5} "${OBS_SRCMD5}" )
endif()


set(Host "${CMAKE_SYSTEM} ${LSB_RELEASE_DESCRIPTION}")
set(Distribution ${LSB_RELEASE_ID_SHORT})
