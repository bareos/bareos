all_subdirs = filed

all:
	@for I in ${all_subdirs}; \
	  do (cd $$I; echo "==>Entering directory `pwd`"; \
		$(MAKE) WIN_VERSION=$(WIN_VERSION) DESTDIR=$(DESTDIR) $@ || (echo ""; echo ""; echo "  ====== Error in `pwd` ======"; \
		echo ""; echo ""; exit 1;)); \
	done

install:
	@for I in ${all_subdirs}; \
	  do (cd $$I; echo "==>Entering directory `pwd`"; \
		$(MAKE) WIN_VERSION=$(WIN_VERSION) DESTDIR=$(DESTDIR) $@ || (echo ""; echo ""; echo "  ====== Error in `pwd` ======"; \
		echo ""; echo ""; exit 1;)); \
	done

clean:
	@for I in ${all_subdirs}; \
	  do (cd $$I; echo "==>Entering directory `pwd`"; \
		$(MAKE) WIN_VERSION=$(WIN_VERSION) DESTDIR=$(DESTDIR) $@ || (echo ""; echo ""; echo "  ====== Error in `pwd` ======"; \
		echo ""; echo ""; exit 1;)); \
	done

distclean:
	@for I in ${all_subdirs}; \
	  do (cd $$I; echo "==>Entering directory `pwd`"; \
		$(MAKE) WIN_VERSION=$(WIN_VERSION) DESTDIR=$(DESTDIR) $@ || (echo ""; echo ""; echo "  ====== Error in `pwd` ======"; \
		echo ""; echo ""; exit 1;)); \
	done
