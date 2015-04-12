#!/bin/sh
# $Id$
# Utility script to prepare a new release
echo "Creating new ChangeLog ..."
bldscripts/mkcl.sh
echo "Updating code statistics ..."
bldscripts/src-stat.sh
echo "Running autoreconf ..."
autoreconf
if test "$?" = 0; then
	echo "Running stdconfig ..."
		bldscripts/stdconfig.sh > /dev/null
	if test "$?" = 0; then
		make -s distcheck 
	else
		echo "Error running config !"
	fi
else
	echo "Error running autoreconf !"
fi

