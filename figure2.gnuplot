#!/usr/local/bin/gnuplot
set terminal pngcairo crop enhanced font "Times,15"
set output 'figure2.png'
set border linewidth 1.5
set style line 1 lc rgb '#800000' lt 1 lw 2
set style line 2 lc rgb '#ff0000' lt 1 lw 2
set style line 3 lc rgb '#ff4500' lt 1 lw 2
set style line 4 lc rgb '#ffa500' lt 1 lw 2
set style line 5 lc rgb '#006400' lt 1 lw 2
set style line 6 lc rgb '#0000ff' lt 1 lw 2
set style line 7 lc rgb '#9400d3' lt 1 lw 2
set style line 11 lc rgb '#808080' lt 1
set border 19 back ls 11
set tics nomirror out scale 0.75
set style line 12 lc rgb'#808080' lt 0 lw 1
set grid back ls 12
#########################################################
set key bottom right
set xlabel 'milliseconds'
set datafile separator '\t'

stats 'figure2-cgi.tsv' u 5 nooutput
nr1=STATS_records
stats 'figure2-fcgi.tsv' u 5 nooutput
nr2=STATS_records
stats 'figure2-static.tsv' u 5 nooutput
nr3=STATS_records
stats 'figure2-cgi-naked.tsv' u 5 nooutput
nr4=STATS_records

set xrange [0:60]
plot 'figure2-static.tsv'    u 5:(1.0/nr3) s cum lw 4 ti 'static', \
     'figure2-cgi.tsv'       u 5:(1.0/nr1) s cum lw 4 ti 'CGI', \
     'figure2-cgi-naked.tsv' u 5:(1.0/nr4) s cum lw 4 ti 'CGI (simple)', \
     'figure2-fcgi.tsv'      u 5:(1.0/nr2) s cum lw 4 ti 'FastCGI'
