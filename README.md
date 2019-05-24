# ShortFTP

##Overview
ShortFTP is a basic FTP that I have made for my own learning purposes that allows for certain options and a MD5 check at the end of each transfer.


I started this project outside of school while in my first networking class only a few months ago when I was introduced to networking with C. At first, I was introduced to how two hosts communicate and thought it was exciting to see how FTP actually worked. I designed this based off a project that was assigned which allowed for bare-bones file transfering (you had to know what file to ask for as a client beforehand) between two hosts. Once I was done with that project, I started this one on my own, eager to see what I could do if I took the concept a little bit further. The result is ShortFTP, which allows file-transfer using passwords, MD5 checks, and other various features. 


*The lookup_and_connect() in client.c as well as the bind_and_listen() in server.c are modified from a small sample code given in "Computer Networks: A Systems Approach", 5h Edition by Larry L. Peterson and Bruce S. Davis. The rest is original work but it is nessecery to give credit where it is due.*


##Functionality
###Server:
`(Port number) (Password) (Options)`
Options for the server are:
..*-D for debugging, which prints more information to terminal than usual
..*-NP which allows any password thrown at the server from the client to be accepted
..*-NT allows the client to take infinite time to respond with a file that they want once given the list


###Client:
`(Server Address) (Server Port) (Password) (Options)`
Options for the client are:
..*-D for debugging, which prints more information to terminal than usual
