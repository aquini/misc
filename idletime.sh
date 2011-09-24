#!/bin/sh
# idletime.sh - demonstrates how to sample /proc/stat file.
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
count=0

while :; do
    arr=($(head -1 /proc/stat))

    cusr=${arr[1]}
    cnic=${arr[2]}
    csys=${arr[3]}
    cidl=${arr[4]}
    ciow=${arr[5]}
    chwi=${arr[6]}
    cswi=${arr[7]}
    cstl=${arr[8]}

    totjiffies=$((cusr+cnic+csys+cidl+ciow+chwi+cswi+cstl))

    
    user=$((cusr+cnic))
    idle=$((cidl+0))
    syst=$((csys+chwi+cswi))
    iowt=$((ciow+0))
    stol=$((cstl+0))

    if [ -z $ljiffies ]; then
	ljiffies=$totjiffies
    fi 
    ((!(count % 20))) && printf "%%idle\t%%user\t%%sys\t%%iowait\t%%steal\tDjiffies\n"
     printf "%s\t%s\t%s\t%s\t%s\t%d\n" \
               $(echo "scale=2;100 * $idle / $totjiffies"|bc) \
	       $(echo "scale=2;100 * $user / $totjiffies"|bc) \
               $(echo "scale=2;100 * $syst / $totjiffies"|bc) \
               $(echo "scale=2;100 * $iowt / $totjiffies"|bc) \
               $(echo "scale=2;100 * $stol / $totjiffies"|bc) \
               $((totjiffies-ljiffies))
    ((count++))
    ljiffies=$((totjiffies+0))

    sleep ${1:-1}
done

