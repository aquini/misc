#!/bin/sh
# db-workload-report.sh - support script for db-bench.sh demos
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

PRG=${1}
CLN=${2}

RUN=$(ps r -C ${PRG} | awk 'END {print NR-1}')
SLP=$(ps -C ${PRG} |awk 'END {print NR-2}')

echo | awk -v C=$CLN -v R=$RUN -v S=$SLP 'END {printf "work: %d/%d (%3.2f %)\n", S, C, 100 *  S / C}'
echo "==="
free
echo "==="
awk -f $HOME/mm-101/scripts/memreport.awk /proc/meminfo
echo "==="
awk '/PageTables/ {print}' /proc/meminfo
