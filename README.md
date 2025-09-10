Normal file replication software requires a purchase. Not this software. This program is meant to take a file, given by the client, and have a server replicate it. It also allows up to 10 concurrent clients replicating at the same time.

client - The client executable runs with 6 paramaters, server ip and port, maximum segment size the server is allowed to send, maximum window size, input and output file paths. They have to be on the same machine and the server will automatically find a path to it

server - The server execuatable runs with 2 parameters, a port number and a drop rate. The drop rate parameter is for testing purposes only. For a seamless experience, just type 0.