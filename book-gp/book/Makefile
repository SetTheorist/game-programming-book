


book: book.ind book.bbl book.dvi book.pdf

all: book minishogi chapter-1-tictactoe

touch:
	touch book.tex *.rs

minishogi: minishogi.rs
	rustc minishogi.rs

chapter-1-tictactoe: chapter-1-tictactoe.rs
	rustc chapter-1-tictactoe.rs

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


