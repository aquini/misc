##BIOS e820 gap finder
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
#USAGE: dmesg | grep BIOS-e820 | sed -s 's|\[.*\]\ ||' | awk -f e820-gapfinder.awk  
BEGIN { n=0 }

/^ BIOS-.*:.*)$/ {	mem_start[n]	= "0x" toupper($2)
			mem_end[n]	= "0x" toupper($4)
			mem_status[n]	= $5
			n++
}

END { 
	printf("%-15s%-20s%-20s%-20s%-20s%15s%15s\n", "STATUS", "START (hex)", "END (hex)", "SIZE (hex)", "GAP (hex)", "SIZE (dec)", "GAP (dec)")

	mem_size_total = 0
	mem_gap_total = 0
	mem_rsvd_total = 0
	for (x=0; x < n; x++) {
		mem_diff = 0
		mem_gap  = 0
		mem_diff = strtonum(mem_end[x]) - strtonum(mem_start[x])
		mem_gap  = strtonum(mem_start[x]) - strtonum(mem_end[x-1])

		printf("%-15s",		mem_status[x])
		printf("%-20s",		mem_start[x])
		printf("%-20s",		mem_end[x])
		printf("0x%016x",	mem_diff)
		printf("  0x%016x",	mem_gap)
		printf("%17s",		mem_diff)
		printf("%15s",		mem_gap)
		printf("\n")
		
		if (toupper(mem_status[x]) == "(USABLE)")
			mem_size_total += mem_diff
		
		if (toupper(mem_status[x]) == "(RESERVED)" || 
		    toupper(mem_status[x]) == "(ACPI") 
			mem_rsvd_total += mem_diff
		
		mem_gap_total += mem_gap	
	}

	printf("\nMEMORY MAP REPORT:\n%20s%20s%20s\n", "USABLE", "RESERVED", "HOLE")
	printf("%20s%20s%20s\n", mem_size_total, mem_rsvd_total, mem_gap_total)
	printf("%17s MB%17s MB%17s MB\n", (mem_size_total/2^20), (mem_rsvd_total/2^20), (mem_gap_total/2^20))
}

