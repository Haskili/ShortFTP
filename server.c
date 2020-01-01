#include "sFunctions.h"

int main(int argc, char *argv[]) {
	char buf[MAX_LINE];//Buffer we will use to send(), recv(), and read()
	char fileList[MAX_LIST_LEN * sizeof(char) + 1], logFileName[37];
	const char * SERVER_PORT = argv[1];
	const char * SERVER_PASSWORD = argv[2];
	int s, new_s, size;
    struct sockaddr_in clientAddr;//Holds our client address
	socklen_t clientAddrSize = sizeof(struct sockaddr_in);
	fd_set readfds;//Create a structure to hold file descriptors
    FD_ZERO(&readfds);//Reset the file set

    if(!argv[1] || !argv[2]) {//Check that port and password are given by user
    	fprintf(stderr, "SERVER: Incorrect arguments,\nUSAGE: PORT#, PASSWORD, -[OPTIONS]\n");
    	exit(1);
    }

	//Get all the options asked for, if any - Makes sure options can be selected in any order
	int debugMode = 0, noPassMode = 0, noTimeOutMode = 0, stayUpMode = 0, recoveryMode = 0, printFileMode = 0, logOutputMode = 0;
	if(argc != 3) {
		for(int i = 3; i < argc; i++) {
			if(strcmp(argv[i], "-D") == 0) {debugMode = 1;}//Debug mode for more verbose printing during process
			else if(strcmp(argv[i], "-NP") == 0) {noPassMode = 1;}//No password mode to allow any password to be accepted
			else if(strcmp(argv[i], "-NT") == 0) {noTimeOutMode = 1;}//No timeout mode to allow infinite time for client to choose file on receipt of list
			else if(strcmp(argv[i], "-SU") == 0) {stayUpMode = 1;}//Stay up mode to keep the server up and ready to receive the next client after each transfer process until such time the user exits
			else if(strcmp(argv[i], "-RM") == 0) {recoveryMode = 1;}//Recovers server to continue (resets) after fatal error (IE: Client requesting bad file)
			else if(strcmp(argv[i], "-PF") == 0) {printFileMode = 1;}//Prints file as we read() it into stdout
			else if(strcmp(argv[i], "-LO") == 0) {logOutputMode = 1;}//Logs interactions between server/client
			else if(strcmp(argv[i], "-H") == 0) {printf("SERVER: Usage is as follows - PORT# PASSWORD -[OPTIONS]\n");return 0;}
			else {//Alert client to invalid option and continue
				fprintf(stderr, "SERVER: Invalid option '%s'\nSERVER: The valid options are listed in the README.md, continuing with process\n", argv[i]);
			}
		}
	}

	//Create a log file
	if(logOutputMode == 1) {
		printf("SERVER: Logfile '%s' %s\n", logFileName, makeLogFile(logFileName) > 0 ? "opened. Writing activity to the file.\n" : "couldn't be opened, continuing.\n");
	}

	//Bind socket to local interface and passive open
	if((s = bind_and_listen(SERVER_PORT)) < 0) {
		exit(1);
	}
	if(varPrint(logOutputMode, logFileName, 0, "SERVER: Starting server on port '%s'.\n", SERVER_PORT) == -1) {fprintf(stderr, "SERVER: Error with log file. Please check output.\n");}

	//Go into the main loop that waits for a potential client, this loop will allow us to receive multiple clients consecutively
	while (1) {
	    //Try to accept a potential client on new_s
		if((new_s = accept(s, (struct sockaddr *)&clientAddr, &clientAddrSize)) < 0) {
			fprintf(stderr, "SERVER: Error accepting new client on new_s\n");
			close(s);
			exit(1);
		}

	    //Print appropriate message for finding a potential client
	    getpeername(new_s, (struct sockaddr *)&clientAddr, &clientAddrSize);//Get the adress of the client trying to connect to server
	    if(debugMode == 1) {
	    	time_t timeNow = time(NULL);
	    	varPrint(logOutputMode, logFileName, 0, "SERVER: We found a potential client with address of '%s' - %s", inet_ntoa(clientAddr.sin_addr), ctime(&timeNow));
	    } 
	    else {varPrint(logOutputMode, logFileName, 0, "SERVER: We found a potential client with address of '%s'\n", inet_ntoa(clientAddr.sin_addr));}

		//Grab the password from the client
		if(isReceiving(new_s, readfds, 0, 500000) == 1) {
			memset(buf, 0, MAX_LINE);
			recv(new_s, buf, MAX_LINE, 0);
		}
		else {
			varPrint(logOutputMode, logFileName, 1, "SERVER: Error receiving the client's password. Possible issue on either end.");
			if(recoveryMode == 1) {goto endClientInteraction;}
			close(new_s);
			close(s);
			return 1;
		}

		//Verify the password we got from client
		if(strcmp(buf, SERVER_PASSWORD) != 0 && noPassMode == 0) {//Verify the password given by client matches what's in argv[2]
			varPrint(logOutputMode, logFileName, 0, "SERVER: Bad password given, '%s', looking for '%s'. Terminating connection with client.\n", buf, SERVER_PASSWORD);
			send(new_s, "BAD", 3, 0);
			if(recoveryMode == 1) {goto endClientInteraction;}
			close(new_s);
			close(s);
			return 0;
		}

		//Alert server of good password as well as send the response with timeout settings to client 
		varPrint(logOutputMode, logFileName, 0, "SERVER: Valid password from client, sending a good response and waiting for them to accept the list\n");
		noTimeOutMode == 0 ? send(new_s, "VALID-TS", 8, 0) : send(new_s, "VALID-NT", 8, 0);
		
		//Receive the client response on whether they want to receive the list (it could be massive depending on directory)
        memset(buf, 0, MAX_LINE);
		recv(new_s, buf, 1, 0);

		//Alert the Server to the response from client
		if(strcmp(buf, "y") == 0 || strcmp(buf, "Y") == 0) {//Client responded 'yes'
			varPrint(logOutputMode, logFileName, 0, "SERVER: The client accepted the list, sending the file name list\n");
		}
		else if(strcmp(buf, "n") == 0 || strcmp(buf, "N") == 0) {//Client responded 'no'
			varPrint(logOutputMode, logFileName, 0, "SERVER: The client denied the list. Terminating connection with client.\n");
			if(recoveryMode == 1) {goto endClientInteraction;}
			close(new_s);
			close(s);
			return 0;
		}
		else {//Response was invalid - likely premature termination of connection from client
			varPrint(logOutputMode, logFileName, 1, "SERVER: The client responded with invalid input, likely premature disconnection. Terminating connection with client.\n");
			if(recoveryMode == 1) {goto endClientInteraction;}
			close(new_s);
			close(s);
			return 0;
		}

		//We can make the assumption that the client responded "y" from here, so we create the list
		if(createList() != 0) {//createList() returns 1 for errors
			varPrint(logOutputMode, logFileName, 1, "SERVER: Couldn't create/open the list. Terminating connection with client.\n");
			if(recoveryMode == 1) {goto endClientInteraction;}
			close(new_s);
			close(s);
			exit(1);
	    }

	    //Attempt to open newly create file with list
	    int listFile = open("DirectoryList", O_RDWR);
		if(listFile < 0) {//Verify we can open the file that contains our list of files in directory
			varPrint(logOutputMode, logFileName, 1, "SERVER: Error opening list file after creating it. Terminating connection with client.\n");
			send(new_s, "Unable to open file.", 20, 0);//Alert client to error
			if(recoveryMode == 1) {goto endClientInteraction;}
			close(new_s);
			close(s);
			exit(1);
		}

		//Send list of file names to the client so they can choose from that list
		if(debugMode == 1) {printf("SERVER: - Start list -\n\n");}
		while((size = read(listFile, buf, MAX_SIZE)) != 0) {
			buf[MAX_LINE - 1] = '\0';
			send(new_s, buf, size, 0);//Send buffer to client as it is modified by read()
			if(debugMode == 1) {write(1, buf, size);}//Write the buffer as we update it with read() into stdout if debug on
		}
		if(debugMode == 1) {printf("\nSERVER: - End list -\n");}
		close(listFile);
		remove("DirectoryList");//Remove the the list of files we just sent to client, now that it's not needed anymore

		//Wait up to 60 seconds or forever if -NT is set for client to send us their filename choice
		int timeoutVal = 0;
		varPrint(logOutputMode, logFileName, 0, "%s", noTimeOutMode == 0 ? "SERVER: Waiting 1 minute for a request response from client.\n" : "SERVER: Waiting for a request response from client.\n");
		while(1) {
			//If we're receiving anything on new_s within the next 10 seconds, recv() it and break out
			if(isReceiving(new_s, readfds, 10, 0) == 1) {
				//Make a string to hold this new file list
				memset(fileList, 0, MAX_LIST_LEN);		
				recv(new_s, fileList, MAX_LIST_LEN, 0);
				if(debugMode == 0) {varPrint(logOutputMode, logFileName, 0, "SERVER: We received a request list from the client\n\n");}
				else {varPrint(logOutputMode, logFileName, 0, "SERVER: We received a request list of '%s' from client\n\n", fileList);}
				break;
			}

			//Otherwise, we increment the timeout value and alert the server appropriately (and log it depending out noTimeOutMode setting)
			timeoutVal++;
			if(noTimeOutMode == 0) {varPrint(logOutputMode, logFileName, 0, "SERVER: Client hasn't sent a file list yet. %i seconds remaining until client is dropped.\n", ((6-timeoutVal)*10));}
			else {varPrint(logOutputMode, logFileName, 0, "SERVER: Client hasn't sent a file list yet. %i seconds have passed.\n", (timeoutVal*10));}

			//If the user didn't set NT and it's been 60 seconds, conditionally leave the loop
			if(noTimeOutMode == 0 && timeoutVal == 6) {
				varPrint(logOutputMode, logFileName, 1, "SERVER: We didn't receive a filename in alloted time. Terminating connection with client.\n");		
				if(recoveryMode == 1) {goto endClientInteraction;}
				close(new_s);
				close(s);
				return 0;
			}
		}

		//Check to make sure we didn't receive an empty request list
		if(atoi(fileList) == 0) {
			varPrint(logOutputMode, logFileName, 0, "SERVER: Client didn't select any files in their list. Terminating connection with client.\n");
			if(recoveryMode == 1) {goto endClientInteraction;}
			close(new_s);
			close(s);
			return 0;
		}
		
		//Setup structures to use list we received from client
		char curFile[MAX_LINE];
		char *ptr = strtok(fileList, ";;");
		ptr = strtok(NULL, ";;");
		
		//Go through the transfer process with each file in the list we just received (Check the name against files in PWD -> Open file -> Send file -> SHA1 Check)
		while(ptr != NULL) {
			//Set the curFile equal to the file in the list we're current at as defined by ptr
			strcpy(curFile, ptr);
			curFile[strlen(ptr)] = '\0';
			varPrint(logOutputMode, logFileName, 0, "SERVER: Processing the request for file '%s'\n", curFile);

			//Try to open the requested file
			int requestedFile = open(curFile, O_RDWR);

			//If we failed to open the requested file, send a bad response "n" to client
			if(requestedFile < 0) {
				varPrint(logOutputMode, logFileName, 1, "SERVER: Error opening requested file. Terminating connection with client.\n");
				send(new_s, "n", 1, 0);//Alert client to error
				if(recoveryMode == 1) {goto endClientInteraction;}
				close(new_s);
				close(s);
				exit(1);
			}

			//Check this specific file against all files in folder and see if the requested file matches one of them as a final security check
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
				varPrint(logOutputMode, logFileName, 1, "SERVER: Error finding requested file '%s'. Terminating connection with client.\n", curFile);
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

			//Send the file data to the client as we read() it into buf
			if(printFileMode == 1) {varPrint(logOutputMode, logFileName, 0, "SERVER: - Start file -\n\n");}
			int bytes = 0;
			while((size = read(requestedFile, buf, MAX_SIZE)) != 0) {
				buf[MAX_LINE - 1] = '\0';
				send(new_s, buf, size, 0);
				SHA1_Update(&ctx, buf, size);//Update the SHA1 as buf keeps getting read() into
				bytes += size;//Increment bytes by size of packet sent each time
				if(printFileMode == 1) {write(1, buf, size);}//Print contents of buf to stdout each run of read()
			}
			if(printFileMode == 1) {varPrint(logOutputMode, logFileName, 0, "\n\nSERVER: - End file -\n");}
			varPrint(logOutputMode, logFileName, 0, "SERVER: Finished sending file to client. %i total bytes sent.\n", bytes);
			if(printFileMode == 1) {varPrint(logOutputMode, logFileName, 0, "\n\n");}
			close(requestedFile);

			//Build structure for the file hash and use SHA1_Final()
			unsigned char fileHash[SHA_DIGEST_LENGTH];//Reminder: SHA_DIGEST_LENGTH = 20 bytes
			SHA1_Final(fileHash, &ctx);

			//Use sprintf() to put the file hash into a workable string
			char serverSHA1[SHA_DIGEST_LENGTH*2];
			for(int pos = 0; pos < SHA_DIGEST_LENGTH; sprintf((char*)&(serverSHA1[pos*2]), "%02x", fileHash[pos]), pos++);

			//Now we receive the SHA1 the client got to check against it as a check
			varPrint(logOutputMode, logFileName, 0, "SERVER: Waiting for the client's SHA1 to check against our local SHA1\n");
			while(1) {
				if(isReceiving(new_s, readfds, 0, 500000) == 1) {//If we're receiving data from client on new_s
					memset(buf, 0, MAX_LINE);
					recv(new_s, buf, MAX_LINE, 0);

					//Check if our file SHA1 is the same as client's
					if(strcmp(buf, serverSHA1) == 0) {
						memset(buf, 0, MAX_LINE);
						send(new_s, "y", 1, 0);
						varPrint(logOutputMode, logFileName, 0, "SERVER: The client's SHA1 matched. Sending good response message and moving on...\n\n");
						break;
					}
					else {
						varPrint(logOutputMode, logFileName, 1, "SERVER: The client's SHA1 '%s' didn't match our SHA1 '%s'. Sending error message and terminating connection with client.\n\n", buf, serverSHA1);
						send(new_s, "n", 1, 0);
						if(recoveryMode == 1) {goto endClientInteraction;}
						close(new_s);
						close(s);
						exit(1);
					}
				}
			}
			ptr = strtok(NULL, ";;");//Increment the ptr to go to the next token (filename) in fileList
		}
	
		//We reached the end of an interaction with a particular client, we now need to act based on the settings the User has selected with their options (IE: SU)
		endClientInteraction: ;//Designated point for server to go to in recovery mode

		//We need to ask if SU is off, in which case we need to get User to decide how to proceed 
		if(stayUpMode == 0) {
			varPrint(logOutputMode, logFileName, 0, "SERVER: We finished with our current client, would you like to stay up for the next client? (yes = y, no = n)\n");
			while(1) {
				memset(buf, 0, MAX_LINE);
				fgets(buf, MAX_LINE, stdin);
				buf[1] = '\0';//Correction for new line on fgets
				if(strcmp(buf, "y") == 0 || strcmp(buf, "Y") == 0) {
					varPrint(logOutputMode, logFileName, 0, "SERVER: User chose to continue with '%s'...\n", buf);
					break;
				}
				else if (strcmp(buf, "n") == 0 || strcmp(buf, "N") == 0) {
					varPrint(logOutputMode, logFileName, 0, "SERVER: User terminated with '%s'. Ending processes.\n", buf);
					close(new_s);
					close(s);
					return 0;
				}
				else {varPrint(logOutputMode, logFileName, 1, "SERVER: Invalid input, please type 'y' for yes, or 'n' for no\n");}
			}
		}

		//Server would've returned/exited by this point based on if-statement conditions, so we alert them that we're restarting (going back to start of while-loop)
		varPrint(logOutputMode, logFileName, 0, "\nSERVER: Restarting the server and listening for a connection on port '%s' for a new client.\n", SERVER_PORT);
	}
	close(new_s);
	close(s);
	return 0;
}