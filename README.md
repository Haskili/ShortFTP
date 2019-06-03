# ShortFTP

## Overview
ShortFTP is a basic FTP server/client that allows the client to look through a given list of files in the directory server is executed in, choose files from that list, and verify the validity of those files using an md5sum. It is made mainly for learning purposes.


I started this project outside of school while in my first networking class only a few months ago when I was introduced to networking with C. At first, I was introduced to how two hosts communicate and thought it was exciting to see how that interaction worked. I designed this based off a project that was assigned in class which allowed for bare-bones (as in you actually had to know what file to ask for beforehand as the client and everything) file transfering between two hosts. Once I was done with that project, I started this one on my own, eager to see what I could do if I explored the concept a little bit further. The result is ShortFTP, which allows file-transfer between server/client using passwords, MD5 checks, and various other features. 


While by no means is it complete, I believe it may serve as a decent learning tool for networking using C if nothing else.


*The lookup_and_connect() in client.c as well as the bind_and_listen() in server.c are modified from a small sample code given in "Computer Networks: A Systems Approach", 5th Edition by Larry L. Peterson and Bruce S. Davis. The rest is original work but it is necesarry to give credit where it is due.*


## Functionality
### Server:
`(Port number) (Password) (Options)`

Options for the server are:
* -D for debugging, which prints more information to terminal such as client IP
* -NP which allows any password thrown at the server from the client to be accepted
* -NT Allows the client to take infinite time to respond with a file that they want once given the list
* -SU Allows the user to designate they wish for the server to stay up and keep receiving new clients until they manually close the process
* -RM Sets the server to recover after certain types of errors. Allows for better usage of -SU if the user wants to leave the server running without resetting on error.


### Client:
`(Server Address) (Server Port) (Password) (Options)`

Options for the client are:
* -D for debugging, which prints more information to terminal such as file contents as it's received


## Example usage
### Server
![Server](https://imgur.com/RmRxzsW.png)


### Client
![Client](https://imgur.com/AlHhq4W.png)


## Todo:
* Consider changing the recovery mode process (using a goto). Make it such that the file transfer process is a function and based on that return value we can recover from an error depending on it's type
* Change the password verification process to tell the client the timeout settings at the same time as server responds to password
* Allow user to specify a timeout value rather than the default of 30 or no timeout at all
* Reduce/eliminate use of certain high-cost functions
* Re-comment some of the items in the client.c and server.c such as lookup_and_connect()