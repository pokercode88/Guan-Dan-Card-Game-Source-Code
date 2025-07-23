#!/bin/sh

if [ $# = 0 ]; then
    make -f makefile_gate
else
    if [ $1 = "debug" ]; then
        make -f makefile_gate
    else
        make release -f makefile_gate
    fi
fi


