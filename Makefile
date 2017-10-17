
SOURCES = $(wildcard src/*.cpp)

include ../../plugin.mk


dist: all
	mkdir -p dist/JW-Modules
	cp LICENSE* dist/JW-Modules/
	cp plugin.* dist/JW-Modules/
	cp -R res dist/JW-Modules/
