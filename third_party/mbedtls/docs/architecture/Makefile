PANDOC = pandoc

default: all

all_markdown = \
	       mbed-crypto-storage-specification.md \
	       testing/driver-interface-test-strategy.md \
	       testing/invasive-testing.md \
	       testing/test-framework.md \
	       # This line is intentionally left blank

html: $(all_markdown:.md=.html)
pdf: $(all_markdown:.md=.pdf)
all: html pdf

.SUFFIXES:
.SUFFIXES: .md .html .pdf

.md.html:
	$(PANDOC) -o $@ $<
.md.pdf:
	$(PANDOC) -o $@ $<

clean:
	rm -f *.html *.pdf
	rm -f testing/*.html testing/*.pdf
