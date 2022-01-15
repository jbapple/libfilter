.PHONY: all clean

all: arxiv.zip

%.pdf: %.eps Makefile
	epstopdf $<

%.pdf: %.tex Makefile
	latexmk -pdf $<

arxiv.zip: taffy.tex taffy.bbl sbbf-diagram.pdf quotienting-diagram.pdf pagh-1-diagram.pdf pagh-diagram.pdf mpcf-diagram.pdf mtcf-state-transition.pdf bits-per-item.pdf space.pdf ideal-bits-per-item.pdf insert-cumulative.pdf arm-insert-cumulative.pdf lookup-both.pdf arm-lookup-both.pdf tcf-key-split.pdf usenix-2020-09.sty Makefile
	rm -f arxiv.zip
	zip arxiv $^
	zip -d arxiv.zip Makefile
