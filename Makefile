
subdirs = manuals/en/main/ manuals/en/developers/

MAKE=make

all:

depend:
	@for I in ${subdirs}; \
		do (cd $$I; echo "==>Entering directory `pwd`"; $(MAKE) $@ || exit 1); done

%:
	@for I in ${subdirs}; \
		do (cd $$I; echo "==>Entering directory `pwd`"; $(MAKE) $@ || exit 1); done
