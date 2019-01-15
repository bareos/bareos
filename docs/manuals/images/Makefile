CONVERT=convert
COPY=cp
MKDIRCONV=mkdir -p conv/

ORIG_EPS = $(wildcard *.eps)
CONV_EPS = $(patsubst %.eps,conv/%.eps,$(ORIG_EPS))
ORIG_PNG = $(wildcard *.png)
CONV_PNG = $(patsubst %.eps,conv/%.png,$(ORIG_EPS))
COPY_PNG = $(patsubst %.png,conv/%.png,$(ORIG_PNG))
ORIG_DOT = $(wildcard *.dot)
COPY_DOT = $(patsubst %.dot,conv/%.dot,$(ORIG_DOT))
ORIG_SVG = $(wildcard *.svg)
COPY_SVG = $(patsubst %.svg,conv/%.svg,$(ORIG_SVG))

# conversion from fig to png does not work as expected,
# so we use the png as base.
#ORIG_FIG = $(wildcard *.fig)
#CONV_FIG_PNG = $(patsubst %.fig,conv/%.png,$(ORIG_FIG))
#CONV_FIG_EPS = $(patsubst %.fig,conv/%.eps,$(ORIG_FIG))


all: $(CONV_PNG) $(COPY_PNG) $(COPY_DOT) $(COPY_SVG) $(ORIG_PNG:conv/%.png=%.svg)  conv/bareos-full-logo.png conv/autoxflate-functionblocks.png conv/bareos-webui-jobs.png conv/blocksize-decisionchart.png conv/Conf-Diagram.png conv/dbconfig-1-enable.png conv/dbconfig-2-select-database-type.png conv/dbconfig-3-postgresql-password.png conv/different_time.png conv/flow.png conv/ndmp-backup.svg conv/ndmp-cfg.svg conv/passive-client-communication.png conv/univention-configuration-registry-settings.png conv/univention-ucs-overview-administration.png

# bareos-full-logo: png is of better quality than eps, therefore take that as source
conv/bareos-full-logo.eps: bareos-full-logo.png
	@$(MKDIRCONV)
	$(CONVERT) bareos-full-logo.png conv/bareos-full-logo.eps

conv/bareos-full-logo.png: bareos-full-logo.png
	@$(MKDIRCONV)
	$(COPY) bareos-full-logo.png conv/

conv/passive-client-communication.eps: passive-client-communication.png
	@$(MKDIRCONV)
	$(CONVERT) passive-client-communication.png conv/passive-client-communication.eps

conv/passive-client-communication.png: passive-client-communication.png
	@$(MKDIRCONV)
	$(COPY) passive-client-communication.png conv/

conv/autoxflate-functionblocks.png: autoxflate-functionblocks.png
	@$(MKDIRCONV)
	$(COPY) autoxflate-functionblocks.png conv/

conv/autoxflate-functionblocks.eps: autoxflate-functionblocks.png
	@$(MKDIRCONV)
	$(CONVERT) autoxflate-functionblocks.png conv/autoxflate-functionblocks.eps

conv/dbconfig-1-enable.png: dbconfig-1-enable.png
	@$(MKDIRCONV)
	$(COPY) dbconfig-1-enable.png conv/

conv/dbconfig-1-enable.eps: dbconfig-1-enable.png
	@$(MKDIRCONV)
	$(CONVERT) dbconfig-1-enable.png conv/dbconfig-1-enable.eps

conv/dbconfig-2-select-database-type.png: dbconfig-2-select-database-type.png
	@$(MKDIRCONV)
	$(COPY) dbconfig-2-select-database-type.png conv/

conv/dbconfig-2-select-database-type.eps: dbconfig-2-select-database-type.png
	@$(MKDIRCONV)
	$(CONVERT) dbconfig-2-select-database-type.png conv/dbconfig-2-select-database-type.eps

conv/dbconfig-3-postgresql-password.png: dbconfig-3-postgresql-password.png
	@$(MKDIRCONV)
	$(COPY) dbconfig-3-postgresql-password.png conv/

conv/dbconfig-3-postgresql-password.eps: dbconfig-3-postgresql-password.png
	@$(MKDIRCONV)
	$(CONVERT) dbconfig-3-postgresql-password.png conv/dbconfig-3-postgresql-password.eps

conv/duplicate-real.png: duplicate-real.svg
	@$(MKDIRCONV)
	$(CONVERT) duplicate-real.svg conv/duplicate-real.png

conv/univention-configuration-registry-settings.png: univention-configuration-registry-settings.png
	@$(MKDIRCONV)
	$(COPY) univention-configuration-registry-settings.png conv/

conv/univention-ucs-overview-administration.png: univention-ucs-overview-administration.png
	@$(MKDIRCONV)
	$(COPY) univention-ucs-overview-administration.png conv/


#conv/%.png: %.png
#	@$(MKDIRCONV)
	#$(CONVERT) $< conv/`basename $< .eps`.png
#	$(COPY) $< conv/$<

conv/%.eps: %.eps
	@$(MKDIRCONV)
	$(COPY) $< conv/$<

# SVG are copied and converted to png
conv/%.svg: %.svg
	@$(MKDIRCONV)
	$(CONVERT) $< conv/`basename $< .svg`.png
	$(COPY) $< conv/$<

conv/%.dot: %.dot
	@$(MKDIRCONV)
	dot -Tsvg -o conv/`basename $< .dot`.svg $<
	dot -Tpng -o conv/`basename $< .dot`.png $<
	$(COPY) $< conv/$<

conv/%.svg: %.png
	@$(MKDIRCONV)
	$(CONVERT) $< conv/`basename $< .png`.svg
	$(COPY) $< conv/$<
clean:
	rm -f conv/*
