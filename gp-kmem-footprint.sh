#!/bin/bash
# gp-kmem-footprint.sh - plots data collected from kmem-footprint.sh
#  Copyright (C) 2011 Rafael Aquini <aquini@redhat.com>
# 
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 2 of the License, or
#  (at your option) any later version.
# 
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
# 
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
