#!/bin/sh
ver=
while getopts v: o
do  case "$o" in
    v)      ver="$OPTARG"
            ;;
    [?])    printf >&2 "Usage; $0 -v version\n"
            exit 1
            ;;
    esac
done

if test -z "$ver"; then
    printf >&2 "Usage: $0 -v ver\n"
    exit 1
fi

echo "Uploading [g7ctrl-cov.tar.xz] as version [$ver] ..."

curl --form token=$COVERITY_TOKEN \
  --form email=$COVERITY_EMAIL \
  --form file=@g7ctrl-cov.tar.xz \
  --form version="$ver" \
  --form description="Control daemon for GM7 tracker" \
  https://scan.coverity.com/builds?project=g7ctrl

if test "$?" != 0; then
    echo "================================"
    echo "FAILED to upload coverity build"
    echo "================================"
    exit 1
else
    rm -rf g7ctrl-cov.tar.xz
    echo "DONE."
fi
exit 0

