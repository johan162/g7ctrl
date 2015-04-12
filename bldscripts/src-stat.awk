# $Id$
# AWK source to format the output from cloc into valid Docbook XML
# Author: Johan Persson <johan162@gmail.com>
#
# Command line assumed in cloc: 
# cloc --quiet --progress-rate=0 --csv <DIRECTORIES>
#

BEGIN { 
print "<table xmlns=\"http://docbook.org/ns/docbook\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" version=\"5.0\" revision=\"$Id$\" xml:id=\"table.source-stats\" >\
<title>Breakdown of project LOC by type</title>\
<tgroup cols=\"5\" align=\"center\">\
<colspec colname=\"c1\" colnum=\"1\" colwidth=\"150pt\" align=\"left\"/>\
<colspec colname=\"c2\" colnum=\"2\" colwidth=\"50pt\" align=\"left\"/>\
<colspec colname=\"c3\" colnum=\"3\" colwidth=\"80pt\" align=\"right\"/>\
<colspec colname=\"c4\" colnum=\"4\" colwidth=\"80pt\" align=\"right\"/>\
<colspec colname=\"c5\" colnum=\"5\" colwidth=\"90pt\" align=\"right\"/>";
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
    printf( "<row><entry><emphasis role=\"bold\">TOTAL</emphasis></entry>\
  <entry><emphasis role=\"bold\">%d</emphasis></entry>\
  <entry><emphasis role=\"bold\">%d</emphasis></entry>\
  <entry><emphasis role=\"bold\">%d</emphasis></entry>\
  <entry><emphasis role=\"bold\">%d</emphasis></entry></row>\n",sum1,sum2,sum3,sum4);

printf( "<row><entry><emphasis role=\"bold\"> </emphasis></entry>\
  <entry><emphasis role=\"bold\"> </emphasis></entry>\
  <entry><emphasis role=\"bold\"> </emphasis></entry>\
  <entry><emphasis role=\"bold\"> </emphasis></entry>\
  <entry><emphasis role=\"bold\">(All: %d)</emphasis></entry></row>\n",sum1+sum2+sum3+sum4);
    
    print "</tbody></tgroup></table>\n"; 
}
