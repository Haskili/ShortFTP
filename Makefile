EXE = client server
CFLAGS = -Wall

all: $(EXE)

client: client.c
	gcc -Wall client.c -o client

server: server.c
	gcc -Wall server.c -o server

clean:
	rm -f $(EXE)

