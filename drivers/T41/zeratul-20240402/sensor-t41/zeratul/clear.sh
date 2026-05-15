#!/bin/bash
dir=`ls ./` #定义遍历的目录
for i in $dir
do
    echo "##build" $i
    if [ $i != "build.sh" ]; then
    cd $i;make clean;cd -
    fi
done
