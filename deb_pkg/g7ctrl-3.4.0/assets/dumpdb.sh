#!/bin/sh
sqlite3 /var/lib/g7ctrl/gm7tracker_db.sqlite3 <<EOS
.headers on
.mode tabs 
select * from tbl_track;
EOS

