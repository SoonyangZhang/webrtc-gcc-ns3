#! /bin/sh
algo=$1
id=$2
min_bw=$3
max_bw=$4
data_folder=$5
echo "$algo"
echo "$id"
prefix1=${data_folder}/${id}_${algo}_1
prefix2=${data_folder}/${id}_${algo}_2
prefix3=${data_folder}/${id}_${algo}_3
file1=${prefix1}_bw.txt
file2=${prefix2}_bw.txt
file3=${prefix3}_bw.txt
output=${id}-${algo}
gnuplot<<!
set xlabel "time/s" 
set ylabel "rate/kbps"
set xrange [0:200]
set yrange [${min_bw}:${max_bw}]
set term "png"
set output "${output}-send-rate.png"
plot "${file1}" u 1:2 title "flow1" with lines lw 2 lc 1,\
"${file2}" u 1:2 title "flow2" with lines lw 2 lc 2,\
"${file3}" u 1:2 title "flow3" with lines lw 2 lc 3
set output
exit
!


file1=${prefix1}_owd.txt
file2=${prefix2}_owd.txt
file3=${prefix3}_owd.txt
gnuplot<<!
set xlabel "time/s" 
set ylabel "delay/ms"
set xrange [0:200]
set yrange [100:400]
set term "png"
set output "${output}-owd.png"
plot "${file1}" u 1:2 title "flow1" with lines lw 2
set output
exit
!
