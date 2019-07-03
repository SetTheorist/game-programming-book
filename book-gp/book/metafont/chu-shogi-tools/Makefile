


all: test.pdf jet-chu-pieces.pdf

test.pdf: test.tex piece.sty
	latex test.tex
	dvipdfmx -r1200 test.dvi

jet-chu-pieces.pdf: jet-chu-pieces.tex piece.sty
	latex jet-chu-pieces.tex
	dvipdfmx -r1200 jet-chu-pieces.dvi


# mf shg10.mf
# gftopk shg10.2602gf
# gftodvi shg10.2602gf
# dvipdfm shg10.dvi
