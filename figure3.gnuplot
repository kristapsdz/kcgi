#!/usr/local/bin/gnuplot
set terminal pngcairo crop enhanced font "Times,15"
set output 'figure3.png'
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
set size 0.8,0.8
set ylabel "ms"
set xtics rotate by -45
set style fill solid border -1
set xtics ("static" 4, "slowcgi(8)" 3, "kcgi(3)" 2, "no sandbox" 1, "no compress" 0)
plot '-' w hist title ''
2.947
2.481 
2.916 
1.864 
0.22 
e
