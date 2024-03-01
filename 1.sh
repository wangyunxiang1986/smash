#!/bin/bash

cd trento/build

for ((i=1;i<=10;i++))
do
	echo $i
	trento Pb Pb -d 1.2 10
done

