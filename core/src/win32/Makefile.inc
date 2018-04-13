
#
# If no version is forced from the outside set a default.
#
ifeq ($(WIN_VERSION),)
WIN_VERSION = 64
endif


#
# If no debug if forced from the ouside set a default.
#
ifeq ($(WIN_DEBUG),)
WIN_DEBUG = no
endif

# currently not working, missing qt debug packages
#ifeq ($(WIN_DEBUG),yes)
#QMAKE_ARGS += "ConfigurationParser+=debug"
#endif

#
# If no vistacompat is forced from the outside set a default.
#
ifeq ($(WIN_VISTACOMPAT),)
WIN_VISTACOMPAT = no
endif

ifeq ($(WIN_VISTACOMPAT),yes)
WINDOWS_VERSION = 0x600
QMAKE_ARGS += "DEFINES+=_WIN32_WINNT=$(WINDOWS_VERSION)"
else
WINDOWS_VERSION = 0x500
QMAKE_ARGS += "DEFINES+=_WIN32_WINNT=$(WINDOWS_VERSION)"
endif

MINGW_32_BASE = /usr/i686-w64-mingw32
MINGW_64_BASE = /usr/x86_64-w64-mingw32

ifeq ($(WIN_VERSION),32)
DEFINES = -DHAVE_WIN32 -DHAVE_MINGW -D_WIN32_WINNT=$(WINDOWS_VERSION)
QMAKE = /usr/bin/i686-w64-mingw32-qmake
WINDRES = /usr/bin/i686-w64-mingw32-windres
CC = /usr/bin/i686-w64-mingw32-gcc
CXX = /usr/bin/i686-w64-mingw32-g++
ifeq ($(WIN_DEBUG),yes)
CFLAGS = $(DEFINES) $(INCLUDES) -Wall -m$(WIN_VERSION) -mwin32 -mthreads -g
CXXFLAGS = $(DEFINES) $(INCLUDES) -Wall -m$(WIN_VERSION) -mwin32 -mthreads -fno-strict-aliasing -Wno-unknown-pragmas -g
else
CFLAGS = $(DEFINES) $(INCLUDES) -Wall -m$(WIN_VERSION) -mwin32 -mthreads -O3
CXXFLAGS = $(DEFINES) $(INCLUDES) -Wall -m$(WIN_VERSION) -mwin32 -mthreads -O3 -fno-strict-aliasing -Wno-unknown-pragmas
endif
MINGW_LIB = $(MINGW_32_BASE)/sys-root/mingw/lib
else
DEFINES = -DMINGW64 -DHAVE_WIN32 -DHAVE_MINGW -D_WIN32_WINNT=$(WINDOWS_VERSION)
QMAKE = /usr/bin/x86_64-w64-mingw32-qmake
WINDRES = /usr/bin/x86_64-w64-mingw32-windres
CC = /usr/bin/x86_64-w64-mingw32-gcc
CXX = /usr/bin/x86_64-w64-mingw32-g++
ifeq ($(WIN_DEBUG),yes)
CFLAGS = $(DEFINES) $(INCLUDES) -Wall -m$(WIN_VERSION) -mwin32 -mthreads -g
CXXFLAGS = $(DEFINES) $(INCLUDES) -Wall -m$(WIN_VERSION) -mwin32 -mthreads -fno-strict-aliasing -Wno-unknown-pragmas -g
else
CFLAGS = $(DEFINES) $(INCLUDES) -Wall -m$(WIN_VERSION) -mwin32 -mthreads -O3
CXXFLAGS = $(DEFINES) $(INCLUDES) -Wall -m$(WIN_VERSION) -mwin32 -mthreads -O3 -fno-strict-aliasing -Wno-unknown-pragmas
endif
MINGW_LIB = $(MINGW_64_BASE)/sys-root/mingw/lib
endif
WINSOCKLIB = -lws2_32
LDFLAGS_DLL = -mdll -mwindows
LDFLAGS_WINAPP = -mwindows
LDFLAGS_CONS = -mconsole
