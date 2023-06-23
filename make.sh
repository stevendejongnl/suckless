#! /bin/bash

CURRENT=$PWD

echo $1

make_dwm() {
	echo "Make DWM"
	cd ./dwm
	rm -rf config.h
	sudo make clean install
	cd $CURRENT
}

make_slstatus() {
	echo "Make slstatus"
	cd ./slstatus
	rm -rf config.h
	sudo make clean install
	cd $CURRENT
}

make_dmenu() {
	echo "Make dmenu"
	cd ./dmenu
	rm -rf config.h
	sudo make clean install
	cd $CURRENT
}

make_surf() {
	echo "Make surf"
	cd ./surf
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

"dmenu")
	make_dmenu
	;;

"surf")
	make_surf
	;;

"all")
	make_dwm
	make_slstatus
	make_dmenu
	make_surf
	;;

*)
	echo "Run './make.sh dwm' or './make.sh slstatus' or './make.sh dmenu' or './make.sh surf' or './make.sh all'"
	;;

esac
