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

int verifyPassword(int s, const char * password, char * buf) {//Used to send() a given password through a given sockfd and recv() the response
	send(s, password, MAX_LINE, 0);//Send the password to server

    //Setup for receiving the response from server
    memset(buf, 0, MAX_LINE);
    recv(s, buf, MAX_LINE, 0);

    //Ask if the server verified our password and what timeout is set to
    if(strcmp(buf, "VALID-NT") == 0 || strcmp(buf, "VALID-TS") == 0) {
    	return 0;
    }

    //Server didn't validate client's password, return bad value
	return 1;
}

int lookup_and_connect(const char *host, const char *service) {
	struct addrinfo hints;
	struct addrinfo *rp, *result;
	int s;

	//Translate host name into peer's IP address
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	if((s = getaddrinfo( host, service, &hints, &result)) != 0) {//Use getaddrinfo() to give us a linked list of addresses
		fprintf( stderr, "stream-talk-client: getaddrinfo: %s\n", gai_strerror(s));
		return -1;
	}

	//Iterate through the address list and try to connect
	for(rp = result; rp != NULL; rp = rp->ai_next) {
		if((s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
			continue;
		}
		if(connect(s, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
		close(s);
	}
	if(rp == NULL) {//If our pointer in linked list is NULL
		perror("stream-talk-client: connect");
		return -1;
	}
	freeaddrinfo(result);
	return s;
}

int isReceiving(int s, fd_set fds, int seconds, int microseconds) {//Wait a given time period for activity on a designated fd
	//Clear the fdset
	FD_ZERO(&fds);
	FD_SET(s, &fds);

	//Reset the timevalues used for select() wait time
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 500000;
	
	//Return 1 (true) if we see activity
	if((select(s+1, &fds, NULL, NULL, &tv)) > 0) {
		return 1;
	}
	return 0;//Otherwise, we return 0 (false) if we don't get anything within time period
}

int main( int argc, char *argv[] ) {
	char *host;//IP address of the server we want to connect to
	char buf[MAX_LINE];//Buffer we will use to send(), recv(), and write()
	int s, len;
	host = argv[1];
	const char * SERVER_PORT = argv[2];
	char * password = argv[3];
    fd_set readfds;//Create a fileset for file descriptors
    FD_ZERO(&readfds);//Clear the fileset
	char downloadFileStr[MAX_LINE];//String to hold the name of the file that we will create to write downloaded information into

    if(!argv[1] || !argv[2] || !argv[3]) {
       fprintf(stderr, "CLIENT ERROR: Incorrect arguments,\nUSAGE: HOST-ADR, PORT#, PASS, -[OPTIONS]\n");
       exit(1);
    }

    //Get all the options if user selected any - Might want to put ints inside of if() and instead ask for each mode operation is argc != 3 so it won't give error
	int debugMode = 0;
	int printFileMode = 0;
	if(argc != 3) {
		for(int i = 4; i < argc; i++) {
			if(strcmp(argv[i], "-D") == 0) {debugMode = 1;}
			else if(strcmp(argv[i], "-PF") == 0) {printFileMode = 1;}
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
	debugMode == 0 ? printf("\n") : printf("CLIENT: - Start list -\n\n");
	while((len = recv(s, buf, MAX_LINE, 0))) {
		write(1, buf, len);
		memset(buf, 0, MAX_LINE);
		if(isReceiving(s, readfds, 0, 500000) == 0) {break;}//If we're not receiving anymore information, break out of the loop and continue
	}
	debugMode == 0 ? printf("\n") : printf("\nCLIENT: - End list -\n");

	//Alert user to choose from up to 9 items from the list and set up for the user give input
	if(serverTimeout == 1) {printf("CLIENT: The server has given us a minute till the timeout for choosing files.\n");}
	printf("CLIENT: Please choose up to 10 files from the list above and finish by entering a ';;' or an empty line.\n\n");
	char fileList[MAX_LINE];
	memset(fileList, 0, MAX_LINE);
	int counter = 0;
	int fileReqAmount = 0;
	fd_set waitFDS;
	
	//Get the filenames the users wants
	while(1) {
		//Get input on activity in stdin
		if(isReceiving(0, waitFDS, 1, 0) == 1) {//Use isReceiving() to get activity if it appears on stdin (0 in this case)
			memset(buf, 0, MAX_LINE);
			fgets(buf, MAX_LINE, stdin);
			buf[strlen(buf) - 1] = '\0';//Correction for new line on fgets getting input
			
			//First we check if we got the command to break out of the loop
			if(strcmp(buf, ";;") == 0 || strcmp(buf, "") == 0) {memset(buf, 0, MAX_LINE);break;}//We reset our buffer and break out

			//Increment the count of files we're requesting and append the list with the deliminator as well as the new file name
			fileReqAmount++;
			sprintf(fileList + strlen(fileList), ";;%s", buf);
		}

		//Increment counter for the seconds in the input loop and check if we need to exit()
		counter++;
		if(counter == 60 && serverTimeout == 1) {//This denotes the amount of seconds until we leave the loop if applicable
			fprintf(stderr, "CLIENT: We were timed out by the server. Terminating.\n");
			close(s);
			exit(1);
		}
		if(fileReqAmount == 10 || strlen(fileList) > (MAX_LINE - 11)) {break;}//This denotes the maximum files allowed to request (and a check for file list length for send() size issues)
	}

	//Before we send the list to the server, we have to check that it meets the length requirements for our send() usage and reduce it if needed
	if(strlen(fileList) > (MAX_LINE - 11)) {//(MAX_LINE - 11) (245) is our capacity for our list length
		printf("CLIENT: List was too long, we need to first reduce the list given before sending...\n");
		
		//Build variables and reset buf so it can hold the reduced list
		int totItems = 0;
		char *ptr = strtok(fileList, ";;");
		memset(buf, 0, MAX_LINE);

		//Reduce the list as needed (CAPPED AT (245) CHARACTERS)
		for(int len = strlen(ptr) + 3; len <= (MAX_LINE - 11); len += strlen(ptr) + (snprintf( NULL, 0, "%d", (totItems + 1))) + 2) {
			//Append the buffer with the item and it's preceding deliminator
			strcat(buf, ";;");
			strcat(buf, ptr);
			
			//Increment our values
			ptr = strtok(NULL, ";;");
			totItems++;
		}

		//Remake buf and fileList by putting the correct number of items in front followed by the reduced list
		memset(fileList, 0, MAX_LINE);
		sprintf(fileList, "%i", totItems);
		strcat(fileList, buf);
		memset(buf, 0, MAX_LINE);
		strcpy(buf, fileList);
	}
	
	//If we didn't need to reduce the list, set up the buf and fileList so they contain the requested file amount followed by the list of files itself
	else {snprintf(buf, (MAX_LINE + 3), "%i%s", fileReqAmount, fileList);memset(fileList, 0, MAX_LINE);strcpy(fileList, buf);}//The MAX_LINE + 3 would cause issues if we didn't reduce beforehand

	//Check that client asked for at least one file
	if(atoi(buf) == 0) {
		fprintf(stderr, "CLIENT: You didn't select any files from the list to download. Alerting server and exiting.\n");
		close(s);
		exit(1);
	}

	//Send the file request list to the server
	if(debugMode == 0) {printf("CLIENT: We requested %i file(s) from the server, waiting for server response...\n\n", atoi(buf));}
	else {printf("CLIENT: We requested %i file(s) from the server. Our list was '%s' and was %li in length\nCLIENT: Waiting for server response...\n\n", atoi(buf), buf, strlen(buf));}
	send(s, buf, MAX_LINE, 0);//Send file name list

	//Setup the structures for file names for the next part
	char *ptr = strtok(fileList, ";;");
	char curFile[MAX_LINE];
	ptr = strtok(NULL, ";;");

	//Go through the file receiving process for each one of the files in list we sent (Receive file verification -> Receive file -> Send file SHA1 -> Receive SHA1 verification)
	int rqstAmnt = atoi(fileList);
	for(int i = 0; i < rqstAmnt; i++) {
		//Grab the current file that we should be receiving from the fileList
		strcpy(curFile, ptr);
		curFile[strlen(ptr)] = '\0';

		//Get the server's response to our request 
		memset(buf, 0, MAX_LINE);
		if(isReceiving(s, readfds, 0, 500000) == 1) {recv(s, buf, 1, 0);}

		//If we got an bad or an invalid response from server
		if(strcmp(buf, "y") != 0) {
			fprintf(stderr, "CLIENT: We received a %s response from the server for our request of file '%s'. Terminating.\n", (strcmp(buf, "n") == 0 ? "bad" : "invalid"), curFile);
			close(s);
			exit(1);
		}

		//Alert client as appropriate and prepare for downloading the current file
		if(printFileMode == 1) {printf("CLIENT: Valid response from server to request for file '%s', writing file and printing contents\nCLIENT: - Receiving file -\n\n", curFile);}
		if(printFileMode == 0) {printf("CLIENT: Valid response from server to request for file '%s', writing file\n", curFile);}
		strcpy(downloadFileStr, "DF-");
		strcat(downloadFileStr, curFile);
		int downloadFile = open(downloadFileStr, O_CREAT | O_WRONLY | O_TRUNC, 0644);//Open file we're going to write our information into
		int bytesReceived = 0;

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
		if(isReceiving(s, readfds, 0, 500000) == 1) {recv(s, buf, 1, 0);}

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