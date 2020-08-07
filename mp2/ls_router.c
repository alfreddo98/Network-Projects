// Router that Implements LS Routing
// alfredo7 | cdgerth2

#include <pthread.h>
#include <time.h>
#include <inttypes.h>
#include <math.h>
#include "helper_fns.c"




// Declaration of locks and condition for advertisement
pthread_cond_t advr_cond = PTHREAD_COND_INITIALIZER; 
pthread_mutex_t advr_lock = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t neighboor_lock = PTHREAD_MUTEX_INITIALIZER; 


int advr_reasess = YES; // locked by advr_lock
int topo_reasses = NO;

int mySeqNum = 0;
int recent_up = YES;

// Declaration of locks and condition for dijk
pthread_cond_t dijk_cond = PTHREAD_COND_INITIALIZER; 
pthread_mutex_t dijkcond_lock = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t dijk_lock = PTHREAD_MUTEX_INITIALIZER; 

struct topology {
	int seqNum;
	int connectedTo[256];
	int cost[256];
	char in_topo;
};

// struct holding the current topology:
struct topology currentTopo[256];


// DIJK Vars:
char visited[256];
unsigned int dists[256];
unsigned int nearest[256];

// Function Declarations
void handleTopoPackets(char * recvBuf);
void * advertiseLS(void* unusedParam);
void * reasessTopo(void* unusedParam);
void dijk();

int main(int argcount, char ** args){

	int i, j;
    
    if(argcount != 4)
	{
		fprintf(stderr, "Usage: %s mynodeid initialcostsfile logfile\n\n", args[0]);
		exit(1);
	}

    // Perform all the initialization steps.
    init_router(args);

    // Start announce pthread
	pthread_t announcerThread;
	pthread_create(&announcerThread, 0, announceToNeighbors, (void*)0);

	// Start link state advertisment thread
	pthread_t advertismentThread;
	pthread_create(&advertismentThread, 0, advertiseLS, (void*)0);	
	
	// Start dijk background worker
	pthread_t dijkThread;
	pthread_create(&dijkThread, 0, reasessTopo, (void*)0);

	// Init topology
	for (i=0; i<256; i++){
		currentTopo[i].seqNum = -1;
		for (j=0; j<256; j++){
			currentTopo[i].connectedTo[j] = -1;
			currentTopo[i].cost[j] = 1;
			currentTopo[i].in_topo = NO;
		}
	}
	// put myself in the topology
	currentTopo[globalMyID].in_topo = YES;

    // Handle packets here ...

	unsigned char recvBuf[recvBufSize];
	int retval;
	
	while(1)
	{
		// pthread_mutex_lock(&neighboor_lock); // don't advertise while we update neighboors
		retval = fetchPacket(recvBuf); // get the packet.
		// pthread_mutex_unlock(&neighboor_lock); 

		if (retval){ // wake the advertisement thread if needed.
			pthread_mutex_lock(&advr_lock);
			advr_reasess = YES;
			pthread_mutex_unlock(&advr_lock);
			printf("N");
			if (mySeqNum > 2)
				recent_up = YES;
		}

		pthread_mutex_lock(&neighboor_lock); // don't advertise while we update neighboors
        retval = checkDownNeighboors(network_size); // will have to do something with this retval eventually
		pthread_mutex_unlock(&neighboor_lock); 

		if (retval){ // wake the advertisement thread if needed.
			pthread_mutex_lock(&advr_lock);
			advr_reasess = YES;
			pthread_mutex_unlock(&advr_lock);
			printf("D");
		}

		pthread_mutex_lock(&neighboor_lock); // don't advertise while we update neighboors
		pthread_mutex_lock(&dijk_lock); // Don't forward a packet while we are updating the routing table.
		retval = handleNonTopoPackets(recvBuf);
		pthread_mutex_unlock(&dijk_lock);
		pthread_mutex_unlock(&neighboor_lock); // don't advertise while we update neighboors


		if (retval){ // wake the advertisement thread if needed.
			pthread_mutex_lock(&advr_lock);
			advr_reasess = YES;
			pthread_mutex_unlock(&advr_lock);
		}

		pthread_mutex_lock(&advr_lock);
		if (advr_reasess)
			pthread_cond_signal(&advr_cond);
		pthread_mutex_unlock(&advr_lock);


		handleTopoPackets(recvBuf);
	}

	//(should never reach here)
    fclose(log_fd);
	close(globalSocketUDP);
    return 0;

}


