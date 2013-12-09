VPATH = .:../../findlib

include ../Makefile.inc

INCLUDES = -I../.. \
	   -I../../include \
	   -I../include \
	   -I../../findlib \
	   -I../compat/include
LDLIBS = ../lib/libbareos.dll \
	 $(MINGW_LIB)/libpthreadGCE2.a \
	 $(WINSOCKLIB) -lole32 -loleaut32 -luuid

LIBBACFIND_SRCS = acl.c attribs.c bfile.c create_file.c \
		  drivetype.c enable_priv.c find_one.c \
		  find.c fstype.c match.c mkpath.c \
		  shadowing.c win32.c xattr.c
LIBBACFIND_OBJS = $(LIBBACFIND_SRCS:.c=.o)

all: libbareosfind.dll

bareos$(WIN_VERSION).def: $(LIBBACFIND_OBJS) make_def
	./make_def $(WIN_VERSION) $(LIBBACFIND_OBJS) > $@

libbareosfind.dll: DLL_USAGE = -DBUILDING_DLL
libbareosfind.dll: $(LIBBACFIND_OBJS) \
		bareos$(WIN_VERSION).def
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a $(LIBBAC_OBJS) $(LIBBACCFG_OBJS) $(LIBBACFIND_OBJS) $(COMPAT_OBJS) bareos$(WIN_VERSION).def $(LDLIBS) -o $@

clean:
	rm -f *.o bareos$(WIN_VERSION).def

distclean: clean
	rm -f libbareosfind.dll

# inference rules
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<
