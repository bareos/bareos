
PANDOC=pandoc

FILES = generaldevel.md \
      pluginAPI.md \
      platformsupport.md \
      daemonprotocol.md \
      director.md \
      file.md \
      storage.md \
      catalog.md \
      mediaformat.md \
      porting.md \
      gui-interface.md \
      tls-techdoc.md \
      regression.md \
      mempool.md \
      netprotocol.md \
      smartall.md \
      fdl.md

all: html

depend:
	@for I in ${subdirs}; \
		do (cd $$I; echo "==>Entering directory `pwd`"; $(MAKE) || exit 1); done

html: html/bareos-developer-guide.html

html/bareos-developer-guide.html: depend bareos-developer-guide.css footer.html $(FILES)
	mkdir -p html
	$(PANDOC) -s -S --toc -c bareos-developer-guide.css -B footer.html -A footer.html  $(FILES) -o html/bareos-developer-guide.html
	cp -a bareos-developer-guide.css html/

check: html
	true

clean:
	rm -rf html/
	@for I in ${subdirs}; \
		do (cd $$I; echo "==>Entering directory `pwd`"; $(MAKE) clean || exit 1); done

