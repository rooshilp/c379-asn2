Rooshil Patel
Assignment 2
CMPUT 379

In order to run the servers, first run the make file in order to generate the executables.
To create all executable, run:
	make
To create the server_f executable, run:
	make server_f
To create the server_p executable, run:
	make server_p

Server_s is not listed as I was unable to implement it in time.

In order to run the executable, 4 arguments are required. An example of a properly formatted way to run a server is:
	./server_f 8000 "serving directory" "logfile"

Where ./server_f indiciates the server you wish to run (server_f or server_p)
      8000 represents the port number you wish the server to serve on
      "serving directory" represents the directory which contains the files you wish to serve
  and "logfile" indicates the file and location to which you want the server to log to.
