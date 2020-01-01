#include "cFunctions.h"

int main( int argc, char *argv[] ) {
	char *host;//IP address of the server we want to connect to
	char buf[MAX_LINE];//Buffer we will use to send(), recv(), and write()
	char downloadFileStr[MAX_LINE];//String to hold the name of the file that we will create to write downloaded information into
	const char * SERVER_PORT = argv[2];
	char * password = argv[3];
	int s, len;
	host = argv[1];
    fd_set readfds;//Create a fileset for file descriptors
    FD_ZERO(&readfds);//Clear the fileset

    if(!argv[1] || !argv[2] || !argv[3]) {
       fprintf(stderr, "CLIENT ERROR: Incorrect arguments,\nUSAGE: HOST-ADR, PORT#, PASS, -[OPTIONS]\n");
       exit(1);
    }

    //Get all the options if user selected any - Might want to put ints inside of if() and instead ask for each mode operation is argc != 3 so it won't give error
	int debugMode = 0, printFileMode = 0;
	if(argc != 3) {
		for(int i = 4; i < argc; i++) {
			if(strcmp(argv[i], "-D") == 0) {debugMode = 1;}
			else if(strcmp(argv[i], "-PF") == 0) {printFileMode = 1;}
			else if(strcmp(argv[i], "-H") == 0) {printf("CLIENT: Usage is as follows - HOST-ADR, PORT#, PASS, -[OPTIONS]\n");return 0;}
			else {fprintf(stderr, "CLIENT: Invalid option '%s'\nCLIENT: The valid options are listed in the README.md, continuing with process\n", argv[i]);}
		}
	}

	//Lookup IP and connect to server
	if((s = lookup_and_connect(host, SERVER_PORT)) < 0) {
		exit(1);
	}
	FD_SET(s, &readfds);//Add s to list of sockets

	//Receive a verification from server that we gave a valid password
    if(verifyPassword(s, password, buf) == 1) {
    	//Server didn't give good response to password we sent, alert client and exit
    	fprintf(stderr, "CLIENT: Error verifying password with server, terminating.\n");
    	close(s);
    	exit(1);
    }

    //Check buff to see what timeout settings server sent along with our password validation and set ours accordingly
    int serverTimeout = 1;
    if(strcmp(buf, "VALID-NT") == 0) {serverTimeout = 0;}

    //Return response to server on whether user wants to receive the list of files available
	printf("CLIENT: List available from host with address '%s'\nCLIENT: Are you certain you want to download the list from this host? (yes = y, no = n)\n", argv[1]);
	while(1) {
		memset(buf, 0, MAX_LINE);
		fgets(buf, MAX_LINE, stdin);
		buf[1] = '\0';//Correction for new line on fgets getting filename
		if(strcmp(buf, "y") == 0 || strcmp(buf, "Y") == 0) {
			send(s, buf, 1, 0);//Tell server to send the list
			memset(buf, 0, MAX_LINE);//Reset buffer
			break;
		}
		else if (strcmp(buf, "n") == 0 || strcmp(buf, "N") == 0) {
			send(s, buf, 1, 0);//Tell the server to not send the list
			memset(buf, 0, MAX_LINE);
			close(s);
			fprintf(stderr, "CLIENT: Told server not to send list, terminating\n");
			return 0;
		}
		else {fprintf(stderr, "CLIENT: Invalid input: '%s', please type 'y' for yes, or 'n' for no\n", buf);}//User is alerted to invalid input and we will continue until valid input
	}

	//Print the list of files received
	printf("%s", debugMode == 0 ? "\n" : "CLIENT: - Start list -\n\n");
	while((len = recv(s, buf, MAX_LINE, 0))) {
		write(1, buf, len);
		memset(buf, 0, MAX_LINE);
		if(isReceiving(s, readfds, 0, 500000) == 0) {break;}//If we're not receiving anymore information, break out of the loop and continue
	}
	printf("%s", debugMode == 0 ? "\n" : "\nCLIENT: - End list -\n");

	//Build a temporary string to hold the list as we get it from the User and other variables needed
	if(serverTimeout == 1) {printf("CLIENT: The server has given us a minute till the timeout for choosing files.\n");}
	printf("CLIENT: Please choose up to 10 files from the list above and finish by entering a ';;' or an empty line.\n\n");
	char *tempList = malloc(45 * sizeof(char));
	int counter = 0, fileReqAmount = 0;
	fd_set waitFDS;

	//Get the file names from the User
	while(1) {
		//Wait for activity from stdin (0 in first arguement)
		if(isReceiving(0, waitFDS, 1, 0) == 1) {
			memset(buf, 0, MAX_LINE);
			fgets(buf, MAX_LINE, stdin);
			buf[strlen(buf) - 1] = '\0';
			
			//First we check if we got the command to break out of the loop
			if(strcmp(buf, "") == 0 || strcmp(buf, ";;") == 0) {memset(buf, 0, MAX_LINE);break;}

			//Then we check if we can add the entered file name to our list given the length constraint of MAX_LIST_LEN
			if((sizeof(char)*strlen(tempList)) + (sizeof(char)*strlen(buf)) + snprintf(NULL, 0, "%i", fileReqAmount) + 3 < MAX_LIST_LEN) {
				//To add it to the list, we first realloc() the amount we need (+2 for the deliminator, +1 for \0, and +snprintf() for the item amount digits)
				tempList = realloc(tempList, (sizeof(char)*strlen(tempList)) + (sizeof(char)*strlen(buf)) + 3);
				sprintf(tempList + strlen(tempList), ";;%s", buf);
				fileReqAmount++;
			}
			else {fprintf(stderr, "CLIENT: Adding that item to the list would exceede the maximum file list length, breaking out and sending list\n");break;}
		}

		//Increment counter for the seconds in the input loop and check if we need to exit()
		counter++;
		if(counter == 60 && serverTimeout == 1) {//This denotes the amount of seconds until we leave the loop if applicable
			fprintf(stderr, "CLIENT: We were timed out by the server. Terminating.\n");
			exit(1);
		}
		if(fileReqAmount == 10 || strlen(tempList) > MAX_LIST_LEN) {break;}//Check if we meet the break conditions for max list length or item amount
	}
	
	//Now we put the number of items as well as the list itself into a string to hold it
	char fileList[strlen(tempList) + snprintf(NULL, 0, "%i", fileReqAmount) + 1];
	sprintf(fileList, "%i%s", fileReqAmount, tempList);
	free(tempList);

	//Check that client asked for at least one file
	if(fileReqAmount == 0) {
		fprintf(stderr, "CLIENT: You didn't select any files from the list to download. Alerting server and exiting.\n");
		close(s);
		exit(1);
	}

	//Send the file request list to the server
	if(debugMode == 0) {printf("CLIENT: We requested %i file(s) from the server, waiting for server response...\n\n", atoi(fileList));}
	else {printf("CLIENT: We requested %i file(s) from the server. Our list was '%s' and was %li in length\nCLIENT: Waiting for server response...\n\n", atoi(fileList), fileList, strlen(fileList));}
	send(s, fileList, MAX_LIST_LEN, 0);//Send file name list

	//Setup the structures for file names for the next part
	char *ptr = strtok(fileList, ";;"), curFile[MAX_LINE];
	ptr = strtok(NULL, ";;");

	//Go through the file receiving process for each one of the files in list we sent (Receive file verification -> Receive file -> Send file SHA1 -> Receive SHA1 verification)
	for(int i = 0; i < fileReqAmount; i++) {
		//Grab the current file that we should be receiving from the fileList
		strcpy(curFile, ptr);
		curFile[strlen(ptr)] = '\0';

		//Get the server's response to our request 
		if(isReceiving(s, readfds, 3, 500000) == 1) {memset(buf, 0, MAX_LINE);recv(s, buf, 1, 0);}
		else {//Safety check for no response from server
			fprintf(stderr, "No response to file request for '%s'. Terminating.\n", curFile);
			send(s, "INVALID", 7, 0);
			close(s);
			exit(1);
		}

		//If we got an bad or an invalid response from server
		if(strcmp(buf, "y") != 0) {
			fprintf(stderr, "CLIENT: We received a %s response from the server for our request of file '%s'. Terminating.\n", (strcmp(buf, "n") == 0 ? "bad" : "invalid"), curFile);
			close(s);
			exit(1);
		}

		//Alert client as appropriate and prepare for downloading the current file
		if(printFileMode == 1) {printf("CLIENT: Valid response from server to request for file '%s', writing file and printing contents\nCLIENT: - Receiving file -\n\n", curFile);}
		else {printf("CLIENT: Valid response from server to request for file '%s', writing file\n", curFile);}
		strcpy(downloadFileStr, "DF-");
		strcat(downloadFileStr, curFile);
		int bytesReceived = 0, downloadFile = open(downloadFileStr, O_CREAT | O_WRONLY | O_TRUNC, 0644);

		//Build SHA1 structure and initialize
		SHA_CTX ctx;
		SHA1_Init(&ctx);

		//Begin to recv() the file from server
		while((len = recv(s, buf, MAX_LINE, 0))) {
			write(downloadFile, buf, len);
			SHA1_Update(&ctx, buf, len);//Update the SHA1 as buf keeps getting read() into
			if(printFileMode == 1) {write(1, buf, len);}//Write file to terminal in debug mode as we recv() it
			bytesReceived += len; 
			memset(buf, 0, MAX_LINE);
			if(isReceiving(s, readfds, 0, 500000) == 0) {break;}//If we're not receiving anymore information, break out of the loop and continue
		}
		if(printFileMode == 1) {printf("\nCLIENT: - End file -\n");}
		printf("CLIENT: Finished receiving file '%s' from server. %i bytes received\n", curFile, bytesReceived);
		close(downloadFile);

		//Build structure for the hash and use SHA1_Final() to end
		unsigned char fileHash[SHA_DIGEST_LENGTH];//Reminder: SHA_DIGEST_LENGTH = 20 bytes
		SHA1_Final(fileHash, &ctx);

		//We need to make hash into a workable string
		char clientSHA1[SHA_DIGEST_LENGTH*2];
		for (int pos = 0; pos < SHA_DIGEST_LENGTH; pos++) {
	        sprintf((char*)&(clientSHA1[pos*2]), "%02x", fileHash[pos]);
	    }

		//Send out SHA1 of the downloaded file and wait for the server's response for verification
		send(s, clientSHA1, MAX_LINE, 0);
		memset(buf, 0, MAX_LINE);
		if(isReceiving(s, readfds, 3, 500000) == 1) {recv(s, buf, 1, 0);}
		else {//Safety check for no response from server
			fprintf(stderr, "No response to SHA1 verification request for '%s'. Terminating.\n", curFile);
			send(s, "INVALID", 7, 0);
			close(s);
			exit(1);
		}

		//Check the status message and report it to the client before finally ending process
		if(strcmp(buf, "y") == 0) {
	    	printf("CLIENT: Good response from server for SHA1 verification, confirmed that our file is valid. Moving on...\n\n");
	    }
	    else if(strcmp(buf, "n") == 0) {
	    	fprintf(stderr, "\nCLIENT: Bad response from server to our SHA1 of file '%s'. Terminating.\n", curFile);
	    	close(s);
	    	exit(1);
	    }
	    else {
	    	fprintf(stderr, "\nCLIENT: Invalid response from server '%s'. Terminating.\n", buf);
	    	close(s);
	    	exit(1);
	    }
	    ptr = strtok(NULL, ";;");//Go to the next token in our list of files that we requested
	}
	printf("CLIENT: Final file transfered, ending processes.\n");
	close(s);
    return 0;
}