
all: debug

release:
	@haxe compile.hxml --times 

debug:
	@haxe compile.hxml --times -debug -D FIBERUS_DEBUG

run:
	@bin/Main

clean:
	@rm -rf bin

cache_clear:
	@haxelib run fiberus cache clear

.PHONY: all release debug run clean cache_clear
