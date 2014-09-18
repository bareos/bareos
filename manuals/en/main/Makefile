
subdirs = ../../images/

MAKE=make
LATEX=latex
DVI2PDF=dvipdf
PDFLATEX=pdflatex
MAKEINDEX=makeindex
CONVERT=convert

INDEXES = bareos-manual-main-reference.console.ind \
	bareos-manual-main-reference.director.ind \
	bareos-manual-main-reference.general.ind \
	bareos-manual-main-reference.fd.ind \
	bareos-manual-main-reference.sd.ind \
	bareos-manual-main-reference.monitor.ind
HTINDEXES = bareos-manual-main-reference.console.htind \
	bareos-manual-main-reference.director.htind \
	bareos-manual-main-reference.general.htind \
	bareos-manual-main-reference.fd.htind \
	bareos-manual-main-reference.sd.htind \
	bareos-manual-main-reference.monitor.htind


LOG = bareos-manual-main-reference.log

all: pdf html

depend:
	@for I in ${subdirs}; \
		do (cd $$I; echo "==>Entering directory `pwd`"; $(MAKE) || exit 1); done

pdf: bareos-manual-main-reference.pdf

html: html/bareos-manual-main-reference.html

html/bareos-manual-main-reference.html: depend *.sty *.tex $(HTINDEXES) bareos-manual-main-reference.toc
	mkdir -p html
	htlatex bareos-manual-main-reference "bareos-manual-main-reference.htlatex.cfg" "" -d./html/

%.idx:
	$(LATEX) bareos-manual-main-reference.tex

%.ind: %.idx
	$(MAKEINDEX) $<

%.htidx:
	htlatex bareos-manual-main-reference "bareos-manual-main-reference.htlatex.cfg" "" -d./html/

%.4dx: %.htidx
	name=`basename $< .htidx`; tex "\def\filename{{$$name}{htidx}{4dx}{htind}} \input idxmake.4ht"

%.htind: %.4dx
	name=`basename $< .4dx`; $(MAKEINDEX) -o $${name}.htind $<

bareos-manual-main-reference.toc: *.tex
	$(LATEX) bareos-manual-main-reference.tex

bareos-manual-main-reference.dvi: depend *.sty *.tex $(INDEXES) bareos-manual-main-reference.toc
	$(LATEX) bareos-manual-main-reference.tex
	$(LATEX) bareos-manual-main-reference.tex

#bareos-manual-main-reference.pdf: bareos-manual-main-reference.dvi
#	$(DVI2PDF) bareos-manual-main-reference.dvi bareos-manual-main-reference.pdf

bareos-manual-main-reference.pdf: depend *.sty *.tex $(INDEXES) bareos-manual-main-reference.toc
	$(PDFLATEX) bareos-manual-main-reference.tex
	$(PDFLATEX) bareos-manual-main-reference.tex

check: bareos-manual-main-reference.pdf
	@if ! grep "LaTeX Warning: Hyper reference .* undefined on input\|LaTeX Warning: There were undefined references." $(LOG); then true; else false; fi

clean:
	rm -f *.4ct *.4dx *.4ix *.4tc *.aux *.dvi *.htidx *.htind *.idx *.log *.out *.pdf *.toc *.idv *.lg *.tmp *.xref html/* bareos-manual-main-reference.html bareos-manual-main-reference.css
	@for I in ${subdirs}; \
		do (cd $$I; echo "==>Entering directory `pwd`"; $(MAKE) clean || exit 1); done
