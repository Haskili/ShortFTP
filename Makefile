EXE = client server
CFLAGS = -Wall

all: $(EXE)

client: client.c
	gcc client.c -Wall -lcrypto -o client

server: server.c
	gcc server.c -Wall -lcrypto -o server

clean:
	rm -f $(EXE)

