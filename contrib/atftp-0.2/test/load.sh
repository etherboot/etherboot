#!/bin/bash

if [ $# != 1 ]; then
    echo "Usage: load.sh [host1]"
    exit 1
fi

TFTP=../tftp
HOST=$1
FILE=linux
CONCURENT=40
TIMEOUT=80
LOOP=1
i=$LOOP
while [ $i -gt 0 ]; do
    echo -n "Loop $i "
    j=$CONCURENT
    while [ $j -gt 0 ]; do
	$TFTP -T 5 -t 10 --get -r $FILE -l /dev/null $HOST 2>$j.out&
	echo -n "."
	j=$[ $j - 1 ]
    done
    echo  " done"
    i=$[ $i - 1 ]
    if [ $i -gt 0 ]; then
  	sleep $TIMEOUT
    fi
done
