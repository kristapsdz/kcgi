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
set xlabel 'milliseconds'
set size 0.8,0.8
set key outside right
set ytics 0.25
unset title
plot 'perf-time-openbsd-base.dat' u 2:($1/100) w l lw 3 ti '(1)', \
     'perf-time-openbsd-base-cgi.dat' u 2:($1/100) w l lw 3 ti '(2)', \
     'perf-time-openbsd-kcgi-nosandbox.dat' u 2:($1/100) w l lw 3 ti '(3)', \
     'perf-time-openbsd-kcgi-nocompress.dat' u 2:($1/100) w l lw 3 ti '(4)', \
     'perf-time-openbsd-kcgi.dat' u 2:($1/100) w l lw 3 ti '(5)'
