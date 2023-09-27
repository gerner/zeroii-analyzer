SHELL=/bin/bash
SOURCES=$(shell find . -name *.cpp -o -name *.h)

all: $(SOURCES)
	arduino-cli compile --fqbn arduino:avr:uno zeroii-analyzer
