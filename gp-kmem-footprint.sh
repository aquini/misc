#!/bin/bash
# gp-kmem-footprint.sh - plots data collected from kmem-footprint.sh
if [ $# -lt 1 ]; then
	echo "USAGE: $0 <data file>"
	echo "     - <data file>: data collected by kmem-footprint.sh script"
	exit 1
fi

input=${1}
dates="$(awk '{print $1}' $input | sort -u)"
gp=/tmp/gp
data=/tmp/plot.dat

for i in $dates
do
	awkprog="/$(echo $i|sed -s 's/\//\\\//g' )/ {print \$2, \$3, \$4}"
	awk "$awkprog" $input > $data
	
echo "set terminal png
set output \"kmem-footprint_$(echo $i|sed -s 's/\//-/g').png\"
set xdata time
set timefmt \"%H:%M:%S\"
set xrange [\"00:00:00\":\"23:59:59\"]
#set yrange [0:100]
set xlabel \"Hour\"
set ylabel \"RAM (mB)\"
#set xtics \"00:00:00\", 3600, \"23:59:59\"
set format x \"%H\"
#set nomxtics
set grid
set key top right box
set timestamp bottom
set title \"Kernel Memory Footprint - $i\"
plot \"$data\" using 1:(\$2/(2**10)) title \"usage\" with histeps lc rgb 'navy', \
\"$data\" using 1:(\$3/(2**10)) title \"overhead\" with histeps lc rgb 'red'" > $gp
    gnuplot $gp
#    rm -f $cpu $gp
done
