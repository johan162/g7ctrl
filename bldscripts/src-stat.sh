#!/bin/sh
cloc --autoconf --quiet --progress-rate=0 --force-lang=make,in --force-lang=make,ac --exclude-lang=D,make  --exclude-ext=txt --csv src/ docs/ | gawk --file=bldscripts/src-stat.awk > docs/manual/table-src-stats.xml
touch docs/manual/Technical.xml

