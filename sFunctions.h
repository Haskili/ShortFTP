#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>
#include <openssl/sha.h>
#include <stdarg.h>

#define MAX_LINE 256
#define MAX_LIST_LEN 500
#define MAX_PENDING 5
#define MAX_SIZE 150

#ifndef _SERVER_H_
#define _SERVER_H_

int createList() {//Creates a file to store list of files in directory server is started
	struct dirent *DirEntry;
	DIR *directory;
	directory = opendir(".");

	//Verify we can create the file for our list of filenames
	int listFile = open("DirectoryList", O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if(listFile < 0) {
		return 1;
	}

	//Read each filename in present directory and write it into listFile
	while((DirEntry = readdir(directory))) {//While our pointer is being moved to the next entry in our directory
		DIR *dirCheck;
		dirCheck = opendir(DirEntry->d_name);
		if(strcmp(DirEntry->d_name, ".") != 0 && strcmp(DirEntry->d_name, "..") != 0 && dirCheck == NULL) {//Check that current entry is a file (not folder) and for extraneous entries "."/".."
			write(listFile, DirEntry->d_name, strlen(DirEntry->d_name));//Write the filename to the log file
			write(listFile, "\n", 1);//Enter a newline to prepare for the next filename
		}
	}
	close (listFile);
	closedir(directory);
	return 0;
}

int makeLogFile(char *logFileName) {//Create a unique file to log events with a timestamp in file name indicating server start time
	strcpy(logFileName, "Server Log - ");
	time_t timeNow = time(NULL);
	sprintf(logFileName + 13, "%s", ctime(&timeNow));
	logFileName[37] = '\0';
	int logFile = open(logFileName, O_CREAT | O_RDWR | O_APPEND, 0644);//Open the file
	dprintf(logFile, "------------------------\n%s------------------------\n\n", ctime(&timeNow));//Attach header to the top of the log file
	close(logFile);
	return logFile;
}

int varPrint(int logOutputMode, char *logFileName, int isError, const char *format, ...) {//Uses vfprintf() to take care of writing conditionally to stdout, stderr, and a log file
	va_list args;
	va_start(args, format);

	//First we print the message to the appropriate output given arguements
	int len = vfprintf((isError == 0 ? stdout : stderr), format, args);

	//Then take care of logging it to file if we need to
	if(logOutputMode == 1) {
		FILE *logFile = fopen(logFileName, "a");//Opens the file based on name
		if(logFile == NULL) {
			fclose(logFile);
			va_end(args);
			fprintf(stderr, "SERVER: Error, couldn't write event to log file. Continuing...");
			return -1;
		}
		va_start(args, format);
		vfprintf(logFile, format, args);
		fclose(logFile);
	}
	va_end(args);
	return len;//Returns the amount of characters printed (including null-terminator), similar to printf() which it emulates
}

int bind_and_listen(const char *service) {//Look at a particular port given and listen on that port for a client
	struct addrinfo hints;
	struct addrinfo *rp, *result;
	int s;

	//Build address data structure
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = 0;

	//Get local address info
	if((s = getaddrinfo(NULL, service, &hints, &result))!= 0) {//Use getaddrinfo() to give us a linked list of addresses
		fprintf( stderr, "stream-talk-server: getaddrinfo: %s\n", gai_strerror(s));
		return -1;
	}

	//Iterate through the address linked list and try to perform passive open on each one
	for(rp = result; rp != NULL; rp = rp->ai_next) {
		if((s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {continue;}
		if(!bind(s, rp->ai_addr, rp->ai_addrlen)) {break;}
		close(s);
	}

	if(rp == NULL) {//If our pointer in linked list is NULL
		perror("stream-talk-server: bind");
		return -1;
	}
	if(listen(s, MAX_PENDING) == -1) {//If listen returns an non-zero (error) value
		perror("stream-talk-server: listen");
		close(s);
		return -1;
	}

	freeaddrinfo(result);
	return s;//Return a value, which will be used to determine if an error occured (s < 0 on = to function call)
}

int isReceiving(int s, fd_set fds, int seconds, int microseconds) {//Wait a given time period for activity on a designated fd
	//Clear the fdset
	FD_ZERO(&fds);
	FD_SET(s, &fds);

	//Reset the timevalues used for select() wait time
	struct timeval tv;
	tv.tv_sec = seconds;
	tv.tv_usec = microseconds;
	
	//Return 1 (true) if we see activity
	if((select(s+1, &fds, NULL, NULL, &tv)) > 0) {return 1;}
	return 0;//Otherwise, we return 0 (false)
}
#endif