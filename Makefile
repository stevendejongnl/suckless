.PHONY: dwm dmenu slock all

all: dwm dmenu slock

dwm:
	rm -rf ./dwm/config.h
	sudo make -C ./dwm clean install

dmenu:
	rm -rf ./dmenu/config.h
	sudo make -C ./dmenu clean install

slock:
	rm -rf ./slock/config.h
	sudo make -C ./slock clean install

