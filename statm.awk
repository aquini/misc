#USAGE: cat /proc/<pid>/statm | awk -f statm.awk
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
BEGIN { PGSZ=4; }
{ 
	VMSIZE 	= $1*PGSZ;
	RSS	= $2*PGSZ;
	SHARED	= $3*PGSZ;
	ANON	= RSS-SHARED;
	TEXT	= $4;
	LIBS	= $5; #not accounted anymore -- always 0
	DATA	= $6;
	NONE 	= $7; # hardcoded -- always 0
}
END {
	print "/proc/pid/statm (pages): " $0;
	print "Convert to KB:"
	printf("%10s %10s %10s %10s %10s %10s\n", "VMSIZE", "RSS", 
		"SHARED", "ANON", "TEXT", "DATA")
	printf("%10s %10s %10s %10s %10s %10s\n", VMSIZE, RSS, 
		SHARED, ANON, TEXT, DATA)
}
	
  
