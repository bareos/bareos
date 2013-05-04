#
# If no version is forced from the outside set a default.
#
ifeq ($(WIN_VERSION),)
WIN_VERSION=32
endif

#
# If no debug if forced from the ouside set a default.
#
ifeq ($(WIN_DEBUG),)
WIN_DEBUG=no
endif

MINGW_32_BASE = /usr/i686-w64-mingw32
MINGW_64_BASE = /usr/x86_64-w64-mingw32

ifeq ($(WIN_VERSION),32)
QMAKE = /usr/bin/i686-w64-mingw32-qmake
WINDRES = /usr/bin/i686-w64-mingw32-windres
CXX = /usr/bin/i686-w64-mingw32-g++
CXXFLAGS = $(DEFINES) $(INCLUDES) -Wall -m$(WIN_VERSION) -mwin32 -mthreads -O3 -fno-strict-aliasing -Wno-unknown-pragmas
MINGW_LIB = $(MINGW_32_BASE)/sys-root/mingw/lib
else
QMAKE = /usr/bin/x86_64-w64-mingw32-qmake
WINDRES = /usr/bin/x86_64-w64-mingw32-windres
CXX = /usr/bin/x86_64-w64-mingw32-g++
CXXFLAGS = $(DEFINES) $(INCLUDES) -Wall -m$(WIN_VERSION) -mwin32 -mthreads -O3 -fno-strict-aliasing -Wno-unknown-pragmas
MINGW_LIB = $(MINGW_64_BASE)/sys-root/mingw/lib
endif
WINSOCKLIB = -lws2_32
LDFLAGS_DLL = -mdll -mwindows
LDFLAGS_WINAPP = -mwindows
LDFLAGS_CONS = -mconsole

ifeq ($(WIN_DEBUG),yes)
CXXFLAGS += -g
endif
