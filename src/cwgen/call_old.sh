#!/bin/bash

count_total=0

for i in {1..2500}
do
    count=$(cwgen -n 1 -g 100 | awk -f word_freq.awk -n | sort | wc -l)
    count_total=$(($count_total + $count))
    echo "old:" $i $count $count_total
    sleep 2
done

