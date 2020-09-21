#!/bin/bash

count_total=0

for i in {1..30}
do
    count=$(./cwgen -n 1 -g 100 | awk -f word_freq.awk -n | sort | wc -l)
    count_total=$(($count_total + $count))
    echo "new:" $i $count $count_total
    sleep 1
done

