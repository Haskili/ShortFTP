EXE = client server
CFLAGS = -Wall

all: $(EXE)

client: client.c cFunctions.h
	gcc client.c -Wall -lcrypto -o client -L cFunctions.h

server: server.c sFunctions.h
	gcc server.c -Wall -lcrypto -o server -L sFunctions.h

clean:
	rm -f $(EXE)