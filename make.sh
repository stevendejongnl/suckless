#! /bin/bash

CURRENT=$PWD

echo $1

make_dwm () {
    echo "Make DWM"
    cd ./dwm
    rm -rf config.h
    sudo make clean install
    cd $CURRENT
}

make_slstatus () {
    echo "Make slstatus"
    cd ./slstatus
    rm -rf config.h
    sudo make clean install
    cd $CURRENT
}

case $1 in
    "dwm")
        make_dwm
    ;;

    "slstatus")
        make_slstatus
    ;;

    "all")
        make_dwm
        make_slstatus
    ;;

    *)
        echo "Run './make.sh dwm' or './make.sh slstatus' or './make.sh all'"
    ;;

esac
