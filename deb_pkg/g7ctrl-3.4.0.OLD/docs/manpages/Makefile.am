CLEANFILES = g7ctrl.fo g7sh.fo *~
man_MANS = g7ctrl.1 g7sh.1
EXTRA_DIST = g7ctrl.1 g7ctrl.1.xml g7ctrl.1.html g7ctrl.1.pdf g7sh.1 g7sh.1.xml g7sh.1.html g7sh.1.pdf

if have_xsltproc

g7ctrl.1.html: g7ctrl.1.xml
	${XSLTPROC} --output g7ctrl.1.html ${DOCBOOK_XSL}/html/docbook.xsl $<

g7ctrl.1: g7ctrl.1.xml
	${XSLTPROC} ${DOCBOOK_XSL}/manpages/docbook.xsl $<

g7sh.1.html: g7sh.1.xml
	${XSLTPROC} --output g7sh.1.html ${DOCBOOK_XSL}/html/docbook.xsl $<

g7sh.1: g7sh.1.xml
	${XSLTPROC} ${DOCBOOK_XSL}/manpages/docbook.xsl $<

if have_javafop

g7ctrl.1.pdf: g7ctrl.1.xml
	${XSLTPROC} --nonet --output g7ctrl.fo \
		--stringparam paper.type A4 \
		--stringparam body.start.indent 0pt \
		--stringparam title.margin.left 0pt \
		--stringparam variablelist.as.blocks 1 \
		${DOCBOOK_XSL}/fo/docbook.xsl $<
	fop g7ctrl.fo $@

g7sh.1.pdf: g7sh.1.xml
	${XSLTPROC} --nonet --output g7sh.fo \
		--stringparam paper.type A4 \
		--stringparam body.start.indent 0pt \
		--stringparam title.margin.left 0pt \
		--stringparam variablelist.as.blocks 1 \
		${DOCBOOK_XSL}/fo/docbook.xsl $<
	fop g7sh.fo $@

all: g7ctrl.1 g7ctrl.1.html g7ctrl.1.pdf g7sh.1 g7sh.1.html g7sh.1.pdf

else

all: g7ctrl.1 g7ctrl.1.html g7sh.1 g7sh.1.html

endif

else

all:

g7ctrl.1:
	touch $@
g7ctrl.1.pdf:
	touch $@
g7ctrl.1.html:
	touch $@
g7sh.1:
	touch $@
g7sh.1.pdf:
	touch $@
g7sh.1.html:
	touch $@
	
	
endif

stdpackagedoc_DATA = g7ctrl.1.html g7ctrl.1.pdf g7sh.1.html g7sh.1.pdf







