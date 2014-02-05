CONVERT=convert
COPY=cp
MKDIRCONV=mkdir -p conv/

ORIG_EPS = $(wildcard *.eps)
CONV_EPS = $(patsubst %.eps,conv/%.eps,$(ORIG_EPS))
CONV_PNG = $(patsubst %.eps,conv/%.png,$(ORIG_EPS))

all: $(CONV_EPS) $(CONV_PNG) conv/bareos-full-logo.png conv/bareos-full-logo.eps

# bareos-full-logo: png is of better quality than eps, therefore take that as source
conv/bareos-full-logo.eps: bareos-full-logo.png
	@$(MKDIRCONV)
	$(CONVERT) bareos-full-logo.png conv/bareos-full-logo.eps

conv/bareos-full-logo.png: bareos-full-logo.png
	@$(MKDIRCONV)
	$(COPY) bareos-full-logo.png conv/

conv/%.png: %.eps
	@$(MKDIRCONV)
	$(CONVERT) $< conv/`basename $< .eps`.png

conv/%.eps: %.eps
	@$(MKDIRCONV)
	$(COPY) $< conv/$<

clean:
	rm -f conv/*
