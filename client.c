#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_LINE 256
#define MAX_SIZE 30

int verifyPassword(int s, const char * password, char * buf) {//Used to send() a given password through a given sockfd and recv() the response
	send(s, password, 15, 0);//Send the password to server

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
    char md5Command[MAX_LINE];//String that we will use for our system() call
	char clientMD5[MAX_LINE];//String to hold result of client's MD5 on the downloaded files
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
	if ((s = lookup_and_connect(host, SERVER_PORT)) < 0) {
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

    //Return response to server on whether we want to receive the list of files available
	printf("CLIENT: List available from host with address '%s'\nCLIENT: Are you certain you want to download the list from this host? (yes = y, no = n)\n", argv[1]);
	while(1) {
		memset(buf, 0, MAX_LINE);
		fgets(buf, MAX_LINE, stdin);
		buf[1] = '\0';//Correction for new line on fgets getting filename
		if(strcmp(buf, "y") == 0 || strcmp(buf, "Y") == 0) {
			send(s, buf, strlen(buf), 0);//Tell server to send the list
			memset(buf, 0, MAX_LINE);//Reset buffer
			break;
		}
		else if (strcmp(buf, "n") == 0 || strcmp(buf, "N") == 0) {
			send(s, buf, strlen(buf), 0);//Tell the server to not send the list
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
	printf("CLIENT: Please choose up to 9 files from the list above and finish by entering a ';;' or an empty line.\n\n");
	char fileList[MAX_LINE];
	fileList[0] = '0';
	int counter = 0;
	int fileReqAmount = 0;
	fd_set waitFDS;
	
	//Get the filenames the users wants
	while(1) {
		//Clear the fdset
		FD_ZERO(&waitFDS);
		FD_SET(0, &waitFDS);

		//Reset the timevalues used for select() wait time
		struct timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		//Get input on activity in stdin
		if(select(1, &waitFDS, NULL, NULL, &tv) > 0) {
			memset(buf, 0, MAX_LINE);
			fgets(buf, MAX_LINE, stdin);
			buf[strlen(buf) - 1] = '\0';//Correction for new line on fgets getting input
			if(strcmp(buf, ";;") == 0 || strcmp(buf, "") == 0) {
				memset(buf, 0, MAX_LINE);//Reset buffer
				break;
			}
			//Increment the count of files we're requesting since we know user entered a name
			fileReqAmount++;

			//Append the list with the new file and the preliminary separator
			strcat(fileList, ";;");
			strcat(fileList, buf);

			//Copy list of files into buf temporarily
			strcpy(buf, fileList + 1);//Copy only the files of fileList into buf

			//Set the files requested counter in string
			sprintf(fileList, "%i", fileReqAmount);

			//Put the list being held by buf back into the fileList string and print it
			strcat(fileList, buf);
		}

		//Increment counter for the seconds in the input loop and check if we need to exit()
		counter++;
		if(counter == 60 && serverTimeout == 1) {//This denotes the amount of seconds until we leave the loop if applicable
			fprintf(stderr, "CLIENT: We were timed out by the server. Terminating.\n");
			close(s);
			exit(1);
		}
		if(fileReqAmount == 9) {break;}//This denotes the maximum files allowed to request
	}
	
	//Check that client asked for at least one file
	if(atoi(fileList) == 0 || fileReqAmount == 0) {//Added in additional condition for safety reasons
		fprintf(stderr, "CLIENT: You didn't select any files from the list to download. Alerting server and exiting.\n");
		close(s);
		exit(1);
	}

	//Send the file request list to the server
	printf("CLIENT: We requested %i files from the server, waiting for server response...\n\n", atoi(fileList));
	send(s, fileList, MAX_LINE, 0);//Send filename list

	//Setup the structures for file names for the next part
	char *ptr = strtok(fileList, ";;");
	char curFile[MAX_LINE];
	ptr = strtok(NULL, ";;");

	//Receive each file from server by going into a loop an amount of times equal to # of files we requested
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
			fprintf(stderr, "CLIENT: We received an invalid or bad response from the server for our request of file '%s'. Terminating.\n", curFile);
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
		
		//Begin to recv() the file from server
		while((len = recv(s, buf, MAX_LINE, 0))) {
			write(downloadFile, buf, len);
			if(printFileMode == 1) {write(1, buf, len);}//Write file to terminal in debug mode as we recv() it
			bytesReceived += len; 
			memset(buf, 0, MAX_LINE);
			if(isReceiving(s, readfds, 0, 500000) == 0) {break;}//If we're not receiving anymore information, break out of the loop and continue
		}
		if(printFileMode == 1) {printf("\nCLIENT: - End file -\n");}
		printf("CLIENT: Finished receiving file '%s' from server. %i bytes received\n", curFile, bytesReceived);
		close(downloadFile);

		//Prepare the string for the system() MD5 command
		strcpy(md5Command, "md5sum DF-");
		strcat(md5Command, curFile);
		debugMode == 0 ? strcat(md5Command, " > clientTemp 2> /dev/null") : strcat(md5Command, " | tee -a clientTemp");//Append to MD5 command so that it writes to file

		//Get the md5 of our downloaded file with system()
		if(debugMode == 1) {printf("CLIENT: Grabbing the md5sum of our downloaded file and checking with server\n");}
		int clientTemp = open("clientTemp", O_CREAT | O_RDWR | O_TRUNC, 0644);//Create a temporary file to hold our md5 result
		system(md5Command);//Get the md5 for our downloaded file and store it in the temporary file
		read(clientTemp, clientMD5, 32);//Read the MD5 from the file into our string
		clientMD5[32] = '\0';//Correct the string to only include the MD5 result, and not the end "file name" part that includes the file name that the function was called on
		
		//Close the temporary file and delete it now that we're finished creating the MD5 for that file
		close(clientTemp);
		remove("clientTemp");

		//Send our resulting md5 to the server and wait for it's response
		send(s, clientMD5, MAX_LINE, 0);

		//Receive the status of the md5 check from the server
		memset(buf, 0, MAX_LINE);
		if(isReceiving(s, readfds, 0, 500000) == 1) {recv(s, buf, 1, 0);}

		//Check the status message and report it to the client before finally ending process
		if(strcmp(buf, "y") == 0) {
	    	printf("CLIENT: Good response from server for MD5 verification, confirmed that our file is valid. Moving on...\n\n");
	    }
	    else if(strcmp(buf, "n") == 0) {
	    	fprintf(stderr, "\nCLIENT: Bad response from server to our MD5 of file '%s'. Terminating.\n", curFile);
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