#!/bin/sh
# db-bench.sh - simulates a small database workload
#  Copyright (C) 2014 Rafael Aquini <aquini@redhat.com>
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

function start_server()
{
	DB_PRG=${1}
	SGASZ=${2}
	BLOATSZ=${3}
	SLEEPTM=${4:-15}
	HUGETLB=$5
	if [ $HUGETLB == 0 ]; then
		$DB_PRG --create --shmsz $SGASZ --fault=write --wait &>/dev/null &
	else
		$DB_PRG --create --shmsz $SGASZ --tlbhuge --fault=write --wait &>/dev/null &
	fi
	sleep $[1+$[RANDOM % SLEEPTM]]
}

function start_client()
{
	DB_PRG=${1}
	SGASZ=${2}
	BLOATSZ=${3}
	SLEEPTM=${4}
	$DB_PRG --attach -z $[(SGASZ/28)*27] --bloat $BLOATSZ --fault=write --wait &>/dev/null &
	sleep $[1+$[RANDOM % SLEEPTM]]
}

function start_clients()
{
	DB_PRG=${1}
	CLIENTS=${2}
	SGA_SZ=${3}
	BLOATS=${4}
	SLEEPTM=$[1+$[${5:-15} / CLIENTS]]

	for ((i=0; i < $CLIENTS; i++)) {
		start_client $DB_PRG $SGA_SZ $BLOATS $SLEEPTM
	}
}

function start_watch()
{
	PRG=${1}
	CLIENTS=${2}
	watch -n1 scripts/db-workload-report.sh ${PRG#./*} $CLIENTS
	echo ""
}

PRG="./shmem"
SGA_SZ=512
BLOATS=20
CLIENTS=${2:-1}
HUGETLB=${3:-0}
SLEEPTM=${4:-60}

if [ "$1" == "start" ]; then
	echo "Starting server task..."
	start_server $PRG $SGA_SZ $BLOATS $SLEEPTM $HUGETLB

	echo "Starting $CLIENTS client tasks..."
	start_clients $PRG $CLIENTS $SGA_SZ $BLOATS &

	start_watch $PRG $CLIENTS
elif [ "$1" == "stop" ]; then
	killall -9 $PRG
	$PRG -a -d
else
	echo "USAGE: $0 <start|stop> [#-clients] [hugetlb-toggle]"
fi
