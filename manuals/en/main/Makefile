LATEX=latex
DVI2PDF=dvipdf
MAKEINDEX=makeindex

INDEXES = main.console.ind \
	main.director.ind \
	main.general.ind \
	main.fd.ind \
	main.sd.ind \
	main.monitor.ind

all: pdf html

pdf: main.pdf

html: html/main.html

html/main.html: *.sty *.tex version.tex.tmp $(INDEXES) main.toc
	mkdir -p html
	htlatex main "" "" -d./html/

%.idx:
	$(LATEX) main.tex

%.ind: %.idx
	$(MAKEINDEX) $<

main.toc: *.tex
	$(LATEX) main.tex

version.tex.tmp: *.tex
	printf -- "\\\\def\\\\bareosBranch{`git rev-parse --abbrev-ref HEAD`}\n" > version.tex.tmp
	diff -q version.tex.tmp version.tex || cp -a version.tex.tmp version.tex

main.dvi: *.sty *.tex version.tex.tmp $(INDEXES) main.toc
	$(LATEX) main.tex
	$(LATEX) main.tex

main.pdf: main.dvi
	$(DVI2PDF) main.dvi main.pdf

clean:
	rm -f *.aux *.cdx *.ddx *.dvi *.fdx *.idx *.log *.out *.pdf *.sdx *.toc *.4ct *.4tc *.idv *.lg *.tmp *.xref html/* main.html main.css
