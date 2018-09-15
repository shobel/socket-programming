# socket-programming

Compilation Steps:
g++ -o Server Server.cpp -lsocket -lnsl -lresolv
g++ -o Client Client.cpp -lsocket -lnsl -lresolv

Additional info:
Boiler plate socket code borrowed from the guide provided to us in the materials: beej.us/guide/bgnet/html/multi/clientserver.html.

The client can connect to a remote server by specifying the hostname as the first argument. If the first argument is not provided, the client will connect to the localhost over the appropriate port.

Acheiving a good seed for randomness was a challenge due to how fast the servers run. Using time(0) as the seed was not precise enough to generate different values for IP and transaction ID if clients or child forks in the server were started immediately after one another. 
