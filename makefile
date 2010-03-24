
LIBS =
CFLAGS += -O2 -g -Wall

all: client_pipe client_net server_pipe server_net

client_pipe: client.c wrapper.o wrapper.h
	$(CC) $(CFLAGS) $^ -o $@

client_net: client.c wrapper.o wrapper.h
	$(CC) $(CFLAGS) $^ -o $@ -DNET 

server_pipe: server.c wrapper.o wrapper.h
	$(CC) $(CFLAGS) $^ -o $@

server_net: server.c wrapper.o wrapper.h
	$(CC) $(CFLAGS) $^ -o $@ -DNET 

.PHONY: clean

clean:
	rm -rf client_pipe client_net server_pipe server_net *.o
