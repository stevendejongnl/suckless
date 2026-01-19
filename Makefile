.PHONY: dwm dmenu slock overlay all

all: dwm dmenu slock overlay

dwm:
	rm -rf ./dwm/config.h
	sudo make -C ./dwm clean install

dmenu:
	rm -rf ./dmenu/config.h
	sudo make -C ./dmenu clean install

slock:
	rm -rf ./slock/config.h
	sudo make -C ./slock clean install

overlay:
	cd ../../personal/dwm-hotkey-overlay-rs && \
	cargo build --release && \
	sudo cp target/release/dwm-hotkey-overlay /usr/local/bin/

