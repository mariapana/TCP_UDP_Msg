CC = g++
CFLAGS = -Wall -g -Werror -Wno-error=unused-variable

all: server subscriber

server: server.cpp common.cpp
	$(CC) $(CFLAGS) -o $@ $^

subscriber: subscriber.cpp common.cpp
	$(CC) $(CFLAGS) -o $@ $^

.PHONY: clean run_server run_subscriber

clean:
	rm -rf server subscriber *.o *.dSYM
