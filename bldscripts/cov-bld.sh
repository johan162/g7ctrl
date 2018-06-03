#!/bin/sh
# Create a coverity build to be submitted
#PATH=$PATH:/opt/cov-analysis-linux64-2017.07/bin
[ -d cov-int ] && rm -r cov-int
make clean
if test "$?" != 0; then
    echo "Failed to run make clean"
    exit 1
fi
cov-build --dir cov-int make -j8
if test "$?" != 0; then
    echo "========================="
    echo "FAILED coverity build"
    echo "========================="
    exit 1
fi
tail cov-int/build-log.txt
tar cJf g7ctrl-cov.tar.xz cov-int

if test "$?" = 0; then
    rm -rf cov-int
    echo "Deleted temporary build directory"
else
    echo "Failed to create Coverity tar-ball. Leaving temp build directory"
    exit 1
fi
echo "DONE"
exit 0

