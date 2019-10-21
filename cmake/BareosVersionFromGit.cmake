# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2019-2019 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or modify it under
# the terms of version three of the GNU Affero General Public License as
# published by the Free Software Foundation and included in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
# details.
#
# You should have received a copy of the GNU Affero General Public License along
# with this program; if not, write to the Free Software Foundation, Inc., 51
# Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

# Extract version information and commit timestamp if run in a git checkout

if(Git_FOUND)
  execute_process(COMMAND ${GIT_EXECUTABLE}
                          log
                          -1
                          --pretty=format:%ct
                  RESULT_VARIABLE GIT_COMMIT_TIMESTAMP_RESULT
                  OUTPUT_VARIABLE GIT_COMMIT_TIMESTAMP
                  ERROR_QUIET)
endif()

if(GIT_COMMIT_TIMESTAMP_RESULT EQUAL 0)
  execute_process(COMMAND ${GIT_EXECUTABLE}
                          describe
                          --tags
                          --exact-match
                          --match
                          "Release/*"
                          --dirty=.dirty
                  RESULT_VARIABLE GIT_DESCRIBE_RELEASE_RESULT
                  OUTPUT_VARIABLE GIT_DESCRIBE_OUTPUT
                  ERROR_QUIET
                  OUTPUT_STRIP_TRAILING_WHITESPACE)
  if(NOT GIT_DESCRIBE_RELEASE_RESULT EQUAL 0)
    execute_process(COMMAND ${GIT_EXECUTABLE}
                            describe
                            --tags
                            --match
                            "WIP/*"
                            --dirty=.dirty
                    RESULT_VARIABLE GIT_DESCRIBE_WIP_RESULT
                    OUTPUT_VARIABLE GIT_DESCRIBE_OUTPUT
                    ERROR_QUIET
                    OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
endif()

if(NOT GIT_DESCRIBE_OUTPUT STREQUAL "")
  set(GIT_DESCRIBE_REGEX_LONG
      "^([^/]+)/([^-]+)-(([^-]+)?)-?([0-9]+)-g([0-9a-f]+(.dirty)?)[ \n]*")
  set(GIT_DESCRIBE_REPLACE_LONG "\\2~\\3\\5.\\6")
  set(GIT_DESCRIBE_REGEX_SHORT "^([^/]+)/([0-9.]+)((-[^-]+)?)((.dirty)?)[ \n]*")
  set(GIT_DESCRIBE_REPLACE_SHORT "\\2\\3\\5")

  string(REGEX MATCH
               "${GIT_DESCRIBE_REGEX_LONG}"
               GIT_DESCRIBE_REGEX_LONG_MATCH
               "${GIT_DESCRIBE_OUTPUT}")
  if(GIT_DESCRIBE_REGEX_LONG_MATCH STREQUAL "")
    string(REGEX
           REPLACE "${GIT_DESCRIBE_REGEX_SHORT}"
                   "${GIT_DESCRIBE_REPLACE_SHORT}"
                   GIT_DESCRIBE_VERSION
                   "${GIT_DESCRIBE_OUTPUT}")
  else()
    string(REGEX
           REPLACE "${GIT_DESCRIBE_REGEX_LONG}"
                   "${GIT_DESCRIBE_REPLACE_LONG}"
                   GIT_DESCRIBE_VERSION
                   "${GIT_DESCRIBE_OUTPUT}")
  endif()
endif()
