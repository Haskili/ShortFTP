#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <openssl/sha.h>

#define MAX_LINE 256
#define MAX_SIZE 150
#define MAX_LIST_LEN 500

#ifndef _CLIENT_H_
#define _CLIENT_H_

int verifyPassword(int s, const char * password, char * buf) {// Used to send() a given password through a given sockfd and recv() the response
	// Send the password to server
	send(s, password, MAX_LINE, 0);

	// Get response from server
	memset(buf, 0, MAX_LINE);
	recv(s, buf, MAX_LINE, 0);

	// Return based on verification message received (0 valid : 1 invalid)
	return (strcmp(buf, "VALID-NT") == 0 || strcmp(buf, "VALID-TS") == 0) ? 0 : 1;
}

int lookup_and_connect(const char *host, const char *service) {
	struct addrinfo hints;
	struct addrinfo *rp, *result;
	int s;

	// Translate host name into peer's IP address
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	// Use getaddrinfo() to give us a linked list of addresses
	if ((s = getaddrinfo(host, service, &hints, &result)) != 0) {
		fprintf(stderr, "stream-talk-client: getaddrinfo: %s\n", gai_strerror(s));
		return -1;
	}

	// Iterate through the address list and try to connect
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		if ((s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {continue;}
		if (connect(s, rp->ai_addr, rp->ai_addrlen) != -1) {break;}
		close(s);
	}

	// If our pointer in linked list is NULL
	if (rp == NULL) {
		perror("stream-talk-client: connect");
		return -1;
	}
	
	freeaddrinfo(result);
	return s;
}

int isReceiving(int s, fd_set fds, int seconds, int microseconds) {// Wait a given time period for activity on a designated fd
	// Clear the fdset
	FD_ZERO(&fds);
	FD_SET(s, &fds);

	// Reset the timevalues used for select() wait time
	struct timeval tv;
	tv.tv_sec = seconds;
	tv.tv_usec = microseconds;
	
	// Return (1 Activity on 's' : 0 No activity on 's')
	return ((select(s+1, &fds, NULL, NULL, &tv)) > 0) ? 1 : 0;
}
#endif