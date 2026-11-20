CC ?= cc
CFLAGS = -std=c11 -Wall -Wextra -Werror -pedantic -g -pthread
CORE = JumpPilot/Core
ifeq ($(shell uname -s),Darwin)
SHARED_FLAGS = -dynamiclib
SHARED_NAME = libjumppilot.dylib
else
SHARED_FLAGS = -shared -fPIC
SHARED_NAME = libjumppilot.so
endif

.PHONY: test clean
test:
	mkdir -p build
	$(CC) $(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer -I$(CORE) Tests/test_protocol.c $(CORE)/JPProtocol.c -o build/test_protocol
	ASAN_OPTIONS=detect_leaks=0 ./build/test_protocol
	$(CC) $(CFLAGS) $(SHARED_FLAGS) -I$(CORE) $(CORE)/JPProtocol.c $(CORE)/JPClient.c -o build/$(SHARED_NAME)
	python3 Tests/test_network.py

clean:
	rm -rf build
