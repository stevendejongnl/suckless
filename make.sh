#! /bin/bash

CURRENT=$PWD

echo $1

case $1 in
    "dwm")
        echo "Make DWM"
        cd ./dwm
        rm -rf config.h
        sudo make clean install
        cd $CURRENT
    ;;

    "slstatus")
        echo "Make slstatus"
        cd ./slstatus
        rm -rf config.h
        sudo make clean install
        cd $CURRENT
    ;;

    *)
        echo "Run './make.sh dwm' or './make.sh slstatus'"
    ;;

esac
