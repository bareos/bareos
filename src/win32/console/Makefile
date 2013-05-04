VPATH = .:../../console

DEFINES = -DHAVE_WIN32 \
	  -DHAVE_MINGW \
	  -DHAVE_ZLIB_H \
	  -DHAVE_LIBZ \
	  -DHAVE_CRYPTO \
	  -DHAVE_OPENSSL \
	  -DHAVE_TLS \
	  -DHAVE_READLINE \
	  -DMINGW64

include ../Makefile.inc
ifeq ($(WIN_VERSION),32)
INCLUDES = -I../.. \
	   -I../../include \
	   -I../include \
	   -I../compat/include \
	   -I../vss/include
else
INCLUDES = -I../.. \
	   -I../../include \
	   -I../include \
	   -I../compat/include \
	   -I../vss/include
endif
LDLIBS = ../lib/libbareos.a \
	 ../lib/libbareos.dll \
         $(MINGW_LIB)/libpthreadGCE2.a \
         $(MINGW_LIB)/libz.dll.a \
         $(MINGW_LIB)/libreadline.dll.a \
	 $(WINSOCKLIB) -lole32 -loleaut32 -luuid

CONSSRCS = console.c console_conf.c authenticate.c
CONSOBJS = $(CONSSRCS:.c=.o)

WINDRESSRCS = consoleres.rc
WINDRESOBJS = $(WINDRESSRCS:.rc=.o)

all: bconsole.exe

bconsole.exe: DLL_USAGE = -DUSING_DLL
bconsole.exe: $(CONSOBJS) $(WINDRESOBJS)
	$(CXX) $(LDFLAGS_CONS) $(CONSOBJS) $(LDLIBS) $(WINDRESOBJS) -o $@

clean:
	rm -f *.o

distclean:: clean
	rm -f bconsole.exe

# inference rules
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<

%.o : %.rc
	@echo "Ressource Compiling $^"
	$(WINDRES) $^ -o $@

