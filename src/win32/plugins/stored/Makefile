VPATH = .:../../../plugins/stored

include ../../Makefile.inc

ifeq ($(WIN_VERSION),32)
INCLUDES = -I../../.. \
	   -I../../../include \
	   -I../../../stored \
	   -I../../../plugins/stored \
	   -I../../include \
	   -I../../compat/include
else
INCLUDES = -I../../.. \
	   -I../../../include \
	   -I../../../stored \
	   -I../../../plugins/stored \
	   -I../../include \
	   -I../../compat/include
endif
LDLIBS = ../../lib/libbareos.dll \
         $(MINGW_LIB)/libpthreadGCE2.a \
	 $(MINGW_LIB)/libz.dll.a \
	 $(MINGW_LIB)/libfastlz.dll.a \
	 $(WINSOCKLIB) -lole32 -loleaut32 -luuid

all: autoxflate-sd.dll

autoxflate-sd.dll: DLL_USAGE = -DBUILDING_DLL
autoxflate-sd.dll: autoxflate-sd.o
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a autoxflate-sd.o $(LDLIBS) -o $@

clean:
	rm -f *.o

distclean:: clean
	rm -f autoxflate-sd.dll

# inference rules
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<
