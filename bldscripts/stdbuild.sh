#!/bin/sh
# Utility script to do a standard build
bldscripts/stdconfig.sh
if test "$?" = 0; then
    make -j8 -s
else
    echo "Configuration failed. Cannot build package."
fi

