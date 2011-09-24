#!/bin/sh
# kmem-footprint.sh - grabs a rough footprint on kernel memory usage
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
# usage: ./kmem-footprint [timeout (min)]
# prints out: <date> <timestamp> <kernel memory usage (kB)> <overhead (kB)>

meminfo="/proc/meminfo"
timeout=${1:-1}

function calc_kmem_footprint() {
floor="$(awk '/^Slab|^PageTables/ {floor += $2} END {print floor}' $meminfo)"
ceiling="$(awk '/^Mem|ctive:/ {if (tolower($1) == "memtotal:") mt = $2; 
			else ceiling += $2} END {print mt-ceiling}' $meminfo)"
hugepg="$(awk '/^Huge.*(tal|size):/ {if (tolower($1) == "hugepagesize:") 
						size = $2; else nr = $2;} 
						END {print size*nr}' $meminfo)"

echo "$floor $((ceiling-floor-hugepg))"
}

while true; do
	D=$(date +"%D %T")
	calc_kmem_footprint | while read F O; do 
		echo "$D $F $O"
	done
	sleep=$((${timeout}*60))
	sleep $sleep
done
