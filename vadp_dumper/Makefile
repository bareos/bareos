CXX = g++
#CXXFLAGS = -g
CXXFLAGS = -O3
VIXDISKLIBH = -I/usr/lib/vmware-vix-disklib/include
#JANSONH = -I/opt/jansson/include
VIXDISK_LDFLAGS = -L/usr/lib/vmware-vix-disklib/lib64 -Wl,-rpath=/usr/lib/vmware-vix-disklib/lib64
VIXDISK_LIBS = -lvixDiskLib -lpthread
#JANSSON_LDFLAGS = -Wl,-rpath=/opt/jansson/lib -L/opt/jansson/lib
JANSSON_LIBS= -ljansson
SRCS = bareos_vadp_dumper.cpp cbuf.cpp copy_thread.cpp
OBJS = $(SRCS:.cpp=.o)

all: bareos_vadp_dumper

install: bareos_vadp_dumper
	mkdir -p $(DESTDIR)/usr/sbin/
	cp bareos_vadp_dumper $(DESTDIR)/usr/sbin/
	cp bareos_vadp_dumper_wrapper.sh $(DESTDIR)/usr/sbin/
	chmod 750 $(DESTDIR)/usr/sbin/bareos_vadp_dumper_wrapper.sh

bareos_vadp_dumper.o: bareos_vadp_dumper.cpp
	@echo "Compiling $<"
	#$(CXX) -c $(JANSONH) $(VIXDISKLIBH) $(CXXFLAGS) -o $@ $<
	$(CXX) -c $(VIXDISKLIBH) $(CXXFLAGS) -o $@ $<

bareos_vadp_dumper: $(OBJS)
	#$(CXX) -o $@ $(OBJS) $(JANSSON_LDFLAGS) $(JANSSON_LIBS) $(VIXDISK_LDFLAGS) $(VIXDISK_LIBS)
	$(CXX) -o $@ $(OBJS) $(JANSSON_LIBS) $(VIXDISK_LDFLAGS) $(VIXDISK_LIBS)

clean:
	rm -f bareos_vadp_dumper $(OBJS)

# inference rules
.cpp.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<
