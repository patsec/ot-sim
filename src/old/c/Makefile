.DEFAULT_GOAL := all

.PHONY: all
all: bin/debugger bin/logger bin/msgbus-test

.PHONY: clean
clean:
	$(RM) bin/*

bin/debugger: msgbus/debugger.c
	mkdir -p bin
	gcc msgbus/debugger.c -lczmq -lzmq -o bin/debugger

bin/logger: msgbus/logger.c
	mkdir -p bin
	gcc msgbus/logger.c -lczmq -lzmq -o bin/logger

bin/msgbus-test: msgbus/msgbus-test.c
	mkdir -p bin
	gcc msgbus/msgbus-test.c -lczmq -lzmq -o bin/msgbus-test
