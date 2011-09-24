#USAGE: for f in /proc/*/statm; do cat $f; done | awk -f statm2.awk 
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
	PGSZ=4;
	printf("%10s %10s %10s %10s %10s %10s\n", "VMSIZE", "RSS", 
		"SHARED", "ANON", "TEXT", "DATA")
}
{ 
	VMSIZE 	+= $1;
	RSS	+= $2;
	SHARED	+= $3;
	TEXT	+= $4;
	DATA	+= $6;
}
END {
	ANON = RSS-SHARED
	printf("%10s %10s %10s %10s %10s %10s\n", VMSIZE*PGSZ, RSS*PGSZ, 
		SHARED*PGSZ, ANON*PGSZ, TEXT*PGSZ, DATA*PGSZ)
}
	
  
