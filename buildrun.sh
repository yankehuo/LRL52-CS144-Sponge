#!/bin/bash
CUR_DIR="$(dirname "$(realpath $0)")"
echo "CUR_DIR: $CUR_DIR"
DIR="$CUR_DIR/build"
CLEAN="clean"
if [ -d $DIR ]; then
    echo "$DIR existed!"
    if [ "$1" = "$CLEAN" ]; then
        rm -rf $DIR
        echo "$DIR removed!"
    fi
fi

cmake -B build
cmake --build build 

# cd $DIR && make check_webget
# cd $DIR && make check_lab0
# cd $DIR && make check_lab1
# cd $DIR && ctest -R wrap
# cd $DIR && make check_lab2
# cd $DIR && make check_lab3
# cd $DIR && make check_lab4
# cd $DIR && ctest -R t_active_close
# cd $DIR && valgrind --tool=callgrind ./apps/tcp_benchmark
# cd $DIR && ./apps/tcp_benchmark
# cd $DIR && make check_lab5
# cd $DIR && ctest -V -R "^arp"
# cd $DIR && make check_lab6
cd $DIR && ./apps/webget cs144.keithw.org /hello
cd $DIR && ./apps/webget search.bilibili.com /all