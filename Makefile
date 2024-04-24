CFLAGS = -Wall -g -Werror -Wno-error=unused-variable

all: server subscriber

common.o: common.c

# Compileaza server.c
server: server.c common.o

# Compileaza subscriber.c
subscriber: subscriber.c common.o

.PHONY: clean run_server run_subscriber

# Ruleaza serverul
run_server:
	./server

# Ruleaza clientul
run_subscriber:
	./subscriber

clean:
	rm -rf server subscriber *.o *.dSYM
