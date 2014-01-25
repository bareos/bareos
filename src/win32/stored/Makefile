VPATH = .:../../stored:../generic

include ../Makefile.inc

INCLUDES = -I. \
	   -I../.. \
	   -I../../include \
	   -I../../stored \
	   -I../include \
	   -I../compat/include
LDLIBS = ../lib/libbareos.a \
	 ../lib/libbareos.dll \
         $(MINGW_LIB)/libpthreadGCE2.a \
	 $(WINSOCKLIB) -lole32 -loleaut32 -luuid -lcomctl32

LDLIBS_FIND = ../lib/libbareos.a \
	      ../lib/libbareos.dll \
	      ../findlib/libbareosfind.dll \
	      $(MINGW_LIB)/libpthreadGCE2.a \
	      $(WINSOCKLIB) -lole32 -loleaut32 -luuid -lcomctl32

SVRSRCS = acquire.c ansi_label.c append.c askdir.c authenticate.c \
	  autochanger.c block.c bsr.c butil.c crc32.c dev.c device.c \
	  dir_cmd.c ebcdic.c fd_cmds.c job.c label.c lock.c mac.c mount.c \
	  ndmp_tape.c read_record.c read.c record.c reserve.c scan.c sd_cmds.c \
	  sd_plugins.c spool.c status.c stored_conf.c stored.c vol_mgr.c \
	  wait.c win32_tape_device.c win32_file_device.c service.c main.c
SVROBJS = $(SVRSRCS:.c=.o)

TAPESRCS = acquire.c ansi_label.c autochanger.c block.c btape.c bsr.c \
	   butil.c crc32.c dev.c device.c ebcdic.c label.c lock.c mount.c \
	   read_record.c record.c reserve.c scan.c sd_plugins.c spool.c \
	   stored_conf.c vol_mgr.c wait.c win32_tape_device.c win32_file_device.c
TAPEOBJS = $(TAPESRCS:.c=.o)

BLSSRCS = acquire.c ansi_label.c autochanger.c block.c bls.c bsr.c \
	  butil.c crc32.c dev.c device.c ebcdic.c label.c lock.c mount.c \
	  read_record.c record.c reserve.c scan.c sd_plugins.c spool.c \
	  stored_conf.c vol_mgr.c wait.c win32_tape_device.c win32_file_device.c
BLSOBJS = $(BLSSRCS:.c=.o)

BEXTSRCS = acquire.c ansi_label.c autochanger.c bextract.c block.c bsr.c \
	   butil.c crc32.c dev.c device.c ebcdic.c label.c lock.c mount.c \
	   read_record.c record.c reserve.c scan.c sd_plugins.c spool.c \
	   stored_conf.c vol_mgr.c wait.c win32_tape_device.c win32_file_device.c
BEXTOBJS = $(BEXTSRCS:.c=.o)

WINDRESSRCS = storedres.rc
WINDRESOBJS = $(WINDRESSRCS:.rc=.o)

all: bareos-sd.exe btape.exe bls.exe bextract.exe

bareos-sd.exe: DLL_USAGE = -DUSING_DLL
bareos-sd.exe: $(SVROBJS) $(WINDRESOBJS)
	$(CXX) $(LDFLAGS_WINAPP) $(SVROBJS) $(WINDRESOBJS) $(LDLIBS) -o $@

btape.exe: DLL_USAGE = -DUSING_DLL
btape.exe: $(TAPEOBJS) $(WINDRESOBJS)
	$(CXX) $(LDFLAGS_CONS) $(TAPEOBJS) $(WINDRESOBJS) $(LDLIBS) -o $@

bls.exe: DLL_USAGE = -DUSING_DLL
bls.exe: $(BLSOBJS) $(WINDRESOBJS)
	$(CXX) $(LDFLAGS_CONS) $(BLSOBJS) $(WINDRESOBJS) $(LDLIBS_FIND) -o $@

bextract.exe: DLL_USAGE = -DUSING_DLL
bextract.exe: $(BEXTOBJS) $(WINDRESOBJS)
	$(CXX) $(LDFLAGS_CONS) $(BEXTOBJS) $(WINDRESOBJS) $(LDLIBS_FIND) -o $@

clean:
	rm -f *.o

distclean:: clean
	rm -f bareos-sd.exe btape.exe bls.exe bextract.exe

# inference rules
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<

%.o : %.rc
	@echo "Ressource Compiling $^"
	$(WINDRES) $^ -o $@
