#!/bin/bash

export NDN_LOG="kua.*=DEBUG"

sudo pkill kua
nfdc cs erase /

startnode () {
  sleep 0.1
  echo "Starting node $1"
  ./build/bin/kua /kua /$1 >> log/$1.log 2>&1 &
}

startnode one
startnode two
startnode three
startnode four
startnode five
startnode six
startnode seven
startnode eight

sleep 0.1
echo "Starting master"
./build/bin/kua-master /kua /master 2>&1 | tee log/master.log
