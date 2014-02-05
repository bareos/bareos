CONVERT=convert
COPY=cp

ORIG_EPS = $(wildcard *.eps)
CONV_EPS = $(patsubst %.eps,conv/%.eps,$(ORIG_EPS))
CONV_PNG = $(patsubst %.eps,conv/%.png,$(ORIG_EPS))

all: $(CONV_EPS) $(CONV_PNG) conv/bareos-full-logo.png conv/bareos-full-logo.eps

conv/%.png: %.eps
	$(CONVERT) $< conv/`basename $< .eps`.png

# bareos-full-logo: png is of better quality than eps, therefore take that as source
conv/bareos-full-logo.eps: bareos-full-logo.png
	$(CONVERT) bareos-full-logo.png conv/bareos-full-logo.eps

conv/bareos-full-logo.png: bareos-full-logo.png
	$(COPY) bareos-full-logo.png conv/

conv/%.eps: %.eps
	$(COPY) $< conv/$<

clean:
	rm -f conv/*
