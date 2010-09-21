
DIRS = \
build-tools \
libdroplet \
tools \
tests

ALLDIRS = $(DIRS) 

PREFIX = /usr/local

all:
	@for i in `echo $(DIRS)`; do \
	echo Making all in $$i...; \
	if [ -d $$i ] ;\
	then \
	cd $$i; \
	$(MAKE); \
	cd ..; \
	fi \
	done

prod:
	@for i in `echo $(DIRS)`; do \
	echo Making all in $$i...; \
	if [ -d $$i ] ;\
	then \
	cd $$i; \
	$(MAKE) prod; \
	cd ..; \
	fi \
	done

prod32:
	@for i in `echo $(DIRS)`; do \
	echo Making all in $$i...; \
	if [ -d $$i ] ;\
	then \
	cd $$i; \
	$(MAKE) prod32; \
	cd ..; \
	fi \
	done

debug:
	@for i in `echo $(DIRS)`; do \
	echo Making all in $$i...; \
	if [ -d $$i ] ;\
	then \
	cd $$i; \
	$(MAKE) debug; \
	cd ..; \
	fi \
	done

valgrind:
	@for i in `echo $(DIRS)`; do \
	echo Making all in $$i...; \
	if [ -d $$i ] ;\
	then \
	cd $$i; \
	$(MAKE) debug; \
	cd ..; \
	fi \
	done

install:
	@for i in `echo $(DIRS)`; do \
	echo Making install in $$i...; \
	if [ -d $$i ] ;\
	then \
	cd $$i; \
	$(MAKE) install PREFIX=$(PREFIX); \
	cd ..; \
	fi \
	done

clean:
	@for i in `echo $(ALLDIRS)`; do \
	echo Cleaning $$i...; \
	if [ -d $$i ] ;\
	then \
	cd $$i; \
	$(MAKE) clean; \
	cd ..; \
	fi \
	done;

fproto:
	@for i in `echo $(ALLDIRS)`; do \
	echo Fprototyping $$i...; \
	if [ -d $$i ] ;\
	then \
	cd $$i; \
	$(MAKE) fproto; \
	cd ..; \
	fi \
	done;

