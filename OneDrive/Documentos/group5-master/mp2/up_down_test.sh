#!/bin/bash

# Pick one of these to test:
# exe="./ls_router"
exe="./vec_router"


echo "Launching routers."

cp ${exe} ${exe}_tmp

sudo perl utility/make_topology.pl utility/8x8gridtopo.txt

for ((num = 0; $num < 78; num++))
do
if (($num % 10 == 8 || $num % 10 == 9 || $num == 1 || $num == 15 || $num == 23 || $num == 30 || $num == 42 || $num == 57 || $num == 61 || $num == 5 || $num == 76 || $num == 73 || $num == 45)); then continue; fi
${exe} $num /dev/null logs/$num.log &
done

for ((num = 0; $num < 78; num++))
do
if (($num % 10 == 8 || $num % 10 == 9)); then continue; fi

if (($num == 1 || $num == 15 || $num == 23 || $num == 30 || $num == 42 || $num == 57 || $num == 61 || $num == 5 || $num == 76 || $num == 73 || $num == 45))
then
${exe}_tmp $num /dev/null logs/$num.log &
fi

done

while ((1))
do

read -p "Press enter to take down half the grid."

pkill vec_router_tmp
pkill ls_router_tmp

read -p "Press enter to put it back."

for ((num = 0; $num < 78; num++))
do
if (($num % 10 == 8 || $num % 10 == 9)); then continue; fi
if (($num == 1 || $num == 15 || $num == 23 || $num == 30 || $num == 42 || $num == 57 || $num == 61 || $num == 5 || $num == 76 || $num == 73 || $num == 45))
then
${exe}_tmp $num /dev/null logs/$num.log &
fi
done

read -p "Press enter to end."

done 

pkill vec_router_tmp
pkill ls_router_tmp
pkill vec_router
pkill ls_router


rm ${exe}_tmp