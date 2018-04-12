
SUBDIRS = manuals/en/main/ manuals/en/developers/
.PHONY: $(SUBDIRS)

all:   TARGET=
all:   $(SUBDIRS)

check: TARGET=check
check: $(SUBDIRS)

clean: TARGET=clean
clean: $(SUBDIRS)

html:  TARGET=html
html:  $(SUBDIRS)

pdf:   TARGET=pdf
pdf:   $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(TARGET)

# %: $(SUBDIRS)
# 	@echo $<
# 	@echo $^
# 	@echo $@
# 	@echo $?
# 	@echo "$< $@" >>/tmp/make.log
# 	@if [ $@ != Makefile ]; then $(MAKE) -C $< $@; else $(MAKE) -C $<; fi
