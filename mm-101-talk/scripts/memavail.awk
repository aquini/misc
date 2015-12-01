#  USAGE: awk -f memavail.awk /proc/zoneinfo
#
#  Copyright (C) 2012 Rafael Aquini <aquini@redhat.com>
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

function min(a, b)
{
	if (a < b)
		return a;
	return b;
}

BEGIN {
        free = inactive_file = actvive_file = wmark_low = slab_reclaim = 0;
}
	/nr_slab_reclaimable/ {slab_reclaim += $2};
	/nr_inactive_file/    {inactive_file += $2};
	/nr_active_file/      {actvive_file += $2};
        /nr_free_pages/       {free += $2};
        /low/                 {wmark_low += $2};
END {

	# Estimate the amount of memory available for userspace allocations,
	# without causing swapping.
	#
	# Free memory cannot be taken below the low watermark, before the
	# system starts swapping.
	available = free - wmark_low;

	# Not all the page cache can be freed, otherwise the system will
	# start swapping. Assume at least half of the page cache, or the
	# low watermark worth of cache, needs to stay.
	pagecache = inactive_file + actvive_file;
	pagecache -= min(pagecache/2, wmark_low);

	# Part of the reclaimable slab consists of items that are in use,
	# and cannot be freed. Cap this estimate at the low watermark.
	slab_reclaim -= min(slab_reclaim/2, wmark_low);

	available += pagecache + slab_reclaim;

	if (available < 0)
		available = 0;

        printf("MemAvailable: %d kB\n", available * 4);
}
