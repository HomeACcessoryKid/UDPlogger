# UDPlogger
a light way to get the logs out of your ESP8266

There is a client you can compile on macOS that will collect your logs.
The key advantage is that it is simple, and it only sends outs logs if your client is looking for them.
You can run multiple clients in parallel, each looking for a particular IP address source.

(c) HomeAccessoryKid 2019

## Instructions
- add to Makefile: EXTRA_COMPONENTS = $(abspath UDPlogger)
- git submodule add https://github.com/HomeACcessoryKid/UDPlogger
- in your .c files: #include <udplogger.h>
- read the udplogger.h file for macros and #defines


## Client
- $ gcc udplog-client.c -o udplog-client  
- $ ./udplog-client 192.168.0.2  
output arrives here                                 and/or  
Received trigger from 192.168.0.2:44444             if the source started after the client  
Trigger from 192.168.0.3:44444 not for 192.168.0.2  if another source started  
- So if you don't know the IP yet, just run it for a random IP in your subnet and see what comes up

