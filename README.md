# ShortFTP

## Overview
ShortFTP is a basic FTP server/client that allows the client to look through a given list of files in the directory server is executed in, choose files from that list, download them, and verify the validity of those downloaded files. It is made mainly for learning purposes.


I started this project outside of school while in my first networking class last semester (Spring 2019) when I was introduced to C. I designed ShortFTP based off a project that was assigned in class which allowed for bare-bones (as in you actually had to know what file to ask for beforehand as the client type of "bare") file transfering between two hosts. Once I was done with that project I started this one on my own, eager to see what I could do if I explored the concept a little bit further.


While by no means is it complete, I believe it may serve as a decent learning tool for networking using C. I try to comment as best as I can while going over important topics/code. If you need any clarification or have any questions, feel free to message me.


*The lookup_and_connect() in client.c as well as the bind_and_listen() in server.c are modified from a small sample code given in "Computer Networks: A Systems Approach", 5th Edition by Larry L. Peterson and Bruce S. Davis. The rest is original work but it is necesarry to give credit where it is due.*


## Functionality
### Server:
`(Port number) (Password) (Options)`

Options for the server are:
```
-D    Turn on debugging, which prints more information to terminal such as client IP
-NP   Disable the password requirement
-NT   Disable the timeout while waiting for client to respond with a file request list
-SU   The server will automatically wait for the next client after finishing a transfer
-RM   Allow the server to recover after certain types of errors, best used with -SU
-PF   Print the file to terminal as the contents are being read during transfer
-LO   Log all activity to a uniquely named output file
-H    Print the usage information to the terminal
```

### Client:
`(Server Address) (Server Port) (Password) (Options)`

Options for the client are:
```
-D    Turn on debugging, which prints more information to terminal such as the SHA1 as it's created
-PF   Allow the user to see the file contents in the terminal terminal as it's received
-H    Print the usage information to the terminal
```

## Example usage
### Server
![Server](https://imgur.com/DpNnkoG.png)


### Client
![Client](https://imgur.com/n3pl5hp.png)


## Todo:
-   [x] Create more automated tests to cover edge cases
-   [ ] Additional security features for Server
-   [ ] Abiltiy for server to handle multiple clients simultaneously