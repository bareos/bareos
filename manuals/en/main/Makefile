LATEX=latex
DVI2PDF=dvipdf
LATEX2HTML=latex2html

all: main.pdf

main.dvi: *.sty *.tex
	$(LATEX) main.tex
	$(LATEX) main.tex

main.pdf: main.dvi
	$(DVI2PDF) main.dvi main.pdf

clean:
	rm -f *.aux *.cdx *.ddx *.dvi *.fdx *.idx *.log *.out *.pdf *.sdx *.toc
