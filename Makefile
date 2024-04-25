CC = g++
CFLAGS = -Wall -g -Werror -Wno-error=unused-variable

all: server subscriber

# Compileaza server.cpp
server: server.cpp common.cpp
	$(CC) $(CFLAGS) -o $@ $^

# Compileaza subscriber.cpp
subscriber: subscriber.cpp common.cpp
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean run_server run_subscriber

clean:
	rm -rf server subscriber *.o *.dSYM
