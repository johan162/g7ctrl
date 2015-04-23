#!/bin/sh
# Utility script to prepare a new release
echo "Creating new ChangeLog ..."
bldscripts/mkcl.sh
echo "Updating code statistics ..."
rm docs/manual/table-src-stats.xml
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

