#USAGE: awk -f memreport.awk /proc/meminfo
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
BEGIN { 
	anon = mapm = memt = memf = kbuf = kchd = slab = ptbl = smem = 0;
}
	/AnonPages/	{anon += $2};
	/Mapped/ 	{mapm += $2}; 
	/MemTotal/ 	{memt += $2}; 
	/MemFree/ 	{memf += $2}; 
	/Buffers/ 	{kbuf += $2}; 
	/Cached/ 	{kchd += $2}; 
	/SReclaimable/  {slab += $2};
	/PageTables/ 	{ptbl += $2};
	/Shmem/ 	{smem += $2};
END {
	# total amount committed to userspace (mapped an unmapped pages)
	us = anon + mapm + smem;

	# total amount allocated by kernel not for userspace
	kd = memt - memf - us;

	# total amount in kernel caches (potentially free memory)
	kdc = kbuf + slab + (kchd - mapm) - ptbl;

	printf("%20s%20s%20s%20s\n", "", "Used (KB)", 
		"Cache (KB)", "non-Cache (KB)");
	printf("%-20s%20s%20s%20s\n", "kernel dynamic mem", 
		kd, kdc, (kd - kdc));
	printf("%-20s%20s%20s%20s\n", "userspace memory", 
		us, mapm, (us - mapm));
	printf("%-20s%20s%20s%20s\n", "free memory", 
		memf, 0, 0);
}
