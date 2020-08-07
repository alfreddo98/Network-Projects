#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define recvBufSize (8 * 256 * 2) + 30 // extra 30 is for the ttl and just some padding in case.
#define YES 1
#define NO 0

// Define the log events --CdG
#define EVT_FWD 1
#define EVT_SND 2
#define EVT_REC 3
#define EVT_UNR 4

// define min macro
#ifndef min
    #define min(a,b) ((a) < (b) ? (a) : (b))
#endif


int globalMyID;
//last time you heard from each node. 
struct timeval globalLastHeartbeat[256];

int newly_down[256];

int network_size = 1;

//our all-purpose UDP socket, to be bound to 10.1.1.globalMyID, port 7777
int globalSocketUDP;
//pre-filled for sending to 10.1.1.0 - 255, port 7777
struct sockaddr_in globalNodeAddrs[256];

// Keeps track of costs and up hosts that are connected to it: --CdG
struct neighboor_cost {
    unsigned int cost;
    unsigned short up;
};
struct neighboor_cost neighboors[256];

// Routing Table: If I want to route to some host, the next host to forward to is at the index of the destination host.
// -1 if there is no route to that host. -- CdG
short routing_table[256];

// File descriptor for the logfile:
FILE * log_fd;


// Function Declarations:
extern void init_router(char ** argv);
extern int fetchPacket(char * recvBuf);
extern int handleNonTopoPackets(char * recvBuf);
extern void* announceToNeighbors(void* unusedParam);
extern int updateCost(char recvBuf[recvBufSize]);
extern int checkDownNeighboors();
extern int markNeighboorUp(unsigned short ID);
extern int markNeighboorDown(unsigned short ID);
extern void logEvent(short type, short dest, short next, char* msg);
extern void routeMessage(char * recvBuf, short send);
extern void sendPacket(char * buf, unsigned int len, short destID);


