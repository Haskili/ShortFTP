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

int verifyPassword(int s, fd_set readfds, const char * password, char * buf) {//Send password in argv[3] to server and act on reponse from server
	send(s, password, 15, 0);//Send password to server

    /* Setup for receiving the response from server */
    FD_SET(s, &readfds);//Add s to list of sockets
    memset(buf, 0, MAX_LINE);
    recv(s, buf, MAX_LINE, 0);

    /* Ask if the server verified our password */
    if(strcmp(buf, "y") == 0) {
    	return 0;
    }

    /* Server didn't give good response to password we sent, alert client */
    printf("CLIENT: Error verifying password with server, terminating.\n");
	return 1;
}

int lookup_and_connect( const char *host, const char *service ) {
	struct addrinfo hints;
	struct addrinfo *rp, *result;
	int s;

	/* Translate host name into peer's IP address */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	if ((s = getaddrinfo( host, service, &hints, &result)) != 0) {
		fprintf( stderr, "stream-talk-client: getaddrinfo: %s\n", gai_strerror(s));
		return -1;
	}

	/* Iterate through the address list and try to connect */
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		if ((s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
			continue;
		}
		if (connect(s, rp->ai_addr, rp->ai_addrlen) != -1) {
			break;
		}
		close(s);
	}
	if (rp == NULL) {
		perror("stream-talk-client: connect");
		return -1;
	}
	freeaddrinfo(result);
	return s;
}

