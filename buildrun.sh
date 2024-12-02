#!/bin/bash
DIR=./build
if [ -d $DIR ];then
    echo "$DIR existed!"
    rm -rf $DIR
    echo "$DIR removed!"
fi

cmake -B build
cmake --build build