// Initializes and sets up connections for router
void init_router(char ** argv){

    //initialization: get this process's node ID, record what time it is, 
	//and set up our sockaddr_in's for sending to the other nodes.
    globalMyID = 0; // just in case
	globalMyID = atoi(argv[1]);
	int i;

	for(i=0;i<256;i++)
	{
		gettimeofday(&globalLastHeartbeat[i], 0);
		
		char tempaddr[100];
		sprintf(tempaddr, "10.1.1.%d", i);
		memset(&globalNodeAddrs[i], 0, sizeof(globalNodeAddrs[i]));
		globalNodeAddrs[i].sin_family = AF_INET;
		globalNodeAddrs[i].sin_port = htons(7777);
		inet_pton(AF_INET, tempaddr, &globalNodeAddrs[i].sin_addr);

        // init the neighboors struct: -- CdG
        neighboors[i].cost = 1;
        neighboors[i].up = NO;
        // init the routing table to have no valid routes: -- CdG
        routing_table[i] = -1;
	}

    // Set up log file descriptor:
    log_fd = fopen(argv[3], "w");
	
	
	// Read and parse initial costs file. default to cost 1 if no entry for a node. file may be empty. --CdG
	// Credit: https://www.programiz.com/c-programming/examples/read-file
    FILE * costs_fd;
    unsigned int host;
    unsigned int cost;

    costs_fd = fopen(argv[2], "r");

    if (costs_fd == NULL){
        perror("Invalid costs file.");
		exit(1);
    }

    while (fscanf(costs_fd, "%u %u [^\n]", &host, &cost) != EOF){ // read a line from the file

        neighboors[host].cost = cost; // update the cost

    }

    neighboors[globalMyID].cost = 0; // costs 0 to send to myself
    
    fclose(costs_fd);


	//socket() and bind() our socket. We will do all sendto()ing and recvfrom()ing on this one.
	if((globalSocketUDP=socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		perror("socket");
		exit(1);
	}
	char myAddr[100];
	struct sockaddr_in bindAddr;
	sprintf(myAddr, "10.1.1.%d", globalMyID);	
	memset(&bindAddr, 0, sizeof(bindAddr));
	bindAddr.sin_family = AF_INET;
	bindAddr.sin_port = htons(7777);
	inet_pton(AF_INET, myAddr, &bindAddr.sin_addr);
	if(bind(globalSocketUDP, (struct sockaddr*)&bindAddr, sizeof(struct sockaddr_in)) < 0)
	{
		perror("bind");
		close(globalSocketUDP);
		exit(1);
	}
}

// Fetches a packet. --CdG
int fetchPacket(char * recvBuf){
    
    int bytesRecvd;
    int retval = 0;
    char fromAddr[100];
    struct sockaddr_in theirAddr;
    socklen_t theirAddrLen;
    
    theirAddrLen = sizeof(theirAddr);
    if ((bytesRecvd = recvfrom(globalSocketUDP, recvBuf, 10000 , 0, 
                (struct sockaddr*)&theirAddr, &theirAddrLen)) == -1)
    {
        perror("connectivity listener: recvfrom failed");
        exit(1);
    }

    // String delimeter:
    recvBuf[bytesRecvd] = '\0';

    inet_ntop(AF_INET, &theirAddr.sin_addr, fromAddr, 100);
    
    short int heardFrom = -1;
    if(strstr(fromAddr, "10.1.1."))
    {
        heardFrom = atoi(
                strchr(strchr(strchr(fromAddr,'.')+1,'.')+1,'.')+1);
        
        // Mark the neighboor as connected: --CdG
        if (!neighboors[heardFrom].up){
            markNeighboorUp(heardFrom);
            retval = 1;
        }

        //record that we heard from heardFrom just now.
        gettimeofday(&globalLastHeartbeat[heardFrom], 0);
    }

    return retval;
}

// Handles non topography related packets
// Ie send, recieve, forward, cost
// Returns 0 if no costs changed.
// Returns 1 if a cost was changed. --CdG
int handleNonTopoPackets(char * recvBuf){
    int retval = 0;
    //Is it a packet from the manager? (see mp2 specification for more details)
    //send format: 'send'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
    if(!strncmp(recvBuf, "send", 4))
    {
        // send the requested message to the requested destination node --CdG
        routeMessage(recvBuf, YES);
    }
    //'cost'<4 ASCII bytes>, destID<net order 2 byte signed> newCost<net order 4 byte signed>
    else if(!strncmp(recvBuf, "cos", 3))
    {
        updateCost(recvBuf);
        retval = 1;
    }
    //forward format: 'fwrd'<4 ASCII bytes>, destID<net order 2 byte signed>, <some ASCII message>
    else if(!strncmp(recvBuf, "fwrd", 4))
    {
        // Forward packets via the table, or receive it if it is addressed to you -- CdG
        routeMessage(recvBuf, NO);
    }

    return retval;
}


//Yes, this is terrible. It's also terrible that, in Linux, a socket
//can't receive broadcast packets unless it's bound to INADDR_ANY,
//which we can't do in this assignment.
void hackyBroadcast(const char* buf, int length)
{
	int i;
	for(i=0;i<256;i++)
		if(i != globalMyID) //(although with a real broadcast you would also get the packet yourself)
			sendto(globalSocketUDP, buf, length, 0,
				  (struct sockaddr*)&globalNodeAddrs[i], sizeof(globalNodeAddrs[i]));
}

void* announceToNeighbors(void* unusedParam)
{
	struct timespec sleepFor;
	sleepFor.tv_sec = 0;
	sleepFor.tv_nsec = 700 * 1000 * 1000; //900 ms

    // Make this thread max priority
    struct sched_param params;
    pthread_t this_thread = pthread_self();
    params.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(this_thread, SCHED_FIFO, &params);

	while(1)
	{
		hackyBroadcast("H", 1);
		nanosleep(&sleepFor, 0);
	}
}


// UPdates the cost of a link given the message from the manager --CdG
int updateCost(char recvBuf[recvBufSize]){

    
    // record the cost change (remember, the link might currently be down! in that case,
    // this is the new cost you should treat it as having once it comes back up.)
    // Yeah, this is nasty I know: --CdG
    signed short destID = ntohs(*(short*)(&recvBuf[4]));
    signed int newCost = ntohl(*(int*)(&recvBuf[6]));

    if (destID > 255) // out of bounds
        return -1; // failure

    neighboors[destID].cost = newCost;

    if (recvBuf[3] == 't'){
        // Tell the neighboor that my cost has changed too: (make it bidirectional)
        recvBuf[3] = 's'; // so that the neighboor knows not to send this back.
        (*(short*)(&recvBuf[4])) = htons(globalMyID); // make me the node.
        sendPacket(recvBuf, recvBufSize, destID);
    }

    return 0; // success

}   

// Checks to see if any neighboors have become down.
// Returns 0 for no change in topography.
// Returns the number of neighboors that went down otherwise. --CdG
int checkDownNeighboors(int network_size){

    int i, j = 0;

    struct timeval now;
    struct timeval then;
    long delta;

    gettimeofday(&now, 0);

    for (i = 0; i < 256; i++){
        
        then = globalLastHeartbeat[i];
        // If last heartbeat was more than a second ago, mark as down:
        delta = ((now.tv_sec - then.tv_sec) * 1000000) + (now.tv_usec - then.tv_usec);
        if ((network_size < 50 && delta >= 950000) || (network_size >= 50 && delta >= 1250000) || (network_size >= 150 && delta >= 1750000)){
            if (neighboors[i].up){
                markNeighboorDown(i);
                newly_down[j] = i;
                j++;
            }
        }
    }

    return j;

}

// Mark a neighboor as up. --CdG
// Returns 0 if neighboor was already up.
// Returns 1 if the neighboor is newly up.
int markNeighboorUp(unsigned short ID){

    int retval = 0;

    if (ID > 255) // make sure it's valid.
        return 0;

    if (neighboors[ID].up == NO)
        retval = 1;

    neighboors[ID].up = YES;
    // routing_table[ID] = ID; // Route directly to this node.

    return retval;

}

// Mark a neighboor as down. --CdG
// Returns 0 if neighboor was already down.
// Returns 1 if the neighboor is newly down.
int markNeighboorDown(unsigned short ID){

    int retval = 0;

    if (ID > 255) // make sure it's valid.
        return 0;

    if (neighboors[ID].up == YES)
        retval = 1;

    neighboors[ID].up = NO;


    return retval;

}


// Logs event to file:
// General purpose, the args that aren't used for a given log entry can be anything, probably easiest to just make them 0.
// type {EVT_FWD, EVT_SND, EVT_REC, EVT_UNR}: Type of event to log
// dest : destination node
// next: next node to forward to
// msg: string message to log --CdG
void logEvent(short type, short dest, short next, char* msg){

    char log_buf[256]; // Bigger than it needs to be but just in case.
    
    switch(type){
        case EVT_FWD: // Forward Event
            sprintf(log_buf, "forward packet dest %d nexthop %d message %s\n", dest, next, msg);
            break;
        case EVT_REC: // Receive Event
            sprintf(log_buf, "receive packet message %s\n", msg);
            break;
        case EVT_SND: // Send Event
            sprintf(log_buf, "sending packet dest %d nexthop %d message %s\n", dest, next, msg);
            break;
        case EVT_UNR: // Dest Unreachable Event
            sprintf(log_buf, "unreachable dest %d\n", dest);
            break;
        default: return; // type was invalid return now.
    }

    fputs(log_buf, log_fd);
    fflush(log_fd); //Immediately write this to the file.
}

// Routes a given Message
// Inputs:  Send: Yes if a send NO if a forward.
// Cases:   Destination is itself - logs that it recieved.
//          Destination is in the routing table and it forwards it.
//          Destination is unreachable. --CdG
void routeMessage(char * recvBuf, short send){
    
    signed short destID = ntohs(*(short*)(&recvBuf[4]));
    char * msg = &recvBuf[6];

    if (destID == globalMyID){ // I am the destination
        logEvent(EVT_REC, destID, 0, msg);
        return;
    }

    signed short next_node = routing_table[destID];
    
    if (next_node == -1){ // Destination is unreachable
        logEvent(EVT_UNR, destID, 0, NULL);
        return;
    }

    // Forward the packet:
    if (send == YES) {
        recvBuf[0] = 'f';recvBuf[1] = 'w';recvBuf[2] = 'r';recvBuf[3] = 'd'; // convert this to a forward packet if it is a send.
        logEvent(EVT_SND, destID, next_node, msg);
    }
    else {
        logEvent(EVT_FWD, destID, next_node, msg);
    }

    // Actual packet send here:
    sendPacket(recvBuf, recvBufSize, next_node);

}


// Sends a packet in buf to the host with the dest ID. --CdG
// len = max length of buffer to send
void sendPacket(char * buf, unsigned int len,  short destID){

    // From manager_send.c:
    struct sockaddr_in destAddr;
	char tempaddr[100];
	sprintf(tempaddr, "10.1.1.%d", destID);
	memset(&destAddr, 0, sizeof(destAddr));
	destAddr.sin_family = AF_INET;
	destAddr.sin_port = htons(7777);
	inet_pton(AF_INET, tempaddr, &destAddr.sin_addr);
    
    sendto(globalSocketUDP, buf, len, 0, (struct sockaddr*)&destAddr, sizeof(destAddr));

}
