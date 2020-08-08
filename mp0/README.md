The application will connect to our server running on cs438.cs.illinois.edu port: 5900 and perform a handshake, after which your program will receive and print some data from our server. The communication will happen using a TCP socket (the same you have seen in the client.c / server.c pair). The concept of handshake is common to many networking applications, in which an initial exchange of information is required to establish a logical connection (e.g., authenticate a user). This must not be confused with the TCP handshake, which is taken care of (thankfully!) by the system libraries. In short, the procedure works as follows.
First, begin by establishing a TCP connection to the server and port specified at the beginning of this section. You can use the source code from client.c as a guide, but don’t simply copy and paste from there, try to memorize the procedure  and do it yourself. Once the socket is created, the handshake is performed with the following exchange (s: indicates messages sent by our server, c: indicates messages sent by your application, ‘\n’ indicates a new line character):
c: HELO\n
s: 100 - OK\n
c: USERNAME <username>\n
s: 200 - Username: <username>\n
where username identifies your client among the multiple connections that the server can handle at the same time (Will the server be confused if you run multiple clients simultaneously and specify the same username, or will it always be able to distinguish the different connections and which one to respond to? How?). Once your client is successfully registered, it can start retrieving data from the server. To do so, repeat the RECV command, to which the server replies with a random sentence:
c: RECV\n
s: 300 - DATA: <some_string>\n
c: RECV\n
s: 300 - DATA: <some_string>\n
…
For each received line, your program should print on stdout exactly the following line:
Received: <some_string>\n
where of course <some string> is replaced by the sentence received each time. Repeat this operation 10 times, printingeach sentence, and then close the connection:
c: BYE\n
s: 400 – Bye\n
Clean up your variables (close the socket, free any memory you have allocated dynamically) and exit. Congratulations, that was it! 
