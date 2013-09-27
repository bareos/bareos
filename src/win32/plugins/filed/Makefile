VPATH = .:../../../plugins/filed

DEFINES = -DHAVE_WIN32 \
	  -DHAVE_MINGW \
	  -DHAVE_ZLIB_H \
	  -DHAVE_LIBZ \
	  -DHAVE_CRYPTO \
	  -DHAVE_OPENSSL \
	  -DHAVE_TLS \
	  -DMINGW64

include ../../Makefile.inc
ifeq ($(WIN_VERSION),32)
INCLUDES = -I../../.. \
	   -I../../../include \
	   -I../../../filed \
	   -I../../../plugins/filed \
	   -I../../include \
	   -I../../compat/include
VDI_INCLUDES = -I../../vdi/include
else
INCLUDES = -I../../.. \
	   -I../../../include \
	   -I../../../filed \
	   -I../../../plugins/filed \
	   -I../../include \
	   -I../../compat/include
VDI_INCLUDES = -I../../vdi/include
endif
LDLIBS = ../../lib/libbareos.dll \
	 $(MINGW_LIB)/libssl.dll.a \
         $(MINGW_LIB)/libcrypto.dll.a \
         $(MINGW_LIB)/libpthreadGCE2.a \
         $(MINGW_LIB)/libz.dll.a \
	 $(WINSOCKLIB) -lole32 -loleaut32 -luuid

all: bpipe-fd.dll mssqlvdi-fd.dll

bpipe-fd.dll: DLL_USAGE = -DBUILDING_DLL
bpipe-fd.dll: bpipe-fd.o
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a bpipe-fd.o $(LDLIBS) -o $@

mssqlvdi-fd.o: CXXFLAGS += -fpermissive
mssqlvdi-fd.o: mssqlvdi-fd.c
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) $(VDI_INCLUDES) -c -o $@ $<

mssqlvdi-fd.dll: DLL_USAGE = -DBUILDING_DLL
mssqlvdi-fd.dll: mssqlvdi-fd.o
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a mssqlvdi-fd.o $(LDLIBS) -o $@

clean:
	rm -f *.o

distclean:: clean
	rm -f bpipe-fd.dll mssqlvdi-fd.dll

# inference rules
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<
