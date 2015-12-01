#!/bin/bash
# pg-trash-bench.sh - page-thrashing benchmark simulator
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

PRG="./mmap"
BLOATSZ=${2:-512}

if [ "$1" == "anon" ]; then
	$PRG --stats --anon $BLOATSZ --pg-fault=loop 2> logs/mmap-anon-1.log &
	sleep 5
	$PRG --stats --anon $BLOATSZ --pg-fault=loop 2> logs/mmap-anon-2.log &
elif [ "$1" == "file" ]; then
	$PRG --stats --file ./bigfile1.dat --pg-fault=loop 2> logs/mmap-file-1.log &
	sleep 5
	$PRG --stats --file ./bigfile2.dat --pg-fault=loop 2> logs/mmap-file-2.log &
elif [ "$1" == "mixed" ]; then
	$PRG --stats --file ./bigfile1.dat --pg-fault=loop 2> logs/mmap-mixed-1.log &
	sleep 5
	$PRG --stats --anon $BLOATSZ --pg-fault=loop 2> logs/mmap-mixed-2.log &
elif [ "$1" == "stop" ]; then
	killall $PRG
else
	echo "USAGE: $0 <anon|file|mixed>"
fi
