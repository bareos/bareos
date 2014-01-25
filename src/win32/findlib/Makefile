VPATH = .:../../findlib

include ../Makefile.inc

INCLUDES = -I../.. \
	   -I../../include \
	   -I../include \
	   -I../../findlib \
	   -I../compat/include
LDLIBS = ../lib/libbareos.dll \
	 $(MINGW_LIB)/libpthreadGCE2.a \
	 $(WINSOCKLIB)

LIBBAREOSFIND_SRCS = acl.c attribs.c bfile.c create_file.c \
                     drivetype.c enable_priv.c find_one.c \
                     find.c fstype.c match.c mkpath.c \
                     shadowing.c win32.c xattr.c
LIBBAREOSFIND_OBJS = $(LIBBAREOSFIND_SRCS:.c=.o)

DYNAMIC_OBJS = $(LIBBAREOSFIND_OBJS)

all: libbareosfind.dll

bareos$(WIN_VERSION).def: $(DYNAMIC_OBJS) make_def
	./make_def $(WIN_VERSION) $(DYNAMIC_OBJS) > $@

libbareosfind.dll: DLL_USAGE = -DBUILDING_DLL
libbareosfind.dll: $(DYNAMIC_OBJS) \
		bareos$(WIN_VERSION).def
	$(CXX) $(LDFLAGS_DLL) -Wl,--out-implib,$@.a $(DYNAMIC_OBJS) bareos$(WIN_VERSION).def $(LDLIBS) -o $@

clean:
	rm -f *.o bareos$(WIN_VERSION).def

distclean: clean
	rm -f libbareosfind.dll libbareosfind.dll.a

# inference rules
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<
