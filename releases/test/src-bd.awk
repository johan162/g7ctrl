# 
# cloc --quiet --progress-rate=0 --sum-one --csv ../src ../docs | gawk --file=src-bd.awk

BEGIN { 
print "<table frame=\"all\">\
<title>Breakdown of project sources by type</title>\
<tgroup cols=\"5\" align=\"center\">\
<colspec colname=\"c1\" colnum=\"1\" colwidth=\"1.0*\"/>\
<colspec colname=\"c2\" colnum=\"2\" colwidth=\"1.0*\"/>\
<colspec colname=\"c3\" colnum=\"3\" colwidth=\"1.0*\"/>\
<colspec colname=\"c4\" colnum=\"4\" colwidth=\"1.0*\"/>\
<colspec colname=\"c5\" colnum=\"5\" colwidth=\"1.0*\"/>"
FS=",";
sum1=sum2=sum3=sum4=0;
}

NR==2 { 
    printf( "<thead>\
  <row>\
  <entry>%s</entry>\
  <entry>%s</entry>\
  <entry>%s</entry>\
  <entry>%s</entry>\
  <entry>%s</entry>\
  </row>\
  </thead><tbody>\n",$2,$1,$3,$4,$5);
}

NR>2 {
    printf( "<row><entry>%s</entry>\
  <entry>%s</entry>\
  <entry>%s</entry>\
  <entry>%s</entry>\
  <entry>%s</entry></row>\n",$2,$1,$3,$4,$5);
    sum1 += $1;
    sum2 += $3;
    sum3 += $4;
    sum4 += $5;
}

END { 
    printf( "<row><entry>TOTAL</entry>\
  <entry>%d</entry>\
  <entry>%d</entry>\
  <entry>%d</entry>\
  <entry>%d</entry></row>\n",sum1,sum2,sum3,sum4);

    print "</tbody></tgroup></table>\n"; 
}
