#!/bin/bash

#What this script aims to do is simulate a Server/Client interaction using a random port (within a given range) and a random password.
#Both the client and server outputs will be "silenced", but they will have their individual outputs logged 
#in two .out files for verification that the process was successfully completed. For that check, I set a variable equal to the results 
#of searching the client output file for a string that only displays upon successful file tranfser and verification. If the variable
#is empty or isn't what it's supposed to be (see below), it means the process failed. Otherwise, we alert the user of success and exit.

#Check for the compiled server
findServer=`ls server 2> /dev/null`
if [ "$findServer" != "server" ];
	then 
		echo "The compiled Server was not found in the present working directory, compiling..."
		gcc server.c -Wall -lcrypto -o server
fi

#Check for the compiled client
findClient=`ls client 2> /dev/null`
if [ "$findClient" != "client" ];
	then
		echo "The compiled Client was not found in the present working directory, compiling..."
		gcc client.c -Wall -lcrypto -o client
fi

#Build variables
CEILING=9999
FLOOR=7000
portNum=0
defaultAddr="127.0.0.1"
serverOptions="-SU"
path=$(pwd)

#Generate random number (7000-9999) for the port using modulus to reduce as needed
echo "Generating random number for server port..."
while [ "$portNum" -le $FLOOR ]
do
  portNum=$RANDOM
  let "portNum %= $CEILING"
done

#Generate random string for password
echo "Generating random password..."
password=$(openssl rand -base64 10)

#Generate a server output file
touch $path/serverOutputFile.out

#Start the server (options can be edited as needed)
echo "Starting server on port '$portNum' with password '$password'"
$path/server $portNum $password $serverOptions > $path/serverOutputFile.out &

#Generate a random file for transfer process testing
touch $path/tempTransferTestFile
openssl rand -base64 300 > $path/tempTransferTestFile 2> /dev/null

#Generate a client output file
touch $path/clientOutputFile.out

#Start the client (it's accessing loopback with defaultAddr, which works just the same as normal usage)
echo "Starting client and trying to access '$defaultAddr' on port '$portNum' with password '$password'"
$path/client $defaultAddr $portNum $password > $path/clientOutputFile.out << INPUT
y
tempTransferTestFile
INPUT

echo "Process complete, checking results in clientOutputFile.out for verification..."

#Verify that the client got a good response from server for SHA1 check on their downloaded file
getValid=`sed -n '/Good response from server for SHA1 verification/p' clientOutputFile.out`
if [ "$getValid" == "CLIENT: Good response from server for SHA1 verification, confirmed that our file is valid. Moving on..." ];
	then echo "Success, transfer was completed and file was verified with a SHA1 check"
else
	echo "Failure. Please check the clientOutputFile.out for more information"
fi
rm tempTransferTestFile 2> /dev/null
rm DF-tempTransferTestFile 2> /dev/null