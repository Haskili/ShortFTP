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

#define MAX_LINE 256
#define MAX_PENDING 5
#define MAX_SIZE 30

int createList() {//Creates a file to store list of files in directory server is started, itself not included
	struct dirent* DirEntry;
	DIR* directory;
	directory = opendir(".");

	//Verify we can open/create the file for our list of filenames
	int listFile = open("DirectoryList", O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if(listFile < 0) {
		return 1;
	}

	//Read each filename in present directory and write it into listFile
	while((DirEntry = readdir(directory))) {
		if(strcmp(DirEntry->d_name, ".") != 0 && strcmp(DirEntry->d_name, "..") != 0) {//Check for extraneous entries
			write(listFile, DirEntry->d_name, strlen(DirEntry->d_name));
			write(listFile, "\n", 1);
		}
	}
	close (listFile);
	closedir(directory);
	return 0;
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

	//Iterate through the address list and try to perform passive open on each one
	for(rp = result; rp != NULL; rp = rp->ai_next) {
		if((s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
			continue;
		}
		if(!bind(s, rp->ai_addr, rp->ai_addrlen)) {
			break;
		}
		close(s);
	}

	if(rp == NULL) {
		perror("stream-talk-server: bind");
		return -1;
	}
	if(listen(s, MAX_PENDING) == -1) {
		perror("stream-talk-server: listen");
		close(s);
		return -1;
	}

	freeaddrinfo(result);
	return s;//Return a value, which will be used to determine if an error occured (s < 0 on = to function call)
}

int isReceiving(int s, fd_set fds, int seconds, int microseconds) {//Returns 0 if we're not receiving anything within time limit
	//Clear the fdset
	FD_ZERO(&fds);
	FD_SET(s, &fds);

	//Reset the timevalues used for select() wait time
	struct timeval tv;
	tv.tv_sec = seconds;
	tv.tv_usec = microseconds;
	
	//Return 1 if we see activity
	if((select(s+1, &fds, NULL, NULL, &tv)) > 0) {
		return 1;
	}
	return 0;//Otherwise, we return 0 (false) if we don't get anything within time period
}

int main(int argc, char *argv[]) {
	char buf[MAX_LINE];//Buffer used throughout the entire process to receive and send to client
	int s, new_s, size, len;
	const char * SERVER_PORT = argv[1];
	const char * SERVER_PASSWORD = argv[2];
    fd_set readfds;//Create a structure to hold file descriptors
    FD_ZERO(& readfds);//Reset file set
    struct sockaddr_in clientAddr;//Holds our client address
	socklen_t clientAddrSize = sizeof(struct sockaddr_in);

    if(!argv[1] || !argv[2]) {//Check that port and password are given by user
    	printf("SERVER ERROR: Incorrect arguments,\nUSAGE: PORT#, PASSWORD, -[OPTIONS]\n");
    	exit(1);
    }

	//Get all the options asked for, if any - Makes sure options can be selected in any order
	int debugMode = 0;
	int noPassMode = 0; 
	int noTimeOutMode = 0; 
	int stayUpMode = 0;
	if(argc != 3) {
		for(int i = 3; i < argc; i++) {
			if(strcmp(argv[i], "-D") == 0) {debugMode = 1;}//Debug mode for more verbose printing during process
			else if(strcmp(argv[i], "-NP") == 0) {noPassMode = 1;}//No password mode to allow any password to be accepted
			else if(strcmp(argv[i], "-NT") == 0) {noTimeOutMode = 1;}//No timeout mode to allow infinite time for client to choose file on receipt of list
			else if(strcmp(argv[i], "-SU") == 0) {stayUpMode = 1;}//Stay up mode to keep the server up and ready to receive the next client after each transfer process until such time the user exits
			else {//Alert client to invalid option and continue
				printf("SERVER: Invalid option '%s'\n", argv[i]);
				printf("SERVER: The valid options are '-D' for debugging mode, '-NP' for no password mode, and '-NT' for no timeout in requesting file\nSERVER: Continuing with process\n");
			}
		}
	}

	//Bind socket to local interface and passive open
	if((s = bind_and_listen(SERVER_PORT)) < 0) {
		exit(1);
	}

	//Wait for connection
	while (1) {
	    //Try to accept a client new_s on s
		if((new_s = accept(s, (struct sockaddr *)&clientAddr, &clientAddrSize)) < 0) {
			perror("stream-talk-server: accept");
			close(s);
			exit(1);
		}
	    
	    //Print appropriate message for finding a potential client
	    if(debugMode == 1) {
	    	getpeername(new_s, (struct sockaddr *)&clientAddr, &clientAddrSize);//Might need to switch to getsockname() in future
	    	printf("SERVER: We found a potential client with address of '%s'\n", inet_ntoa(clientAddr.sin_addr));
	    }
	    else {printf("SERVER: We found a potential client\n");}

		while((len = recv(new_s, buf, sizeof(buf), 0))) {
			if(len < 0) {
				perror("streak-talk-server: recv");
				close(s);
				exit(1);
			}

			if(strcmp(buf, SERVER_PASSWORD) != 0 && noPassMode == 0) {//Verify the password given by client matches what's in argv[2]
				printf("SERVER: Bad password given, '%s'. Terminating.\n", buf);
				send(new_s, "Bad password given", 18, 0);
				close(new_s);
				close(s);
				exit(1);
			}
			printf("SERVER: Good password from client\n");

			//Alert client of good password and client will accept or deny the list (it could be massive depending on directory)
			printf("SERVER: Asking client to accept list\n");
			send(new_s, "y", 1, 0);//Alert the client that the password was accepted

			//Receive the response of the client on whether they want the list
            memset(buf, 0, MAX_LINE);
			recv(new_s, buf, 1, 0);

			//If the client didn't want the list
			if(strcmp(buf, "n") == 0) {
				printf("SERVER: The client denied the file, exiting.\n");
				close(new_s);
				close(s);
				return 0;
			}

			//We can make the assumption that the client responded "y" from here because it wasn't "n" and client.c ensures valid input
			if(createList() != 0) {//createList() returns 1 for errors
				printf("SERVER: Couldn't create/open the list file. Terminating.\n");
				close(s);
				exit(1);
		    }

		    //Attempt to open newly create file with list
		    int listFile = open("DirectoryList", O_RDWR);
			if(listFile < 0) {//Verify we can open the file that contains our list of files in directory
				printf("SERVER: Error opening list file. Terminating.\n");
				send(new_s, "Unable to open file.", 20, 0);//Alert client to error
				exit(1);
			}

		    //Send list file to the client
			printf("SERVER: The client accepted the list, sending...\n");
			if(debugMode == 1) {printf("SERVER: - Start list -\n\n");}
			while((size = read(listFile, buf, MAX_SIZE)) != 0) {
				buf[MAX_LINE - 1] = '\0';
				send(new_s, buf, size, 0);//Send buffer to client as it is modified by read()
				if(debugMode == 1) {write(1, buf, size);}//Write the buffer as we update it with read() into stdout if debug on
			}
			if(debugMode == 1) {printf("\nSERVER: - End list -\n");}
			close (listFile);
			remove("DirectoryList");

			//Wait up to 30 seconds or forever if -NT is set for client to send us their filename choice
			int timeoutVal = 0;
			int gotRequest = 0;
			if(noTimeOutMode == 0) {printf("SERVER: List was sent to the client, waiting 30 seconds for a response.\n");}
			else if(noTimeOutMode == 1) {printf("SERVER: List was sent to the client, waiting for a response.\n");}	
			while(1) {
				//If we're receiving anything on new_s within the next 10 seconds, recv() it and break out
				if(isReceiving(new_s, readfds, 10, 0) == 1) {
					memset(buf, 0, MAX_LINE);
					recv(new_s, buf, MAX_LINE, 0);
					printf("SERVER: We received a request for file '%s'\n", buf);
					gotRequest = 1;
					break;
				}

				//If we didn't set no timeout and it's been 30 seconds, break out of loop
				if(noTimeOutMode == 0 && timeoutVal == 2) {break;}//The timeoutVal starts at 0 and increments after each 10 seconds of no information on new_s, so at '== 2' 30 seconds have passed

				//Else we increment the timeout value and alert the server appropriately
				timeoutVal++;
				if(noTimeOutMode == 0) {printf("SERVER: Client hasn't sent a filename yet. %i seconds remaining.\n", ((3-timeoutVal)*10));}
				else if(noTimeOutMode == 1) {printf("SERVER: Client hasn't sent a filename yet. %i seconds have passed.\n", (timeoutVal*10));}
			}
			
			//If we timed out of our 'get filename request' loop without getting a file name request
			if(noTimeOutMode == 0 && gotRequest != 1) {
				printf("SERVER: We didn't receive a filename in alloted time, terminating.\n");
				close(new_s);
				close(s);
				exit(1);
			}

			//Try to open the requested file
			int requestedFile = open(buf, O_RDWR);

			//If we failed to open the requested file, send a bad response "n" to client
			if(requestedFile < 0) {
				printf("SERVER: Error opening requested file. Terminating.\n");
				send(new_s, "n", 1, 0);//Alert client to error
				close(new_s);
				close(s);
				exit(1);
			}

			//Now we can make the string for the system() call
			char md5Command[MAX_LINE];//String that we will use for our system() call
			char serverMD5[MAX_LINE];//String to hold result of the server MD5 on the original file asked for
			strcpy(md5Command, "md5sum ");//Prep the system() call string
			strcat(md5Command, buf);//Get the filename requested and put it into the command string
			strcat(md5Command, " | tee -a serverTemp");

			//Check against all files in folder and see if the requested file matches one of them as a final security check
			struct dirent* DirEntry;
			DIR* directory;
			directory = opendir(".");
			int gotFile = 0;
			while((DirEntry = readdir(directory))) {
				if(strcmp(DirEntry->d_name, buf) == 0) {//If the name of the pointer to our current file in directory is file requested
					gotFile = 1;
					break;
				}
			}
			closedir(directory);

			//If we didn't find the file requested withing our current directory, alert client and terminate connection
			if(gotFile != 1) {
				printf("SERVER: Error finding requested file '%s'. Terminating.\n", buf);
				send(new_s, "n", 1, 0);//Alert client to error
				close(new_s);
				close(s);
				exit(1);
			}

			//We see the requested file is in our PWD and can open it, return a good response "y" to client and send the data afterwards
			printf("SERVER: Sending file to client\n");
			send(new_s, "y", 1, 0);
			if(debugMode == 1) {printf("\nSERVER: - Start file -\n\n");}
			int bytes = 0;
			while((size = read(requestedFile, buf, MAX_SIZE)) != 0) {
				buf[MAX_LINE - 1] = '\0';
				send(new_s, buf, size, 0);
				bytes += size;//Increment bytes by size of packet sent each time
				if(debugMode == 1) {write(1, buf, size);}//Print contents of buf to stdout each run of read
			}
			if(debugMode == 1) {printf("\n\nSERVER: - End file -\n");}
			printf("SERVER: Finished sending file to client. %i total bytes sent.\n", bytes);
			close(requestedFile);

			//We get the md5 of our file and save it for the next step
			printf("SERVER: Grabbing the md5sum for our file\n\n");
			int serverTemp = open("serverTemp", O_CREAT | O_RDWR | O_TRUNC, 0644);//Create a temporary file
			system(md5Command);//Execute the md5sum on our file with output appended to our temporary file
			read(serverTemp, serverMD5, 32);//Read the result from the file in our string
			serverMD5[32] = '\0';
			
			//Close the temporary file and delete it now that we are done with the MD5 creation
			close(serverTemp);
			remove("serverTemp");

			//Now we receive the md5 the client got to check against it as a check
			printf("\nSERVER: Waiting for the client's md5sum to check against\n");
			while(1) {
				if(isReceiving(new_s, readfds, 0, 500000) == 1) {//If we're receiving data from client on new_s
					memset(buf, 0, MAX_LINE);
					recv(new_s, buf, MAX_LINE, 0);

					//Check if our file md5 is the same as client's
					if(strcmp(buf, serverMD5) == 0) {
						memset(buf, 0, MAX_LINE);
						send(new_s, "y", 1, 0);
						printf("SERVER: The client's md5 matched. Sending good response message and terminating connection with client.\n");
						break;
					}
					else {
						printf("SERVER: The client's md5 '%s' didn't match our '%s'. Sending error message and terminating.\n", buf, serverMD5);
						send(new_s, "n", 1, 0);
						close(new_s);
						close(s);
						exit(1);
					}
				}
			}

			//Ask the user if they wish to continue to receive new clients if we haven't set it to stay up after each run
			if(stayUpMode == 0) {
				printf("SERVER: We finished sending the file to our client, would you like to stay up for the next client?\n");
				while(1) {
					memset(buf, 0, MAX_LINE);
					fgets(buf, MAX_LINE, stdin);
					buf[1] = '\0';//Correction for new line on fgets getting filename
					if(strcmp(buf, "y") == 0 || strcmp(buf, "Y") == 0) {
						printf("SERVER: Restarting the server and listening for a connection on port '%s' for a new client.\n", SERVER_PORT);
						break;
					}
					else if (strcmp(buf, "n") == 0 || strcmp(buf, "N") == 0) {
						printf("SERVER: Ending server processes.\n");
						close(s);
						return 0;
					}
					else {printf("SERVER: Invalid input: '%s', please type 'y' for yes, or 'n' for no\n", buf);}//User is alerted to invalid input and we will continue until valid input
				}
			}
		}

	}
	close(new_s);
	close(s);
	return 0;
}