LATEX=latex
DVI2PDF=dvipdf
MAKEINDEX=makeindex
LATEX2HTML=latex2html

INDEXES = main.console.ind \
	main.director.ind \
	main.general.ind \
	main.fd.ind \
	main.sd.ind \
	main.monitor.ind

all: main.pdf


%.idx:
	$(LATEX) main.tex

%.ind: %.idx
	$(MAKEINDEX) $<

main.toc: *.tex
	$(LATEX) main.tex

main.dvi: *.sty *.tex $(INDEXES) main.toc
	$(LATEX) main.tex
	$(LATEX) main.tex

main.pdf: main.dvi
	$(DVI2PDF) main.dvi main.pdf

clean:
	rm -f *.aux *.cdx *.ddx *.dvi *.fdx *.idx *.log *.out *.pdf *.sdx *.toc