void handleTopoPackets(char * recvBuf){

	// TODO: Handle packets that signal a change in topography.

	if(!strncmp(recvBuf, "advr", 4))
	{
		int i, h;
		char changed = NO;
		int j = 4 + sizeof(int);
		int hostID = *(int*)(&recvBuf[4]);
		int seqNum = *((int*)&recvBuf[j]);
		j += sizeof(unsigned int);


		// we had a loop so drop and don't do any more flooding
		if (seqNum <= currentTopo[hostID].seqNum)
			return;

		// set the sequence number:
		currentTopo[hostID].seqNum = seqNum;

		//if we have a packet, the node is in the topo.
		currentTopo[hostID].in_topo = YES;

		pthread_mutex_lock(&dijk_lock); // do not update table WHILE we are rerunning dijkstra's

		i = 0;
		// pull out the connected hosts:
		while (j < recvBufSize){
			if (*((unsigned int *)&recvBuf[j]) == 0)
				break;
			if (currentTopo[hostID].cost[i] != *((unsigned int *)&recvBuf[j])){
				changed = YES;
				currentTopo[hostID].cost[i] = *((unsigned int *)&recvBuf[j]);
			}
			j += sizeof(unsigned int);
			if (currentTopo[hostID].connectedTo[i] != (unsigned char)recvBuf[j]){
				currentTopo[currentTopo[hostID].connectedTo[i]].seqNum = -1;
				currentTopo[hostID].seqNum = -1;
				changed = YES;
				currentTopo[hostID].connectedTo[i] = (unsigned char)recvBuf[j];
			}
			i++;
			j++;
		}
		
		if (currentTopo[hostID].connectedTo[i] != -1){
				changed = YES;
				currentTopo[hostID].seqNum = -1;
		}

		j += sizeof(int);

		// Finish out the rest of it.
		for (; i<256; i++)
			if (currentTopo[hostID].connectedTo[i] != -1){
				currentTopo[currentTopo[hostID].connectedTo[i]].seqNum = -1;
				currentTopo[hostID].connectedTo[i] = -1;
			}
			else break;
		
		pthread_mutex_unlock(&dijk_lock);


		if (changed || recent_up){
			usleep((globalMyID * 4) % 777);
			// Forward this packet:
			for (i=0; i<256; i++){
				if (neighboors[i].up)
					sendPacket(recvBuf, j, i);
			}
		}

		// recompute the routing table.
		if (changed){
			pthread_mutex_lock(&dijkcond_lock);
			pthread_cond_signal(&dijk_cond);
			topo_reasses = YES;
			pthread_mutex_unlock(&dijkcond_lock);
		}
	}

}

void * advertiseLS(void* unusedParam){

	unsigned short i;
	int j;
	int offset;
	char packetBuf[recvBufSize];
	packetBuf[0] = 'a'; packetBuf[1] = 'd'; packetBuf[2] = 'v'; packetBuf[3] = 'r'; 
	*((int*)&packetBuf[4]) = globalMyID;

	struct timespec relTime;
	struct timespec now;

	pthread_mutex_lock(&advr_lock);
	advr_reasess = YES;
	pthread_mutex_unlock(&advr_lock);

	// sleep for random amount of time to avoid flooding the network on startup:
	// sleep(1);

	while(1)
	{
		// Advertise the current link state to the neighboors
		*((int*)&packetBuf[8]) = mySeqNum;

		if (advr_reasess){

			pthread_mutex_lock(&neighboor_lock); // don't read an incomplete neighboors block.

			pthread_mutex_lock(&advr_lock);
			advr_reasess = NO;
			pthread_mutex_unlock(&advr_lock);

			j = 4 + sizeof(globalMyID) + sizeof(mySeqNum);
			
			for (i=0; i<256; i++){
				if (neighboors[i].up){
					*((unsigned int *)&packetBuf[j]) = neighboors[i].cost;
					j += sizeof(unsigned int);
					*((unsigned short *)&packetBuf[j]) = i;
					j++;
				}
			}

			pthread_mutex_unlock(&neighboor_lock);

			*((unsigned int *)&packetBuf[j]) = 0; // End Sentinel
			j += sizeof(unsigned int);

			printf("x");
			fflush(stdout);
		}



		for (i=0; i<256; i++){
			if (neighboors[i].up)
				sendPacket(packetBuf, j, i);
		}

		// increment the sequence number
		mySeqNum++;

		clock_gettime(CLOCK_REALTIME, &now);
		relTime.tv_nsec = now.tv_nsec;
		relTime.tv_sec = now.tv_sec + 2;

		// Block until either: change in topology or 1 seconds is up.
		pthread_mutex_lock(&advr_lock);
		if (!advr_reasess)
			pthread_cond_timedwait(&advr_cond, &advr_lock, &relTime);
		pthread_mutex_unlock(&advr_lock);


		// Rest for a bit to not just attack the network all at once:
		sleep(1);

		// if (network_size > 100){
		// 	sleep(1);
		// } 
		// if (network_size > 70){
		// 	sleep(1.5);
		// }

	}

}

