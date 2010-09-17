
MAKE = make

DIRS = 

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

prod: all
prod32: all
debug: all
valgrind: all

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
	$(MAKE) clean; \
	cd ..; \
	fi \
	done;

