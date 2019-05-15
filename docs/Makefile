
SUBDIRS = manuals/main/
.PHONY: $(SUBDIRS)

all:   TARGET=
all:   $(SUBDIRS)

clean: TARGET=clean
clean: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@ $(TARGET)

# %: $(SUBDIRS)
# 	@echo $<
# 	@echo $^
# 	@echo $@
# 	@echo $?
# 	@echo "$< $@" >>/tmp/make.log
# 	@if [ $@ != Makefile ]; then $(MAKE) -C $< $@; else $(MAKE) -C $<; fi
