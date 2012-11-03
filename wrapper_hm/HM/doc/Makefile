SHELL=/bin/bash

DOCNUM:=software-manual

LATEX:=$(shell which xelatex || which pdflatex || which latex)
BIBTOOL:=$(shell which bibtool || echo \\\# skipping bibtool )
BIBTOOL_DB=~/mpeg/doc/bib/jctvc.bib

all: $(DOCNUM).pdf

%.aux: %.tex
	$(LATEX) $(LATEXFLAGS) $<

%.bib: %.tex %.aux
	$(BIBTOOL) -q -s -d -r <(echo check.double.delete = ON) -i $(BIBTOOL_DB) -x $(*F).aux -o $@
	touch $@

%.bbl: %.tex %.aux %.bib
	-bibtex $(*F)

ifneq ($(LATEX),latex)
%.pdf: %.tex %.bbl
	$(LATEX) $(LATEXFLAGS) $<
	$(LATEX) $(LATEXFLAGS) $<
endif

%.pdf: %.dvi
	dvipdfm $*.dvi

%.dvi: %.tex %.bbl
	$(LATEX) $(LATEXFLAGS) $<
	$(LATEX) $(LATEXFLAGS) $<

$(DOCNUM)-diff_%.pdf: LATEXFLAGS=--interaction=batchmode
$(DOCNUM)-diff_%.tex:
	-latexdiff-vc --exclude-safecmd=textbf --append-context1cmd=subfloat --append-context2cmd=multicolumn --encoding=utf8 --force --git -r $* $(DOCNUM).tex
