
PANDOC=pandoc

FILES = generaldevel.md \
      messages.md \
      pluginAPI.md \
      platformsupport.md \
      daemonprotocol.md \
      file.md \
      storage.md \
      catalog.md \
      mediaformat.md \
      porting.md \
      api.md \
      tls-techdoc.md \
      regression.md \
      mempool.md \
      netprotocol.md \
      directorConsole.md \
      smartall.md \
      fdl.md

all: html

pdf:
	# skipped, pdf will not be generated

# depend:
# 	@for I in ${subdirs}; \
# 		do (cd $$I; echo "==>Entering directory `pwd`"; $(MAKE) || exit 1); done

html: html/bareos-developer-guide.html

html/bareos-developer-guide.html: bareos-developer-guide.css footer.html $(FILES)
	mkdir -p html
	$(PANDOC) --standalone --smart --toc --css=bareos-developer-guide.css --include-after-body=footer.html $(FILES) --output=html/bareos-developer-guide.html
	cp -a bareos-developer-guide.css html/

check: html
	true

clean:
	rm -rf html/
# 	@for I in ${subdirs}; \
# 		do (cd $$I; echo "==>Entering directory `pwd`"; $(MAKE) clean || exit 1); done
