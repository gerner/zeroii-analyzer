SHELL=/bin/bash
SOURCES=$(shell find . -name *.cpp -o -name *.h -o -name *.ino)

FQBN=arduino:renesas_uno:minima

build/build_flag:  $(SOURCES)
	arduino-cli compile --fqbn $(FQBN) zeroii-analyzer
	touch build/build_flag

all: build/build_flag

upload: all
	arduino-cli upload zeroii-analyzer -p /dev/ttyACM0 --fqbn $(FQBN)

clean:
	rm build/build_flag
