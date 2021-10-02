.PHONY: build
build:
	APPS_DIR=apps ./build.sh

.PHONY: run
run:
	APPS_DIR=apps ./build.sh run
