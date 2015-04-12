#!/bin/sh
# ------------------------------------------------------------------------------------------------------------
# Boostrap script for autoconfif/automake setup assuming environment is in place
# ------------------------------------------------------------------------------------------------------------
touch ChangeLog 
autoreconf --install --symlink

if [ "$?" = 0 ]; then 

echo "--------------------------------------------------------------"
echo " DONE. Build environment is ready. "
echo " "
echo " You can now run \"./stdbuild.sh\" to build the daemon "
echo " and then then run \"./mkrelease.sh\" to create new releases. "
echo "--------------------------------------------------------------"

else

echo "ERROR: Cannot setup build environment. Is autoconf/automake installed?"

fi