int main( int argc, char *argv[] ) {
	char *host;
	char buf[MAX_LINE];
	int s, len;
	host = argv[1];
	const char * SERVER_PORT = argv[2];
	char * password = argv[3];
	struct timeval tv;
    fd_set readfds;//Create a fileset for file descriptors
    FD_ZERO(&readfds);//Clear fileset
    char md5Command[MAX_LINE];//String that we will use for our system() call
	strcpy(md5Command, "md5sum Downloaded_File | tee -a clientTemp");//Prepare the system() call string
	char clientMD5[MAX_LINE];

    if(!argv[1] || !argv[2] || !argv[3]) {
       printf("CLIENT ERROR: Incorrect arguments,\nUSAGE: HOST-ADR, PORT#, PASS\n");
       exit(1);
    }

    //Get all the options if user selected any - Might want to put ints inside of if() and instead ask for each mode operation is argc != 3 so it won't give error
	int debugMode = 0;
	if(argc != 3) {
		for(int i = 4; i < argc; i++) {
			if(strcmp(argv[i], "-D") == 0) {
				debugMode = 1;
			}
			else if(strcmp(argv[i], "-MODE") == 0) {
				//set the int
			}
			else {
				printf("CLIENT: Invalid option '%s', \nthe valid options are '-D' for debugging mode, '-MODE' for MODE, and 'XX'.\nCLIENT: Continuing with process.\n", argv[i]);
			}
		}
	}

	/* Lookup IP and connect to server */
	if ((s = lookup_and_connect(host, SERVER_PORT)) < 0) {
		exit(1);
	}

	/* Receive a verification from server that we gave a valid password */
    if(verifyPassword(s, readfds, password, buf) != 0) {
    	close(s);
    	exit(1);
    }

    /* Return response to server on whether we want to receive the list of files available */
	printf("CLIENT: List available from host with address '%s'\nCLIENT: Are you certain you want to download the list from this host? (yes = y, no = n)\n", argv[1]);
	while(1) {
		memset(buf, 0, MAX_LINE);
		fgets(buf, MAX_LINE, stdin);
		buf[1] = '\0';//Correction for new line on fgets getting filename
		if(strcmp(buf, "y") == 0) {
			send(s, buf, strlen(buf), 0);//Tell server to send the list
			memset(buf, 0, MAX_LINE);//Reset buffer
			break;
		}
		else if (strcmp(buf, "n") == 0) {
			send(s, buf, strlen(buf), 0);//Tell the server to not send the list
			memset(buf, 0, MAX_LINE);
			close(s);
			printf("CLIENT: Told server not to send list, terminating\n");
			return 0;
		}
		else {
			printf("Invalid input: '%s', please type 'y' for yes, or 'n' for no\n", buf);
		}
	}

	/* Print the list of files received with clear indicators of start/stop */
	printf("CLIENT: - Start list -\n\n");
	while((len = recv(s, buf, MAX_LINE, 0))) {
		write(1, buf, len);
		memset(buf, 0, MAX_LINE);
		
		//Clear the fdset and reset the timevals
		FD_ZERO(&readfds);
		FD_SET(s, &readfds);
		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		
		//Break out of receiving loop if we're not receiving data from the server anymore
		if((select(s+1, &readfds, NULL, NULL, &tv)) <= 0) {
			break;
		}
	}
	printf("\nCLIENT: - End list -\n");

	//Get the filename choice and send it back to the server
	printf("CLIENT: Please choose a file from the list above.\n");
	memset(buf, 0, MAX_LINE);
	fgets(buf, MAX_LINE, stdin);//Retrieve input
	buf[strlen(buf) - 1] = '\0';//Correction for newline on fgets
	send(s, buf, MAX_LINE, 0);//Send filename

	//Get the server's response to our request 
	memset(buf, 0, MAX_LINE);
	recv(s, buf, 1, 0);

	//If we got an bad or an invalid response from server
	if(strcmp(buf, "y") != 0) {
		printf("CLIENT: We received an invalid or bad response from the server for our request of file. Terminating.\n");
		close(s);
		exit(1);
	}

	//Since we got a good response, we are going to receive the file data
	if(debugMode == 1) {printf("CLIENT: Valid response from server, writing file and printing contents to terminal\nCLIENT: - Receiving file -\n\n");}
	else {printf("CLIENT: Valid response from server, writing file\n");}
	int downloadFile = open("Downloaded_File", O_CREAT | O_WRONLY | O_TRUNC, 0644);
	int bytesReceived = 0;
	while((len = recv(s, buf, MAX_LINE, 0))) {
		write(downloadFile, buf, len);
		if(debugMode == 1) {write(1, buf, len);}//Write file to terminal in debug mode as we recv() it
		bytesReceived += len; 
		memset(buf, 0, MAX_LINE);

		//Clear the fdset and reset the timevals
		FD_ZERO(&readfds);
		FD_SET(s, &readfds);
		tv.tv_sec = 0;
		tv.tv_usec = 500000;

		//Break out of receiving loop if we're not receiving data from the server anymore
		if((select(s+1, &readfds, NULL, NULL, &tv)) <= 0) {
			break;
		}
	}
	if(debugMode == 1) {printf("\n\nCLIENT: - End file -\n");}
	printf("CLIENT: Finished receiving file from server, %i bytes received.\n", bytesReceived);
	close(downloadFile);

	//Get the md5 of our downloaded file
	printf("CLIENT: Grabbing the md5sum of our downloaded file and checking with server...\n\n");
	int clientTemp = open("clientTemp", O_CREAT | O_RDWR | O_TRUNC, 0644);
	system(md5Command);
	read(clientTemp, clientMD5, MAX_SIZE);
	close(clientTemp);
	remove("clientTemp");

	//Send our resulting md5 to the server and wait for it's response
	send(s, clientMD5, MAX_LINE, 0);

	//Receive the status of the md5 check from the server
	memset(buf, 0, MAX_LINE);
	recv(s, buf, 1, 0);

	//Check the status message and report it to the client before finally ending process
	if(strcmp(buf, "y") == 0) {
    	printf("\nCLIENT: Valid response from server. End of process.\n");
    }
    else if(strcmp(buf, "n") == 0) {
    	printf("\nCLIENT: Error response from server. Terminating.\n");
    	close(s);
    	exit(1);
    }
    else {
    	printf("\nInvalid respopnse from server '%s'. Terminating.\n", buf);
    	close(s);
    	exit(1);
    }
	close(s);
    return 0;
}