// Recomputes the routing table given the current Link states
void * reasessTopo(void * unusedParam){
	int i;
	
	struct timespec relTime;
	struct timespec now;

	while (1){

		pthread_mutex_lock(&dijkcond_lock);
		// Wait on the condition var:
		pthread_cond_wait(&dijk_cond, &dijkcond_lock);
		topo_reasses = NO;
		pthread_mutex_unlock(&dijkcond_lock);


		// Now wait a second in case a bunch of things just changed to avoid redoing this algorithm an obscene numer of times:
		while (1){

			clock_gettime(CLOCK_REALTIME, &now);
			relTime.tv_nsec = now.tv_nsec;
			relTime.tv_sec = now.tv_sec + 1;
			
			pthread_mutex_lock(&dijkcond_lock);
			pthread_cond_timedwait(&dijk_cond, &dijkcond_lock, &relTime);
			
			if (!topo_reasses){
				pthread_mutex_unlock(&dijkcond_lock);
				break;
			} else {
				recent_up = NO;
				topo_reasses = NO;
				pthread_mutex_unlock(&dijkcond_lock);
			}

		}

		pthread_mutex_lock(&dijk_lock); 

		printf(".");
		fflush(stdout);

		// None visited
		memset(visited, NO, 256);
		// Max dists
		memset(dists, 0x77, 256 * sizeof(unsigned int));
		dists[globalMyID] = 0;

		// Reset Routing table
		memset(routing_table, 0xFF, 256 * sizeof(short));
		
		// Reset Nearest:
		memset(nearest, 0x77, 256 * sizeof(int));
		nearest[globalMyID] = globalMyID;

		// Distance to self is 0.
		dists[globalMyID] = 0;

		network_size = 0;

		// Run Dijkstra's
		dijk(globalMyID);

		pthread_mutex_unlock(&dijk_lock);


	}
	// Never gets here.
}


// Runs dijkstra's algorithm and returns the best next path to some node
// returns the next node that should be used to get to the given dest.
void dijk(int node){

	int i, j, minNeighbor;
	int neighboor;

	network_size++;

	// Mark the tentative distance so far:
	for (i=0; i<256; i++){
		neighboor = (int)currentTopo[node].connectedTo[i];
		
		if (neighboor == -1){
			if (i==0)
				network_size--;
			break;
		}

		// base case of if we are processing the root node.
		if (node == globalMyID){
			routing_table[neighboor] = neighboor;
			dists[neighboor] = neighboors[neighboor].cost;
			nearest[neighboor] = globalMyID;
			continue;
		}

		// Update tentative costs:
		if ((unsigned int)(dists[node] + currentTopo[node].cost[i]) < dists[neighboor]){
			dists[neighboor] = dists[node] + currentTopo[node].cost[i];
			routing_table[neighboor] = routing_table[node];
			nearest[neighboor] = node;
		} 
		else if ((unsigned int)(dists[node] + currentTopo[node].cost[i]) == dists[neighboor]){ // check for if smaller ID
			if (node != globalMyID && (node < nearest[neighboor])){
				routing_table[neighboor] = routing_table[node];
				nearest[neighboor] = node;
			}
		}
	}
	
	// Mark the node as visited
	visited[node] = YES;

	for (j=0; j<256; j++){
		minNeighbor = -1;
		// Find the minimum neighboor that has not been visited:
		for (neighboor=0; neighboor<256; neighboor++){
			if (visited[neighboor] == NO && currentTopo[neighboor].in_topo){
				if (minNeighbor == -1)
					minNeighbor = neighboor;
				else if (dists[neighboor] < dists[minNeighbor])
					minNeighbor = neighboor;
				else if ((dists[neighboor] == dists[minNeighbor]) && (neighboor < minNeighbor))
					minNeighbor = neighboor;
			}
		}
		
		// we have visited all the neighboors
		if (minNeighbor == -1)
			return;

		// Recurse down on the neighboor:
		dijk(minNeighbor);


	}
	// Never Gets here
	return;
}