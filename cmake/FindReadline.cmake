# Looking for rlconf.h ensures we get GNU readline and not libedit,
# which we need.

IF( NOT Readline_ROOT_DIR )
   # Search for the path containing library's headers
   find_path( Readline_ROOT_DIR
      NAMES include/readline/rlconf.h
      )
ENDIF()

# Search for include directory
find_path( Readline_INCLUDE_DIR
   NAMES readline/rlconf.h
   HINTS ${Readline_ROOT_DIR}/include
   PATHS /usr/local/opt/readline/include /usr/local/include /opt/local/include /usr/include
   )

# Search for library
find_library( Readline_LIBRARY
   NAMES readline
   HINTS ${Readline_ROOT_DIR}/lib
   PATHS /usr/local/opt/readline/lib /usr/local/lib /opt/local/lib /usr/lib
   )

# Conditionally set READLINE_FOUND value
if( Readline_INCLUDE_DIR AND Readline_LIBRARY AND Ncurses_LIBRARY )
   set( READLINE_FOUND TRUE )
else()
   FIND_LIBRARY( Readline_LIBRARY NAMES readline )
   include( FindPackageHandleStandardArgs )
   FIND_PACKAGE_HANDLE_STANDARD_ARGS(
      Readline DEFAULT_MSG
      Readline_INCLUDE_DIR Readline_LIBRARY
      )
endif()

# Hide these variables in cmake GUIs
mark_as_advanced(
   Readline_ROOT_DIR
   Readline_INCLUDE_DIR
   Readline_LIBRARY
)
