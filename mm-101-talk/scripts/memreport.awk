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
	anon = cach = memf = memt = 0;
}
	/AnonPages:/	{anon += $2};
	/^Cached:/ 	{cach += $2};
	/MemFree:/ 	{memf += $2};
	/MemTotal:/ 	{memt += $2};
END {
	# total amount committed to userspace (mapped an unmapped pages)
	us = anon + cach;

	# total amount allocated by kernel not for userspace
	kd = memt - memf - us;

	printf("%20s%20s%20s%20s\n", "", "Used (KB)",
		"Cached (KB)", "non-Cached (KB)");
	printf("%-20s%20s%20s%20s\n", "userspace memory",
		us, cach, (us - cach));
	printf("%-20s%20s%20s%20s\n", "kernel memory",
		kd, 0, 0);
	printf("%-20s%20s%20s%20s\n", "free memory",
		memf, 0, 0);
}
