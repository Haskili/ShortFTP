# ShortFTP

## Overview
ShortFTP is a basic FTP that allows the client to look through a given list of files in the directory server is executed in, choose a file from that list, and verify the validity of that file using an md5sum. It is made mainly for learning purposes.


I started this project outside of school while in my first networking class only a few months ago when I was introduced to networking with C. At first, I was introduced to how two hosts communicate and thought it was exciting to see how FTP actually worked. I designed this based off a project that was assigned which allowed for bare-bones file transfering (you had to know what file to ask for as a client beforehand) between two hosts. Once I was done with that project, I started this one on my own, eager to see what I could do if I took the concept a little bit further. The result is ShortFTP, which allows file-transfer using passwords, MD5 checks, and other various features. 


While by no means is it complete, it serves as a good representation of where I see the project headed.


*The lookup_and_connect() in client.c as well as the bind_and_listen() in server.c are modified from a small sample code given in "Computer Networks: A Systems Approach", 5th Edition by Larry L. Peterson and Bruce S. Davis. The rest is original work but it is nessecery to give credit where it is due.*


## Functionality
### Server:
`(Port number) (Password) (Options)`

Options for the server are:
* -D for debugging, which prints more information to terminal than usual
* -NP which allows any password thrown at the server from the client to be accepted
* -NT allows the client to take infinite time to respond with a file that they want once given the list


### Client:
`(Server Address) (Server Port) (Password) (Options)`

Options for the client are:
* -D for debugging, which prints more information to terminal than usual


## Example usage
### Server
![Server](https://imgur.com/fzdHg3Q.png)


### Client
![Client](https://imgur.com/39ckNfM.png)


## Todo:
* Change the password verification process to tell the client the timeout settings
* Allow user to specify a timeout value rather than the default of 30 or no timeout at all
* Include multiple file transfer functionality
* Reduce/eliminate certain redundant checks that slow down process for larger file size tranfers
* Implement better checks on both server.c and client.c to allow for more verbose error messages
* Reduce/eliminate use of certain high-cost functions
* Reduce/clean function usage in server.c
* Re-comment some of the items in the client.c and server.c such as lookup_and_connect()
* Write a function called wait_for_response() for both client.c and server.c that returns 1 if the socket designated didn't get anything within time allocated