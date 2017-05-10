#!/bin/sh
#$Id$
#Create ChangeLog from subversion logmessages
if test -d .svn; then
    svn log --xml -v -rHEAD:1 | xsltproc --stringparam strip-prefix "trunk/" bldscripts/svn2cl.xsl - > ChangeLog
fi
