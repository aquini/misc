#!/bin/sh
# kmem-footprint.sh - grabs a rough footprint on kernel memory usage
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
