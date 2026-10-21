LATEX ?= xelatex
FLAGS = -interaction=nonstopmode -halt-on-error

all: report.pdf

report.pdf: report.tex preamble.tex titlepage.tex $(wildcard inc/* ../src/*.c ../src/*.h) ../Makefile
	$(LATEX) $(FLAGS) report.tex
	$(LATEX) $(FLAGS) report.tex

clean:
	rm -f *.aux *.log *.out *.lol *.toc *.fls *.fdb_latexmk *.synctex.gz

.PHONY: all clean
