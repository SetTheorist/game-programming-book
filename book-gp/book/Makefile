


all: book.ind book.bbl book.dvi book.pdf

touch:
	touch book.tex

book.pdf: book.dvi
	dvipdfm book.dvi

book.bbl: book.aux
	bibtex book

book.ind: book.idx
	makeindex book.idx

book.idx: book.tex
	latex book.tex

book.dvi: book.tex
	latex book.tex
	latex book.tex


