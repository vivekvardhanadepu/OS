#!/bin/bash

function GCD(){
	if [ $1 -ge $2 ]; then
		r=$[$1%$2]
		if [ $r == 0 ]; then
			return $2
		else
			GCD $2 $r
		fi
	else
		GCD $2 $1
	fi
}

OLDIFS="$IFS"
IFS=,
arr=($1)
IFS=$OLDIFS

gcd=${arr[0]}

for i in "${!arr[@]}";
 do
 	GCD ${arr[$i]} $gcd
 	gcd=$?
done

echo $gcd