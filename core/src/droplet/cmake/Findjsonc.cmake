# taken from https://gitlab.cern.ch/dss/eos/commit/cd94310c84db2af9de47ff5b197421205b550db2
# Try to find json-c
# Once done, this will define
#
# JSONC_FOUND         - system has jsonc
# JSONC__INCLUDE_DIRS - the jsonc include directories
# JSONC__LIBRARIES    - jsonc libraries directories

if(JSONC_INCLUDE_DIRS AND JSONC_LIBRARIES)
set(JSONC_FIND_QUIETLY TRUE)
endif(JSONC_INCLUDE_DIRS AND JSONC_LIBRARIES)

find_path(JSONC_INCLUDE_DIR json.h
                           HINTS
                           /usr/include/json-c/
                           /usr/local/include/json-c/
			   )
find_library(JSONC_LIBRARY json-c
			  HINTS
			  /usr/lib/
                          /usr/local/lib
			  )

set(JSONC_INCLUDE_DIRS ${JSONC_INCLUDE_DIR})
set(JSONC_LIBRARIES ${JSONC_LIBRARY})

# handle the QUIETLY and REQUIRED arguments and set JSONC_FOUND to TRUE if
# all listed variables are TRUE
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(jsonc DEFAULT_MSG JSONC_INCLUDE_DIRS JSONC_LIBRARIES)

mark_as_advanced(JSONC_INCLUDE_DIRS JSONC_LIBRARIES)

