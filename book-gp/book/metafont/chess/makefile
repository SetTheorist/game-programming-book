# File:		Makefile
# Purpose:	automatic generation of documentation and installation 
#		of the fonts (at least on UNIX)
# Author:	Piet Tutelaers (internet: rcpt@urc.tue.nl)
# Version:	Febr. 1992
#

SHELL = /bin/sh

#
# Next definitions should point to the place where the .sty files are
# stored (INPUTS) and where the .tfm files are (FONTS)
#
INPUTS = /usr/local/lib/tex30/inputs
FONTS  = /usr/local/lib/tex30/fonts:/usr/local/lib/tex30/PSfonts:

#
# Next definitions should point to the place where the .sty files are
# installed (DESTINPUTS), the .tfm files (DESTFONTS) and the 300pk
# files (DESTPK)
DESTTEX=/usr/local/lib/tex30
DESTINPUTS= $(DESTTEX)/inputs
DESTFONTS = $(DESTTEX)/fonts
DESTPK    = $(DESTTEX)/pkfonts

MFCHESSBASE= mf/chesspieces.mf mf/chessbase.mf
CHESSMACROS= chess20.300pk chessf10.300pk inputs/chess.sty

.SUFFIXES: .dvi .ps .300gf .300pk

all: fonts doc 

doc: TUGboat.ps Symbols.ps tal.ps Kasparov.ps
	@echo "You can send the PostScript files to the printer"

fonts: chess10.300pk chess20.300pk chess30.300pk chessf10.300pk 

TUGboat.dvi:	doc/TUGboat.ltx doc/TUGgame.ltx $(CHESSMACROS)
	 (TEXINPUTS=.:doc:inputs:$(INPUTS); export TEXINPUTS; \
	  TEXFONTS=fonts:$(FONTS); export TEXFONTS; \
	  latex "\batchmode\input TUGboat.ltx" \
	  latex "\batchmode\input TUGboat.ltx")
	
Symbols.dvi:	doc/Symbols.ltx $(CHESSMACROS)
	 (TEXINPUTS=.:doc:inputs:$(INPUTS); export TEXINPUTS; \
	  TEXFONTS=fonts:$(FONTS); export TEXFONTS; \
	  latex "\batchmode\input Symbols.ltx")

tal.dvi:	doc/tal.ltx $(CHESSMACROS)
	 (TEXINPUTS=.:doc:inputs:$(INPUTS); export TEXINPUTS; \
	  TEXFONTS=fonts:$(FONTS); export TEXFONTS; \
	  latex "\batchmode\input tal.ltx")

Kasparov.dvi:	doc/Kasparov.ltx $(CHESSMACROS)
	 (TEXINPUTS=.:doc:inputs:$(INPUTS); export TEXINPUTS; \
	  TEXFONTS=fonts:$(FONTS); export TEXFONTS; \
	  latex "\batchmode\input Kasparov.ltx")

.dvi.ps:
	(TEXINPUTS=.:doc:inputs:$(INPUTS); export TEXINPUTS; \
	 dvips -o $*.ps $<)

.300gf.300pk:
	gftopk $<

chess10.tfm chess10.300gf:	mf/chess10.mf $(MFCHESSBASE)
	(MFINPUTS=mf; export MFINPUTS; \
	 mf "\mode:=localfont; input chess10")

chess20.tfm chess20.300gf:	mf/chess20.mf $(MFCHESSBASE)
	(MFINPUTS=mf; export MFINPUTS; \
	 mf "\mode:=localfont; input chess20")

chess30.tfm chess30.300gf:	mf/chess30.mf $(MFCHESSBASE)
	(MFINPUTS=mf; export MFINPUTS; \
	 mf "\mode:=localfont; input chess30")

chessf10.tfm chessf10.300gf:	mf/chessf10.mf $(MFCHESSBASE)
	(MFINPUTS=mf; export MFINPUTS; \
	 mf "\mode:=localfont; input chessf10")

install: fonts inputs/chess.sty
	install -m 644 chess20.300pk $(DESTPK)
	install -m 644 chessf10.300pk $(DESTPK)
	install -m 644 chess20.tfm $(DESTFONTS)
	install -m 644 chessf10.tfm $(DESTFONTS)
	(cd inputs; install -c -m 644 chess.sty $(DESTINPUTS))

clean:
	rm -f *gf *pk *.ps *.dvi *.aux *.log *.tfm

SOURCES = README \
	  CHANGES12 \
	  CopyRight \
	  INSTALLATION \
	  Makefile \
	  doc/TUGboat.ltx \
	  doc/TUGgame.ltx \
	  doc/boards.ltx \
	  doc/tal.ltx \
	  doc/tal.tex \
	  doc/Kasparov.ltx \
	  doc/Symbols.ltx \
	  doc/Symbols.tex \
	  doc/dutch-tt.tex \
	  doc/dutch-tt.ltx \
	  doc/SchaakMaatje.ltx \
	  inputs/chess.sty \
	  mf/README \
	  mf/chessf10.mf \
	  mf/chessdiag.mf \
	  mf/chess20.mf \
	  mf/chessbase.mf \
	  mf/chesspieces.mf \
	  mf/chess30.mf \
	  mf/chessdiag.Xmf \
	  mf/chess10.mf

printsources:
	enscript -pchess12.ps $(SOURCES)
	@echo "You can send the chess12.ps to the printer"

# DO NOT DELETE THIS LINE -- make depend depends on it.
TUGboat.dvi:	chess10.300pk chess20.300pk chessf10.300pk
