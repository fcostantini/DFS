# DFS
Implementation of a Distributed File System in both C and Erlang. Files are virtual.

# Setting up the server
C: -make
   -./sv
   -in another terminal, nc 127.0.0.1 8000 (telnet should work)
   
Erlang: -erl
        -c(serv.erl)
        -serv:init_serv()
        -in another terminal, nc 127.0.0.1 8000 (telnet should work)

#Operations:
-CON: connect to the server
-LSD: list all files
-CRE nfile: creates the (empty) file nfile
-DEL nfile: deletes the file nfile
-OPN nfile: opens the file nfile (returns a file descriptor)
-WRT FD fd SIZE s content: write content of size s to file fd
-REA FD fd SIZE s: reads s bytes from file fd
-CLO FD fd: closes file fd
-BYE: terminates connection
