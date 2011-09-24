#USAGE: cat /proc/<pid>/statm | awk -f statm.awk
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
	
  
