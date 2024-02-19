.PHONY: dwm slstatus dmenu slock all

all: dwm slstatus dmenu slock

dwm:
	rm -rf ./dwm/config.h
	sudo make -C ./dwm clean install

slstatus:
	rm -rf ./slstatus/config.h
	sudo make -C ./slstatus clean install

dmenu:
	rm -rf ./dmenu/config.h
	sudo make -C ./dmenu clean install

slock:
	rm -rf ./slock/config.h
	sudo make -C ./slock clean install

