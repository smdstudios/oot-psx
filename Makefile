TARGET = oot
TYPE = ps-exe
BINDIR = build/

SRCS = src/main.cpp src/room.cpp src/skeleton.cpp

CPPFLAGS += -Isrc
LDFLAGS += -lgcc

include third_party/nugget/psyqo-paths/psyqo-paths.mk

.PHONY: disc
disc: all
	@mkdir -p build
	mkpsxiso -y cd.xml -o build/oot.bin -c build/oot.cue
	@rm -f build/oot.elf build/oot.map build/oot.ps-exe
	@rm -f src/*.o src/*.dep
	@echo "CD image: build/oot.bin / oot.cue"

ROM ?= rom/baserom.z64

.PHONY: extract
extract:
	@if [ ! -f "$(ROM)" ]; then echo "ROM not found at $(ROM)"; echo "Place your OoT ROM at rom/baserom.z64 or run: make extract ROM=/path/to/oot.z64"; exit 1; fi
	bash tools/extract_rooms.sh $(ROM)

.DEFAULT_GOAL := all
