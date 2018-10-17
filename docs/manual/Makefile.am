# Automake makefile to build the manual from XML sources.
#
# Written by Johan Persson <johan162@gmail.com>
# Copyright 2013-2015
#
# See ../COPYING for licensing terms. 

#-----------------------------------------------------------------------------------
# A note on the added complexity in this makefile
#
# This gets a bit more complicated than we want since the paths to handle
# a staged build will trip us up. Since the customized XSL files will be generated
# in the config process but the remaining XSL (and XML) files will remain static.
# This means that the generated files will be copied over to a staging directory
# i.e. when builddir != srcdir but not the remaining files. We therefore need to add
# some extra to handle this case and copy them ourself. One of the few things I
# really dislike about automake.
#
# The added tests to find out what capabilities the system we are building on
# also makes it impossible for make to fully parallelize the building of the
# different targets, for example it is technically possible to build both the
# PDF and HTML versions of the manual in parallel but unfortunately this will not
# be understood by make in this instance.
#-----------------------------------------------------------------------------------

# 
# These files are listed here to be added to EXTRA_DIST and so that they can be copied over
# properly for builddir != srcdir builds.
XML_FILES = Manual.xml CommandReference.xml ControllingDevice.xml Introduction.xml \
    ReceivingData.xml Technical.xml ConfiguringDaemon.xml ExportingDB.xml References.xml \
    TheCommandShell.xml ConnectingDevice.xml Installing.xml Preface.xml RunningTheDaemon.xml \
    TroubleShooting.xml TheCommandShell.xml ReleaseHistory.xml Backlog.xml \
    NEWS.xml table-src-stats.xml 

CSS_FILES = css_stylesheets/article.css \
    css_stylesheets/article.css css_stylesheets/article-fancyterm.css \
    css_stylesheets/comment_tn.gif css_stylesheets/comment_wh.gif \
    css_stylesheets/icon-collapse-minus-large.gif css_stylesheets/icon-expand-plus-large.gif \
    css_stylesheets/leaf_tn.gif css_stylesheets/ledblue.png \
    css_stylesheets/ledgreen.png css_stylesheets/ledorange.png \
    css_stylesheets/ledred.png css_stylesheets/ledyellow.png \
    css_stylesheets/minus-arrow.gif css_stylesheets/minus.gif \
    css_stylesheets/plus-arrow.gif css_stylesheets/plus.gif \
    css_stylesheets/img/admon-bug2.png css_stylesheets/img/admon-bug2_small.png \
    css_stylesheets/img/admon-bug.png css_stylesheets/img/admon-caution.png \
    css_stylesheets/img/admon-note.png css_stylesheets/img/admon-tip.png \
    css_stylesheets/img/admon-warning.png css_stylesheets/img/checkmark-green.png \
    css_stylesheets/img/checkmark-large.png css_stylesheets/img/checkmark-plain.gif \
    css_stylesheets/img/code-anonymous-bottom.png css_stylesheets/img/code-anonymous-middle.png \
    css_stylesheets/img/code-anonymous-top.png

CSS_DIRECTORIES = css_stylesheets/img css_stylesheets

IMAGE_FILES = gm7_tracker_small.jpg gm7_tracker_tall.jpg event_mail_with_minimap_large.png \
 pdf_report_pg1.png  pdf_report_pg2.png fop.cfg

# Documentation for the database that should be included in the distribution 
DBDOC_FILES = dbdoc/export_db.rng dbdoc/README dbdoc/db_schema.sql \
    dbdoc/export_db-rng-grammar.png

DBDOC_DIRECTORIES = dbdoc

XSL_FILES = xsl_stylesheets/xmldepend.xsl \
    xsl_stylesheets/images/caution.png \
    xsl_stylesheets/images/caution-small.png \
    xsl_stylesheets/images/caution.svg \
    xsl_stylesheets/images/important.png \
    xsl_stylesheets/images/important-small.png \
    xsl_stylesheets/images/important.svg \
    xsl_stylesheets/images/note.png \
    xsl_stylesheets/images/note-small.png \
    xsl_stylesheets/images/note.svg \
    xsl_stylesheets/images/tip.png \
    xsl_stylesheets/images/tip-small.png \
    xsl_stylesheets/images/tip.svg \
    xsl_stylesheets/images/warning.png \
    xsl_stylesheets/images/warning-small.png \
    xsl_stylesheets/images/warning.svg \
    xsl_stylesheets/chunk.xsl  \
    xsl_stylesheets/common.xsl  \
    xsl_stylesheets/epub.xsl  \
    xsl_stylesheets/fo.xsl  \
    xsl_stylesheets/html.fancycmd.xsl  \
    xsl_stylesheets/html.xsl  \
    xsl_stylesheets/website.xsl  \
    xsl_stylesheets/xmldepend.xsl  


XSL_DIRECTORIES = xsl_stylesheets/images xsl_stylesheets

