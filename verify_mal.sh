#!/bin/bash

if [ $# -ne 1 ]; then
    echo 1
    exit
fi

perm="$(ls -l $1 | cut -c 8-10)"

if [ "$perm" = "---" ]; then
    echo 1
    exit
fi
echo 0
