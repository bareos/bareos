# Search for the path containing library's headers
find_path(Readline_ROOT_DIR NAMES include/readline/readline.h)

# Search for include directory
find_path(
  Readline_INCLUDE_DIR
  NAMES readline/readline.h
  HINTS ${Readline_ROOT_DIR}/include
)

# Search for library
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(Readline_LIBRARY /usr/local/opt/readline/lib/libreadline.a ncurses)
else()
  find_library(Readline_LIBRARY NAMES readline HINTS ${Readline_ROOT_DIR}/lib)
endif()
# Conditionally set READLINE_FOUND value
if(Readline_INCLUDE_DIR AND Readline_LIBRARY AND Ncurses_LIBRARY)
  set(READLINE_FOUND TRUE)
else(Readline_INCLUDE_DIR AND Readline_LIBRARY AND Ncurses_LIBRARY)
  find_library(Readline_LIBRARY NAMES readline)
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(
    Readline
    DEFAULT_MSG
    Readline_INCLUDE_DIR
    Readline_LIBRARY
  )
  mark_as_advanced(Readline_INCLUDE_DIR Readline_LIBRARY)
endif(Readline_INCLUDE_DIR AND Readline_LIBRARY AND Ncurses_LIBRARY)

# Hide these variables in cmake GUIs
mark_as_advanced(Readline_ROOT_DIR Readline_INCLUDE_DIR Readline_LIBRARY)