EVENT_FILES = 0_action.sh.template 2_action.sh.template 40_action.sh.template tracker_conn.sh.template \
100_action.sh.template 34_action.sh.template 48_action.sh.template 1_action.sh.template 37_action.sh.template 4_action.sh.template


CLEANFILES = *~
EXTRA_DIST = $(XML_FILES) out src-stat.awk ${CSS_FILES} ${IMAGE_FILES} ${XSL_FILES} ${DBDOC_DIRECTORIES} ${DBDOC_FILES}

#
# Hooks to allow us to do the standard "make html" and "make pdf"
# 
out: all
html-local:  out/html/index.html
pdf-local:  out/pdf/Manual-A4-duplex.pdf	

#
# We can only build the text version of the manual if the "links" text browser
# is installed which we use to convert from HTML to text
#
if have_links
all: html-local txt-local pdf-local
txt-local: out/txt/Manual.txt
else
all: html-local pdf-local
endif


#
# If the user has an xsl processor we can go ahead and generate the manual
# from sources. In case the user don't have a working Docbook setup instead
# we use the shipped pre-built version of the manual in the distribution
#

if have_xsltproc

#.PHONY: setup_files

# 
# The copying of files is necessary if the build doesn't take place in the original source but
# in a separate build directory. In that case we must copy the files necessary to build the docs
# to that dirctory and also any files included in the docs.
# This is necessary since the Docbook files reference all inclusion and source by relative paths
# and we must mimic the same structure as in the original source if we rebuild in a separate
# directory.
#
setup-files: 
	[ -d "out/html" ] || mkdir -p "out/html" ; \
	for f in $(IMAGE_FILES) ; do               \
	    [ -e "$(builddir)/out/html/$$f" ] || cp "$(srcdir)/$$f" "$(builddir)/out/html/$$f" ;  \
	done ;		\
	if [ x"$(builddir)" != x"$(srcdir)" ] ; then		\
		[ -d "$(builddir)/../../src/etc/event-scripts" ] || mkdir "$(builddir)/../../src/etc/event-scripts" ; \
		[ -e "$(builddir)/../../src/etc/g7ctrl.conf.template.in" ] || cp "$(srcdir)/../../src/etc/g7ctrl.conf.template.in" "$(builddir)/../../src/etc/g7ctrl.conf.template.in" ; \
		[ -e "$(builddir)/../../NEWS" ] || cp  "$(srcdir)/../../NEWS" "$(builddir)/../../NEWS"	;	 \
		[ -e "$(builddir)/../../TODO" ] || cp  "$(srcdir)/../../TODO" "$(builddir)/../../TODO"	;	 \
		for f in $(EVENT_FILES) ; do					\
		    [ -e "$(builddir)/../../src/etc/event-scripts/$$f" ] || cp "$(srcdir)/../../src/etc/event-scripts/$$f" "$(builddir)/../../src/etc/event-scripts/$$f" ;\
		done ;								\
		for f in $(XML_FILES) ; do					\
		    [ -e "$(builddir)/$$f" ] || cp "$(srcdir)/$$f" "$(builddir)/$$f" ;\
		done ;								\
		for d in $(CSS_DIRECTORIES) ; do                            	\
		    [ -d "$(builddir)/$$d" ] || mkdir -p "$(builddir)/$$d" ;   	\
		done ;                                                      	\
		for f in $(CSS_FILES) ; do                                  	\
		    [ -e "$(builddir)/$$f" ] || cp "$(srcdir)/$$f" "$(builddir)/$$f" ;  \
		done ;                                                      	\
		for d in $(DBDOC_DIRECTORIES) ; do                          	\
		    [ -d "$(builddir)/$$d" ] || mkdir -p "$(builddir)/$$d" ;   	\
		done ;                                                      	\
		for f in $(DBDOC_FILES) ; do                                	\
		    [ -e "$(builddir)/$$f" ] || cp "$(srcdir)/$$f" "$(builddir)/$$f" ;  \
		done ;							    	\
		for f in $(IMAGE_FILES) ; do                                	\
		    [ -e "$(builddir)/$$f" ] || cp "$(srcdir)/$$f" "$(builddir)/$$f" ;    \
		done ;	\
		for d in $(XSL_DIRECTORIES) ; do                          	\
		    [ -d "$(builddir)/$$d" ] || mkdir -p "$(builddir)/$$d" ;   	\
		done ;                                                      	\
		for f in $(XSL_FILES) ; do                                	\
		    [ -e "$(builddir)/$$f" ] || cp "$(srcdir)/$$f" "$(builddir)/$$f" ;  \
		done ;		\
        fi
	@touch setup-files


