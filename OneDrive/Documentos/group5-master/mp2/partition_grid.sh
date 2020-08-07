#!/bin/bash

# Pick one of these to test:
# exe="./ls_router"
exe="./vec_router"


echo "Launching routers."

# do this on a 10x10
sudo perl utility/make_topology.pl utility/10x10gridtopo.txt
for ((num = 0; $num < 100; num++))
do
$exe $num /dev/null logs/$num.log &
done


read -p "Press enter to sever the grid."


# Sever Grid:
sudo iptables -D OUTPUT -s 10.1.1.3  -d 10.1.1.4  -j ACCEPT ; sudo iptables -D OUTPUT -s 10.1.1.4  -d 10.1.1.3  -j ACCEPT
sudo iptables -D OUTPUT -s 10.1.1.13 -d 10.1.1.14 -j ACCEPT ; sudo iptables -D OUTPUT -s 10.1.1.14 -d 10.1.1.13 -j ACCEPT
sudo iptables -D OUTPUT -s 10.1.1.23 -d 10.1.1.24 -j ACCEPT ; sudo iptables -D OUTPUT -s 10.1.1.24 -d 10.1.1.23 -j ACCEPT
sudo iptables -D OUTPUT -s 10.1.1.33 -d 10.1.1.34 -j ACCEPT ; sudo iptables -D OUTPUT -s 10.1.1.34 -d 10.1.1.33 -j ACCEPT
sudo iptables -D OUTPUT -s 10.1.1.43 -d 10.1.1.44 -j ACCEPT ; sudo iptables -D OUTPUT -s 10.1.1.44 -d 10.1.1.43 -j ACCEPT
sudo iptables -D OUTPUT -s 10.1.1.53 -d 10.1.1.54 -j ACCEPT ; sudo iptables -D OUTPUT -s 10.1.1.54 -d 10.1.1.53 -j ACCEPT
sudo iptables -D OUTPUT -s 10.1.1.63 -d 10.1.1.64 -j ACCEPT ; sudo iptables -D OUTPUT -s 10.1.1.64 -d 10.1.1.63 -j ACCEPT
sudo iptables -D OUTPUT -s 10.1.1.73 -d 10.1.1.74 -j ACCEPT ; sudo iptables -D OUTPUT -s 10.1.1.74 -d 10.1.1.73 -j ACCEPT
sudo iptables -D OUTPUT -s 10.1.1.83 -d 10.1.1.84 -j ACCEPT ; sudo iptables -D OUTPUT -s 10.1.1.84 -d 10.1.1.83 -j ACCEPT
sudo iptables -D OUTPUT -s 10.1.1.93 -d 10.1.1.94 -j ACCEPT ; sudo iptables -D OUTPUT -s 10.1.1.94 -d 10.1.1.93 -j ACCEPT

read -p "Press enter to heal the grid."

# Heal Grid:
sudo iptables -I OUTPUT -s 10.1.1.3  -d 10.1.1.4  -j ACCEPT ; sudo iptables -I OUTPUT -s 10.1.1.4  -d 10.1.1.3  -j ACCEPT
sudo iptables -I OUTPUT -s 10.1.1.13 -d 10.1.1.14 -j ACCEPT ; sudo iptables -I OUTPUT -s 10.1.1.14 -d 10.1.1.13 -j ACCEPT
sudo iptables -I OUTPUT -s 10.1.1.23 -d 10.1.1.24 -j ACCEPT ; sudo iptables -I OUTPUT -s 10.1.1.24 -d 10.1.1.23 -j ACCEPT
sudo iptables -I OUTPUT -s 10.1.1.33 -d 10.1.1.34 -j ACCEPT ; sudo iptables -I OUTPUT -s 10.1.1.34 -d 10.1.1.33 -j ACCEPT
sudo iptables -I OUTPUT -s 10.1.1.43 -d 10.1.1.44 -j ACCEPT ; sudo iptables -I OUTPUT -s 10.1.1.44 -d 10.1.1.43 -j ACCEPT
sudo iptables -I OUTPUT -s 10.1.1.53 -d 10.1.1.54 -j ACCEPT ; sudo iptables -I OUTPUT -s 10.1.1.54 -d 10.1.1.53 -j ACCEPT
sudo iptables -I OUTPUT -s 10.1.1.63 -d 10.1.1.64 -j ACCEPT ; sudo iptables -I OUTPUT -s 10.1.1.64 -d 10.1.1.63 -j ACCEPT
sudo iptables -I OUTPUT -s 10.1.1.73 -d 10.1.1.74 -j ACCEPT ; sudo iptables -I OUTPUT -s 10.1.1.74 -d 10.1.1.73 -j ACCEPT
sudo iptables -I OUTPUT -s 10.1.1.83 -d 10.1.1.84 -j ACCEPT ; sudo iptables -I OUTPUT -s 10.1.1.84 -d 10.1.1.83 -j ACCEPT
sudo iptables -I OUTPUT -s 10.1.1.93 -d 10.1.1.94 -j ACCEPT ; sudo iptables -I OUTPUT -s 10.1.1.94 -d 10.1.1.93 -j ACCEPT


read -p "Press enter to end."

pkill vec_router
pkill ls_router