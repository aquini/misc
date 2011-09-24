#USAGE: for f in /proc/*/statm; do cat $f; done | awk -f statm2.awk 
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
	
  
