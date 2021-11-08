#!/bin/bash

export NDN_LOG="kua.*=DEBUG"

sudo pkill kua
nfdc cs erase /

sleep 0.1
echo "Starting node"
./build/bin/kua /kua /one 0 >> log/one.log 2>&1 &
sleep 0.1
echo "Starting node"
./build/bin/kua /kua /two 0 >> log/two.log 2>&1 &

sleep 0.1
echo "Starting node"
./build/bin/kua /kua /three 0 >> log/three.log 2>&1 &

sleep 0.1
echo "Starting node"
./build/bin/kua /kua /four 0 >> log/four.log 2>&1 &

sleep 0.1
echo "Starting node"
./build/bin/kua /kua /five 0 >> log/five.log 2>&1 &

sleep 0.1
echo "Starting node"
./build/bin/kua /kua /six 0 >> log/six.log 2>&1 &

sleep 0.1
echo "Starting node"
./build/bin/kua /kua /seven 0 >> log/seven.log 2>&1 &

sleep 0.1
echo "Starting node"
./build/bin/kua /kua /eight 0 >> log/eight.log 2>&1 &

sleep 0.1
echo "Starting master"
./build/bin/kua /kua /master 1 2>&1 | tee log/master.log
