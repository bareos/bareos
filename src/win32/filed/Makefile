VPATH = .:../../filed

DEFINES = -DHAVE_WIN32 \
	  -DHAVE_MINGW \
	  -DWIN32_VSS \
	  -DWINVER=0x500 \
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
DEFINES += -DHAVE_VSS64
endif
LDLIBS = ../lib/libbareos.a \
	 ../lib/libbareos.dll \
	 ../findlib/libbareosfind.dll \
         $(MINGW_LIB)/libpthreadGCE2.a \
         $(MINGW_LIB)/libz.dll.a \
         $(MINGW_LIB)/liblzo2.dll.a \
	 $(WINSOCKLIB) -lole32 -loleaut32 -luuid -lcomctl32

SVRSRCS = accurate.c authenticate.c backup.c compression.c \
	  crypto.c estimate.c fd_plugins.c filed_conf.c filed.c \
	  fileset.c heartbeat.c job.c restore.c status.c verify.c \
	  verify_vol.c vss.c vss_XP.c vss_W2K3.c vss_Vista.c \
	  service.c main.c
SVROBJS = $(SVRSRCS:.c=.o)

WINDRESSRCS = icon.rc
WINDRESOBJS = $(WINDRESSRCS:.rc=.o)


all: bareos-fd.exe

bareos-fd.exe: DLL_USAGE = -DUSING_DLL
bareos-fd.exe: $(SVROBJS) $(WINDRESOBJS)
	$(CXX) $(LDFLAGS_WINAPP) $(SVROBJS) $(WINDRESOBJS) $(LDLIBS) -o $@

clean:
	rm -f *.o

distclean:: clean
	rm -f bareos-fd.exe

# inference rules
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<

%.o : %.rc
	@echo "Ressource Compiling $^"
	$(WINDRES) $^ -o $@
