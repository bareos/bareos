LATEX=latex
DVI2PDF=dvipdf
MAKEINDEX=makeindex

INDEXES = bareos-manual-main-reference.console.ind \
	bareos-manual-main-reference.director.ind \
	bareos-manual-main-reference.general.ind \
	bareos-manual-main-reference.fd.ind \
	bareos-manual-main-reference.sd.ind \
	bareos-manual-main-reference.monitor.ind

all: pdf html

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

bareos-manual-main-reference.pdf: bareos-manual-main-reference.dvi
	$(DVI2PDF) bareos-manual-main-reference.dvi bareos-manual-main-reference.pdf

clean:
	rm -f *.aux *.dvi *.idx *.log *.out *.pdf *.toc *.4ct *.4tc *.idv *.lg *.tmp *.xref html/* bareos-manual-main-reference.html bareos-manual-main-reference.css