distclean-local: 
	set -e; 
	rm -f "setup-files" ; \
	if [ x"$(builddir)" != x"$(srcdir)" ] ; then	\
		rm -f "$(builddir)/../../NEWS"  	;	\
		rm -f "$(builddir)/../../TODO"  	;	\
		rm -f "$(builddir)/../../src/etc/g7ctrl.conf.template.in" ; \
		for f in $(EVENT_FILES) ; do                         	\
		    rm -f "$(builddir)/../../src/etc/event-scripts/$$f" ;    \
		done ;	\
		[ -d "$(builddir)/../../src/etc/event-scripts" ] || rmdir "$(builddir)/../../src/etc/event-scripts";     \
		for f in $(XML_FILES) ; do                         	\
		    rm -f "$(builddir)/$$f" ;                      	\
		done ;							\
		for f in $(CSS_FILES) ; do                         	\
		    rm -f "$(builddir)/$$f" ;                      	\
		done ;                                             	\
		for f in $(XSL_FILES) ; do                         	\
		    rm -f "$(builddir)/$$f" ;                      	\
		done ;                                             	\
		for f in $(IMAGE_FILES) ; do                       	\
		    rm -f "$(builddir)/$$f" ;                      	\
		done ;                                              	\
		for f in $(DBDOC_FILES) ; do                       	\
		    rm -f "$(builddir)/$$f" ;                      	\
		done ;                                             	\
		for d in $(CSS_DIRECTORIES) ; do                   	\
		    [ -d "$(builddir)/$$d" ] || rmdir "$(builddir)/$$d";     \
		done ;						   	\
		for d in $(DBDOC_DIRECTORIES) ; do                 	\
		    [ -d "$(builddir)/$$d" ] || rmdir "$(builddir)/$$d";    \
		done ; 						   	\
		for d in $(XSL_DIRECTORIES) ; do                   	\
		    [ -d "$(builddir)/$$d" ] || rmdir "$(builddir)/$$d";    \
		done ;						   	\
        fi                        


table-src-stats.xml:
	${CLOC} --autoconf --quiet --progress-rate=0 --force-lang=make,in \
	--force-lang=make,ac --exclude-lang=D,make  --exclude-ext=txt --csv \
	../../src/ ../. | ${GAWK} --file=$(srcdir)/src-stat.awk > $(builddir)/table-src-stats.xml



out/html/index.html: setup-files $(XML_FILES) css_stylesheets/article.css xsl_stylesheets/html.xsl xsl_stylesheets/common.xsl ../../TODO
	set -e; 
	${XSLTPROC} --nonet --xinclude --output out/html/index.html xsl_stylesheets/html.xsl Manual.xml

if have_links
out/txt/Manual.txt: out/html/index.html
	rm -rf out/txt
	mkdir -p out/txt 
	links -width 100 -dump $(builddir)/out/html/index.html > $(builddir)/out/txt/Manual.txt
endif

clean-local:
	-rm -rf out
	-rm -f table-src-stats.xml
	-rm -f setup-files

if have_javafop

out/pdf/Manual-A4-duplex.pdf: setup-files $(XML_FILES) xsl_stylesheets/fo.xsl xsl_stylesheets/common.xsl css_stylesheets/article.css ../../TODO
	set -e; 
	[ -d "out/pdf" ] || mkdir -p "out/pdf" ; \
	${XSLTPROC} --nonet --xinclude --output out/pdf/Manual.fo xsl_stylesheets/fo.xsl Manual.xml
	${FOP} -c $(srcdir)/fop.cfg -q out/pdf/Manual.fo $@  2>&1 | grep -v "\[INFO\]"
	@echo "--------------------------------------------------------------------------------------------"
	@echo "--- It is safe to ignore warnings about fonts and exceeding available area.              ---"
	@echo "--------------------------------------------------------------------------------------------"

else

out/pdf/Manual-A4-duplex.pdf:
	touch $@

endif

else

out/html/index.html:
	touch $@

out/txt/Manual.txt:
	touch $@

setup-files:
	touch $@

endif


# Since we have added the variable "dist_stdpackagedochtml_DATA" we need to tell make how to 
# resolve the dependent files. We need this complication since we want to include a pre-built
# version of the documentation in case the end user does not have a complete Docbook setup.
# We probably should be thinking of having a separate package for the documentation but
# since we include some config files that are taken directly from the source tree it 
# still makes sense to have them in the same tree.

out/html/pdf_report_pg1.png: setup-files 

out/html/pdf_report_pg2.png: setup-files 

out/html/gm7_tracker_small.jpg: setup-files 

out/html/gm7_tracker_tall.jpg: setup-files 

out/html/event_mail_with_minimap_large.png: setup-files 


# We ask make to include these files in the distribution
dist_stdpackagedoc_DATA = out/pdf/Manual-A4-duplex.pdf
dist_stdpackagedochtml_DATA = out/html/index.html css_stylesheets/article.css \
    out/html/gm7_tracker_small.jpg out/html/gm7_tracker_tall.jpg out/html/event_mail_with_minimap_large.png \
    out/html/pdf_report_pg1.png out/html/pdf_report_pg2.png 

