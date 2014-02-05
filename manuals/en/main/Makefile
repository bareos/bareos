
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

LOG = bareos-manual-main-reference.log

all: pdf html

depend:
	@for I in ${subdirs}; \
		do (cd $$I; echo "==>Entering directory `pwd`"; $(MAKE) || exit 1); done

pdf: bareos-manual-main-reference.pdf

html: html/bareos-manual-main-reference.html

html/bareos-manual-main-reference.html: *.sty *.tex $(INDEXES) bareos-manual-main-reference.toc
	mkdir -p html
	htlatex bareos-manual-main-reference "bareos-manual-main-reference.htlatex.cfg" "" -d./html/

%.idx:
	$(LATEX) bareos-manual-main-reference.tex

%.ind: %.idx
	$(MAKEINDEX) $<

bareos-manual-main-reference.toc: *.tex
	$(LATEX) bareos-manual-main-reference.tex

bareos-manual-main-reference.dvi: *.sty *.tex $(INDEXES) bareos-manual-main-reference.toc
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
	rm -f *.aux *.dvi *.idx *.log *.out *.pdf *.toc *.4ct *.4tc *.idv *.lg *.tmp *.xref html/* bareos-manual-main-reference.html bareos-manual-main-reference.css
	@for I in ${subdirs}; \
		do (cd $$I; echo "==>Entering directory `pwd`"; $(MAKE) clean || exit 1); done
