CONVERT=convert
COPY=cp
MKDIRCONV=mkdir -p conv/

ORIG_EPS = $(wildcard *.eps)
CONV_EPS = $(patsubst %.eps,conv/%.eps,$(ORIG_EPS))
CONV_PNG = $(patsubst %.eps,conv/%.png,$(ORIG_EPS))

# conversion from fig to png does not work as expected,
# so we use the png as base.
#ORIG_FIG = $(wildcard *.fig)
#CONV_FIG_PNG = $(patsubst %.fig,conv/%.png,$(ORIG_FIG))
#CONV_FIG_EPS = $(patsubst %.fig,conv/%.eps,$(ORIG_FIG))


all: $(CONV_EPS) $(CONV_PNG) conv/bareos-full-logo.png conv/bareos-full-logo.eps conv/autoxflate-functionblocks.png conv/autoxflate-functionblocks.eps conv/dbconfig-1-enable.png conv/dbconfig-1-enable.eps conv/dbconfig-2-select-database-type.png conv/dbconfig-2-select-database-type.eps conv/dbconfig-3-postgresql-password.png conv/dbconfig-3-postgresql-password.eps


# bareos-full-logo: png is of better quality than eps, therefore take that as source
conv/bareos-full-logo.eps: bareos-full-logo.png
	@$(MKDIRCONV)
	$(CONVERT) bareos-full-logo.png conv/bareos-full-logo.eps

conv/bareos-full-logo.png: bareos-full-logo.png
	@$(MKDIRCONV)
	$(COPY) bareos-full-logo.png conv/

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


conv/%.png: %.eps
	@$(MKDIRCONV)
	$(CONVERT) $< conv/`basename $< .eps`.png

conv/%.eps: %.eps
	@$(MKDIRCONV)
	$(COPY) $< conv/$<

clean:
	rm -f conv/*
