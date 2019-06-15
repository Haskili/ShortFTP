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

#define MAX_LINE 256
#define MAX_PENDING 5
#define MAX_SIZE 30

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
		if((s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
			continue;
		}
		if(!bind(s, rp->ai_addr, rp->ai_addrlen)) {
			break;
		}
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
	if((select(s+1, &fds, NULL, NULL, &tv)) > 0) {
		return 1;
	}
	return 0;//Otherwise, we return 0 (false) if we don't get anything within time period
}

int main(int argc, char *argv[]) {
	char buf[MAX_LINE];//Buffer we will use to send(), recv(), and read()
	int s, new_s, size;
	const char * SERVER_PORT = argv[1];
	const char * SERVER_PASSWORD = argv[2];
    struct sockaddr_in clientAddr;//Holds our client address
	socklen_t clientAddrSize = sizeof(struct sockaddr_in);
	fd_set readfds;//Create a structure to hold file descriptors
    FD_ZERO(& readfds);//Reset the file set

    if(!argv[1] || !argv[2]) {//Check that port and password are given by user
    	fprintf(stderr, "SERVER: Incorrect arguments,\nUSAGE: PORT#, PASSWORD, -[OPTIONS]\n");
    	exit(1);
    }

	//Get all the options asked for, if any - Makes sure options can be selected in any order
	int debugMode = 0;
	int noPassMode = 0; 
	int noTimeOutMode = 0; 
	int stayUpMode = 0;
	int recoveryMode = 0;
	int printFileMode = 0;
	int logOutputMode = 0;
	if(argc != 3) {
		for(int i = 3; i < argc; i++) {
			if(strcmp(argv[i], "-D") == 0) {debugMode = 1;}//Debug mode for more verbose printing during process
			else if(strcmp(argv[i], "-NP") == 0) {noPassMode = 1;}//No password mode to allow any password to be accepted
			else if(strcmp(argv[i], "-NT") == 0) {noTimeOutMode = 1;}//No timeout mode to allow infinite time for client to choose file on receipt of list
			else if(strcmp(argv[i], "-SU") == 0) {stayUpMode = 1;}//Stay up mode to keep the server up and ready to receive the next client after each transfer process until such time the user exits
			else if(strcmp(argv[i], "-RM") == 0) {recoveryMode = 1;}//Recovers server to continue (resets) after fatal error (IE: Client requesting bad file)
			else if(strcmp(argv[i], "-PF") == 0) {printFileMode = 1;}//Prints file as we read() it into stdout
			else if(strcmp(argv[i], "-LO") == 0) {logOutputMode = 1;}//Logs interactions between server/client
			else {//Alert client to invalid option and continue
				fprintf(stderr, "SERVER: Invalid option '%s'\nSERVER: The valid options are listed in the README.md, continuing with process\n", argv[i]);
			}
		}
	}

	//Bind socket to local interface and passive open
	if((s = bind_and_listen(SERVER_PORT)) < 0) {
		exit(1);
	}
	printf("SERVER: Starting server on port '%s'.\n", SERVER_PORT);

	//Create a log file
	char logFileName[37];//Holds name of log file, used throughout entire process for accessing log
	if(logOutputMode == 1) {
		printf("SERVER: Logfile '%s' %s\n", logFileName, makeLogFile(logFileName) > 0 ? "opened. Writing activity to the file.\n" : "couldn't be opened. Continuing.\n");
		int logFile = open(logFileName, O_RDWR | O_APPEND); 
		dprintf(logFile, "SERVER: Starting server on port '%s'.\n", SERVER_PORT); 
		close(logFile);
	}

	//Wait for connection
	while (1) {
	    //Try to accept a potential client on new_s
		if((new_s = accept(s, (struct sockaddr *)&clientAddr, &clientAddrSize)) < 0) {
			perror("stream-talk-server: accept");
			close(s);
			exit(1);
		}

	    //Print appropriate message for finding a potential client
	    if(debugMode == 1) {
	    	getpeername(new_s, (struct sockaddr *)&clientAddr, &clientAddrSize);//Might need to switch to getsockname() in future
	    	printf("SERVER: We found a potential client with address of '%s'\n", inet_ntoa(clientAddr.sin_addr));
	    	if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: We found a potential client with address of '%s'\n", inet_ntoa(clientAddr.sin_addr)); close(logFile);}
	    }
	    else {printf("SERVER: We found a potential client\n"); if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: We found a potential client\n"); close(logFile);}}

		//Grab the password from the client (assuming it is sending it correctly upon connection)
		if(isReceiving(new_s, readfds, 0, 500000) == 1) {
			memset(buf, 0, MAX_LINE);
			recv(new_s, buf, MAX_LINE, 0);
		}

		if(strcmp(buf, SERVER_PASSWORD) != 0 && noPassMode == 0) {//Verify the password given by client matches what's in argv[2]
			fprintf(stderr, "SERVER: Bad password given, '%s', looking for '%s'. Terminating connection with client.\n", buf, SERVER_PASSWORD);
			if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: Bad password given, '%s'. Terminating connection with client.\n", buf); close(logFile);}
			send(new_s, "BAD", 3, 0);
			if(recoveryMode == 1) {goto endClientInteraction;}
			close(new_s);
			close(s);
			exit(1);
		}

		//Alert server of good password as well as send the response with timeout settings to client 
		printf("SERVER: Valid password from client, sending a good response and waiting for them to accept the list\n");
		if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: Valid password from client, sending a good response and waiting for them to accept the list\n"); close(logFile);}
		noTimeOutMode == 0 ? send(new_s, "VALID-TS", 8, 0) : send(new_s, "VALID-NT", 8, 0);
		
		//Receive the client response on whether they want to receive the list (it could be massive depending on directory)
        memset(buf, 0, MAX_LINE);
		recv(new_s, buf, 1, 0);

		//If the client didn't want the list
		if(strcmp(buf, "n") == 0) {
			printf("SERVER: The client denied the list. Terminating connection with client.\n");
			if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: The client denied the list. Terminating connection with client.\n"); close(logFile);}
			if(recoveryMode == 1) {goto endClientInteraction;}
			close(new_s);
			close(s);
			return 0;
		}

		//We can make the assumption that the client responded "y" from here because it wasn't "n" and client.c ensures valid input
		if(createList() != 0) {//createList() returns 1 for errors
			fprintf(stderr, "SERVER: Couldn't create/open the list. Terminating connection with client.\n");
			if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: Couldn't create/open the list. Terminating connection with client.\n"); close(logFile);}
			if(recoveryMode == 1) {goto endClientInteraction;}
			close(new_s);
			close(s);
			exit(1);
	    }

	    //Attempt to open newly create file with list
	    int listFile = open("DirectoryList", O_RDWR);
		if(listFile < 0) {//Verify we can open the file that contains our list of files in directory
			fprintf(stderr, "SERVER: Error opening list file. Terminating connection with client.\n");
			if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: Error opening list file. Terminating connection with client.\n"); close(logFile);}
			send(new_s, "Unable to open file.", 20, 0);//Alert client to error
			if(recoveryMode == 1) {goto endClientInteraction;}
			close(new_s);
			close(s);
			exit(1);
		}

	    //Send list of file names to the client
		printf("SERVER: The client accepted the list, sending the file name list\n");
		if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: The client accepted the list, sending the file name list\n"); close(logFile);}
		if(debugMode == 1) {printf("SERVER: - Start list -\n\n");}
		while((size = read(listFile, buf, MAX_SIZE)) != 0) {
			buf[MAX_LINE - 1] = '\0';
			send(new_s, buf, size, 0);//Send buffer to client as it is modified by read()
			if(debugMode == 1) {write(1, buf, size);}//Write the buffer as we update it with read() into stdout if debug on
		}
		if(debugMode == 1) {printf("\nSERVER: - End list -\n");}
		close(listFile);
		remove("DirectoryList");

		//Wait up to 60 seconds or forever if -NT is set for client to send us their filename choice
		int timeoutVal = 0;
		int gotRequest = 0;
		printf("%s", noTimeOutMode == 0 ? "SERVER: Waiting 1 minute for a request response from client.\n" : "SERVER: Waiting for a request response from client.\n");
		if(logOutputMode == 1) {
			int logFile = open(logFileName, O_RDWR | O_APPEND);
			dprintf(logFile, "%s\n", noTimeOutMode == 0 ? "SERVER: Waiting 1 minute for a request response from client.\n" : "SERVER: Waiting for a request response from client.\n");
			close(logFile);
		}
		while(1) {
			//If we're receiving anything on new_s within the next 10 seconds, recv() it and break out
			if(isReceiving(new_s, readfds, 10, 0) == 1) {
				memset(buf, 0, MAX_LINE);
				recv(new_s, buf, MAX_LINE, 0);
				debugMode == 0 ? printf("SERVER: We received a request list from the client\n\n") : printf("SERVER: We received a request list of '%s' from client\n\n", buf);
				gotRequest = 1;
				break;
			}

			//Otherwise, we increment the timeout value and alert the server appropriately
			timeoutVal++;
			if(noTimeOutMode == 0) {printf("SERVER: Client hasn't sent a file list yet. %i seconds remaining until client is dropped.\n", ((6-timeoutVal)*10));}
			if(noTimeOutMode == 1) {printf("SERVER: Client hasn't sent a file list yet. %i seconds have passed.\n", (timeoutVal*10));}
			if(logOutputMode == 1) {
				int logFile = open(logFileName, O_RDWR | O_APPEND);
				if(noTimeOutMode == 0) {dprintf(logFile, "SERVER: Client hasn't sent a file list yet. %i seconds remaining until client is dropped.\n", ((6-timeoutVal)*10));}
				if(noTimeOutMode == 1) {dprintf(logFile, "SERVER: Client hasn't sent a file list yet. %i seconds have passed.\n", (timeoutVal*10));}
				close(logFile);
			}

			//If the user didn't set NT and it's been 60 seconds, break out of loop
			if(noTimeOutMode == 0 && timeoutVal == 6) {break;}
		}
		
		//If we timed out of our 'get filename request' loop without getting a file name request
		if(noTimeOutMode == 0 && gotRequest != 1) {
			fprintf(stderr, "SERVER: We didn't receive a filename in alloted time. Terminating connection with client.\n");
			if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: We didn't receive a filename in alloted time. Terminating connection with client.\n"); close(logFile);}
			if(recoveryMode == 1) {goto endClientInteraction;}
			close(new_s);
			close(s);
			exit(1);
		}

		//Check to make sure we didn't receive an empty request list
		if(atoi(buf) == 0) {
			printf("SERVER: Client didn't select any files in their list. Terminating connection with client.\n");
			if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: Client didn't select any files in their list. Terminating connection with client.\n"); close(logFile);}
			if(recoveryMode == 1) {goto endClientInteraction;}
			close(new_s);
			close(s);
			exit(1);
		}
		
		//Setup structures to use list we received from client
		char curList[MAX_LINE], curFile[MAX_LINE];
		strcpy(curList, buf);
		char *ptr = strtok(curList, ";;");
		ptr = strtok(NULL, ";;");
		
		//Check each file name in our list and go through the sending process while there are ";;" in curList
		while(ptr != NULL) {
			//Set the curFile equal to the file in the list we're current at as defined by ptr
			strcpy(curFile, ptr);
			curFile[strlen(ptr)] = '\0';
			printf("SERVER: Processing the request for file '%s'\n", curFile);			
			if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: Processing the request for file '%s'\n", curFile); close(logFile);}

			//Try to open the requested file
			int requestedFile = open(curFile, O_RDWR);

			//If we failed to open the requested file, send a bad response "n" to client
			if(requestedFile < 0) {
				fprintf(stderr, "SERVER: Error opening requested file. Terminating connection with client.\n");
				if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: Error opening requested file. Terminating connection with client.\n"); close(logFile);}
				send(new_s, "n", 1, 0);//Alert client to error
				if(recoveryMode == 1) {goto endClientInteraction;}
				close(new_s);
				close(s);
				exit(1);
			}

			//Check against all files in folder and see if the requested file matches one of them as a final security check
			struct dirent* DirEntry;
			DIR* directory;
			directory = opendir(".");
			int gotFile = 0;
			while((DirEntry = readdir(directory))) {
				if(strcmp(DirEntry->d_name, curFile) == 0) {//If the name of the pointer to our current file in directory is file requested
					gotFile = 1;
					break;
				}
			}
			closedir(directory);

			//If we didn't find the file requested withing our current directory, alert client and terminate connection
			if(gotFile != 1) {
				fprintf(stderr, "SERVER: Error finding requested file '%s'. Terminating connection with client.\n", curFile);
				if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: Error finding requested file '%s'. Terminating connection with client.\n", curFile); close(logFile);}
				send(new_s, "n", 1, 0);//Alert client to error
				if(recoveryMode == 1) {goto endClientInteraction;}
				close(new_s);
				close(s);
				exit(1);
			}

			//We see the requested file is in our PWD and can open it, return a good response "y" to client
			send(new_s, "y", 1, 0);

			//Build structure for the SHA1 and initalize it
    		SHA_CTX ctx;
    		SHA1_Init(&ctx);

			//Send the data to the client as we read() it into buf
			if(printFileMode == 1) {printf("SERVER: - Start file -\n\n");}
			int bytes = 0;
			while((size = read(requestedFile, buf, MAX_SIZE)) != 0) {
				buf[MAX_LINE - 1] = '\0';
				send(new_s, buf, size, 0);
				SHA1_Update(&ctx, buf, size);//Update the SHA1 as buf keeps getting read() into
				bytes += size;//Increment bytes by size of packet sent each time
				if(printFileMode == 1) {write(1, buf, size);}//Print contents of buf to stdout each run of read()
			}
			if(printFileMode == 1) {printf("\n\nSERVER: - End file -\n");}
			printf("SERVER: Finished sending file to client. %i total bytes sent.\n", bytes);
			if(debugMode == 1) {printf("\n\n");}
			if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: Finished sending file to client. %i total bytes sent.\n", bytes); close(logFile);}
			close(requestedFile);

			//Build structure for the file hash and use SHA1_Final()
			unsigned char fileHash[SHA_DIGEST_LENGTH];//Reminder: SHA_DIGEST_LENGTH = 20 bytes
			SHA1_Final(fileHash, &ctx);

			//Use sprintf() to put the file hash into a workable string
			char serverSHA1[SHA_DIGEST_LENGTH*2];
			for (int pos = 0; pos < SHA_DIGEST_LENGTH; pos++) {
		        sprintf((char*)&(serverSHA1[pos*2]), "%02x", fileHash[pos]);
		    }

			//Now we receive the SHA1 the client got to check against it as a check
			printf("SERVER: Waiting for the client's SHA1 to check against our local SHA1\n");
			if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: Waiting for the client's SHA1 to check against our local SHA1\n"); close(logFile);}
			while(1) {
				if(isReceiving(new_s, readfds, 0, 500000) == 1) {//If we're receiving data from client on new_s
					memset(buf, 0, MAX_LINE);
					recv(new_s, buf, MAX_LINE, 0);

					//Check if our file SHA1 is the same as client's
					if(strcmp(buf, serverSHA1) == 0) {
						memset(buf, 0, MAX_LINE);
						send(new_s, "y", 1, 0);
						printf("SERVER: The client's SHA1 matched. Sending good response message and moving on...\n\n");
						if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: The client's SHA1 matched. Sending good response message and moving on...\n\n"); close(logFile);}
						break;
					}
					else {
						fprintf(stderr, "SERVER: The client's SHA1 '%s' didn't match our SHA1 '%s'. Sending error message and terminating connection with client.\n\n", buf, serverSHA1);
						if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: The client's SHA1 '%s' didn't match our SHA1 '%s'. Sending error message and terminating connection with client.\n\n", buf, serverSHA1); close(logFile);}
						send(new_s, "n", 1, 0);
						if(recoveryMode == 1) {goto endClientInteraction;}
						close(new_s);
						close(s);
						exit(1);
					}
				}
			}
			ptr = strtok(NULL, ";;");//Increment the ptr to go to the next token (filename) in curList
		}
	
		//Ask the user if they wish to continue to receive new clients if we haven't set it to stay up after each run with -SU
		endClientInteraction: ;//Designated point for server to recover to in recovery mode, allowing server to keep going after certain errors in process
		if(stayUpMode == 0) {
			printf("SERVER: We finished with our current client, would you like to stay up for the next client? (yes = y, no = n)\n");
			if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: We finished with our current client, would you like to stay up for the next client? (yes = y, no = n)\n"); close(logFile);}
			while(1) {
				memset(buf, 0, MAX_LINE);
				fgets(buf, MAX_LINE, stdin);
				if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "%s", buf); close(logFile);}
				buf[1] = '\0';//Correction for new line on fgets
				if(strcmp(buf, "y") == 0 || strcmp(buf, "Y") == 0) {
					printf("\nSERVER: Restarting the server and listening for a connection on port '%s' for a new client.\n", SERVER_PORT);
					if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "\nSERVER: Restarting the server and listening for a connection on port '%s' for a new client.\n", SERVER_PORT); close(logFile);}
					break;
				}
				else if (strcmp(buf, "n") == 0 || strcmp(buf, "N") == 0) {
					printf("SERVER: User terminated. Ending processes.\n");
					if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: User terminated. Ending processes.\n"); close(logFile);}
					close(new_s);
					close(s);
					return 0;
				}
				else {fprintf(stderr, "SERVER: Invalid input, please type 'y' for yes, or 'n' for no\n");if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "SERVER: Invalid input, please type 'y' for yes, or 'n' for no\n"); close(logFile);}}
			}
		}
		printf("\nSERVER: Restarting the server and listening for a connection on port '%s' for a new client.\n", SERVER_PORT);//Server would've returned by this point, so we alert them to restart
		if(logOutputMode == 1) {int logFile = open(logFileName, O_RDWR | O_APPEND); dprintf(logFile, "\nSERVER: Restarting the server and listening for a connection on port '%s' for a new client.\n", SERVER_PORT); close(logFile);}

	}
	close(new_s);
	close(s);
	return 0;
}