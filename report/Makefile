all: report.pdf

report.pdf: report.tex preamble.tex titlepage.tex $(wildcard inc/*) $(shell find ../src -name '*.c' -o -name '*.h')
	xelatex -interaction=nonstopmode -halt-on-error report.tex
	xelatex -interaction=nonstopmode -halt-on-error report.tex

clean:
	rm -f *.aux *.log *.out *.lol *.toc *.fls *.fdb_latexmk *.synctex.gz

.PHONY: all clean
