# Source env.sh first. Use a separate directory from the archived IDF4 build.
.PHONY: all menuconfig flash monitor
all:
	idf.py -B build-matter build
menuconfig:
	idf.py -B build-matter menuconfig
flash:
	idf.py -B build-matter flash
monitor:
	idf.py -B build-matter monitor
