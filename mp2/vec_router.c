// Router that Implements VEC Routing
// alfredo7 | cdgerth2


#include <pthread.h>
#include <time.h>
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <fcntl.h>
#include "helper_fns.c"


void handleTopoPackets(char * recvBuf);
void * advertiseVector(void * unused_param);
void createTable();



// Declaration of locks and condition for advertisement
pthread_cond_t advr_cond = PTHREAD_COND_INITIALIZER; 
pthread_mutex_t advr_lock = PTHREAD_MUTEX_INITIALIZER; 
pthread_mutex_t vector_lock = PTHREAD_MUTEX_INITIALIZER; 

char advr_reasess = YES; // locked by advr_lock
char down_date = NO;
char poisoned = NO;


struct vectors {
	int distance;
	int successor;
	char advertise;
};
struct vectors vector[256];


int main(int argcount, char ** args){
    
    int i, j, k;

    if(argcount != 4)
	{
		fprintf(stderr, "Usage: %s mynodeid initialcostsfile logfile\n\n", args[0]);
		exit(1);
	}

    // Perform all the initialization steps.
    init_router(args);
    createTable();
    // Start announce pthread
	pthread_t announcerThread;
	pthread_create(&announcerThread, 0, announceToNeighbors, (void*)0);

	// Start advertize pthread
	pthread_t advertizerThread;
	pthread_create(&advertizerThread, 0, advertiseVector, (void*)0);

	unsigned char recvBuf[recvBufSize];
	int new_host = NO;
	int num_down = 0;
	int cost_changed = NO;
	
	while(1)
	{
		new_host = fetchPacket(recvBuf); // get the packet.

		if (new_host){ // Immediately send out an advertisement.
			pthread_mutex_lock(&advr_lock);
			advr_reasess = YES;
			pthread_mutex_unlock(&advr_lock);
		}

		pthread_mutex_lock(&vector_lock); // don't advertise while we update neighboors
        num_down = checkDownNeighboors(network_size); // will have to do something with this retval eventually
		pthread_mutex_unlock(&vector_lock); // don't advertise while we update neighboors
		


		pthread_mutex_lock(&vector_lock); // don't advertise while we update neighboors
		cost_changed = handleNonTopoPackets(recvBuf);
		pthread_mutex_unlock(&vector_lock); // don't advertise while we update neighboors


		if (cost_changed){
			pthread_mutex_lock(&advr_lock);
			advr_reasess = YES;
			pthread_mutex_unlock(&advr_lock);
			//printf("C");	
		}
		
		handleTopoPackets(recvBuf);

		if (num_down){
			pthread_mutex_lock(&advr_lock);
			advr_reasess = YES;
			down_date = YES;
			pthread_mutex_unlock(&advr_lock);

			for (i=0; i<num_down; i++){
				vector[newly_down[i]].distance = INT_MAX;
				routing_table[newly_down[i]] = -1;
				for (k=0; k<256; k++){
					if (vector[k].successor == newly_down[i]){
						routing_table[k] = -1;
						vector[k].distance = INT_MAX;
					}
				}
			}
		}

		pthread_mutex_lock(&advr_lock);
		if (advr_reasess == YES){
			pthread_cond_signal(&advr_cond);
		}
		pthread_mutex_unlock(&advr_lock);

		if (num_down){ // Now go to sleep and forget the world to fix routing loops
			num_down = 0;
			usleep(10);
			fcntl(globalSocketUDP, F_SETFL, fcntl(globalSocketUDP, F_GETFL) | O_NONBLOCK);
			while (recv(globalSocketUDP, NULL, 100000, 0) != -1) ;
			fcntl(globalSocketUDP, F_SETFL, fcntl(globalSocketUDP, F_GETFL) & !O_NONBLOCK);
		}

	}

	//(should never reach here)
    fclose(log_fd);
	close(globalSocketUDP);
    return 0;

}


void handleTopoPackets(char * recvBuf){

	if(!strncmp(recvBuf, "advr", 4))
	{

		int i, k, j = 4, dest, dist, origin, ttl;
		char changed;

		origin = *((int*)&recvBuf[j]); // this is the origin of the packet. (will be one of the neighboors.)
		*((int*)&recvBuf[j]) = globalMyID; // make myself the origin of this packet that I will forward.
		j += sizeof(int);

		ttl = *((int*)&recvBuf[j]);

		if (ttl > 0) {

			// Update time to live.
			ttl--;
			*((int*)&recvBuf[j]) = ttl;
			j += sizeof(int);

			pthread_mutex_lock(&vector_lock);


			changed = NO;
			//We will compare if the distance is 
			for(i=0;i<256;i++){
				// Read the destination and the distance it currently thinks it is.
				dest = *((int*)&recvBuf[j]);
				
				if (dest == -1) break; // we reached the end sentinel
				if (dest > 255) break; // we have an error

				vector[dest].advertise = YES;
				dist = *((int*)&recvBuf[j+sizeof(int)]);



				// Check if the distance to the destination through the origin node is less than the current distance we have on file:
				// OR if this neighboor is giving us an update distance that has a higher cost.
				if (dist == INT_MAX){
					if (origin == vector[dest].successor) {
						if (routing_table[dest] != -1){
							routing_table[dest] = -1;
							vector[dest].distance = INT_MAX;
							changed = YES;
							pthread_mutex_lock(&advr_lock);
							down_date = YES;
							pthread_mutex_unlock(&advr_lock);
						}
					}
				} else if (((dist + neighboors[origin].cost) < vector[dest].distance) || (((dist + neighboors[origin].cost) != vector[dest].distance) && (vector[dest].successor == origin))){
					vector[dest].distance = dist + neighboors[origin].cost;
					routing_table[dest] = origin;
					vector[dest].successor = origin;
					changed = YES;


				} else if ((dist + neighboors[origin].cost) == vector[dest].distance){ // tie breaking
					if (origin < vector[dest].successor){
						routing_table[dest] = origin;
						if (vector[dest].successor != origin){
							vector[dest].successor = origin;
							changed = YES;
						}
					}
				} 

				if (routing_table[dest] != -1){
					dist += neighboors[origin].cost; // add the cost to this node to the distance.
					*((int*)&recvBuf[j+sizeof(int)]) = dist;
				}

				j += 2 * sizeof(int);
			}

			pthread_mutex_unlock(&vector_lock);

			
			if (changed){
				pthread_mutex_lock(&advr_lock);
				advr_reasess = YES;
				pthread_cond_signal(&advr_cond);
				pthread_mutex_unlock(&advr_lock);	
				
				// DEBUG:
				printf(".");
				fflush(stdout);
			}


		}

	}

}

void * advertiseVector(void* unusedParam){
//We will send a message with "advrMyIDDestinationDistance" The destination will be all the vectors known (not a distance of infinity) in the table. The distance will be the one that will be in the table. 
	unsigned short i;
	int j = 4;
	int k;
	char packetBuf[recvBufSize];
	char tmpBuf[recvBufSize];
	int packetBuflen;
	int dest;
	int offset = 1;

	int network_size = 0;

	packetBuf[0] = 'a'; packetBuf[1] = 'd'; packetBuf[2] = 'v'; packetBuf[3] = 'r'; 
	*((int*)&packetBuf[j]) = globalMyID; // Set myself as the origin.

	// sleep for random amount of time to avoid flooding the network on startup:
	usleep(globalMyID);

	struct timespec relTime;
	struct timespec now;

	pthread_mutex_lock(&advr_lock);
	advr_reasess = YES;
	pthread_mutex_unlock(&advr_lock);


	
	while(1)
	{
		// Sending destination and distance

		printf("x");
		fflush(stdout);

		if (advr_reasess){
			pthread_mutex_lock(&advr_lock);
			advr_reasess = NO;
			pthread_mutex_unlock(&advr_lock);

			pthread_mutex_lock(&vector_lock); // don't read an incomplete neighboors block.
			
			
			j = 4 + sizeof(int);

			*((int *)&packetBuf[j]) = 20; // time to live
			j += sizeof(int);

			network_size = down_date ? network_size : 0;

			for (i=0; i<256; i++){

				if (neighboors[i].up){ // perform checking to make sure that we have the min cost for directly connected nodes.
					vector[i].advertise = YES;
					if (neighboors[i].cost < vector[i].distance){
						vector[i].distance = neighboors[i].cost;
						vector[i].successor = i;
						routing_table[i] = i;
					} else if (neighboors[i].cost == vector[i].distance){
						if (i < vector[i].successor){
							vector[i].successor = i;
							routing_table[i] = i;
						}
					}
				}

				if ( ( !down_date && vector[i].advertise) || (down_date && vector[i].advertise && vector[i].distance == INT_MAX)){ // only forward if we have or had some sort of route
					*((int *)&packetBuf[j]) = i;
					j += sizeof(int);
					*((int *)&packetBuf[j]) = vector[i].distance;
					j += sizeof(int);
					network_size += down_date ? 0 : 1;
					vector[i].advertise = down_date ? 0 : 1;
				}
			
			}

			pthread_mutex_lock(&advr_lock);
			down_date = NO;
			pthread_mutex_unlock(&advr_lock);

			*((unsigned int *)&packetBuf[j]) = -1; // End Sentinel
			j += sizeof(int);
			packetBuflen = j; 

			pthread_mutex_unlock(&vector_lock); // don't read an incomplete neighboors block.
		}


		for (i=0; i<256; i++){
			if (neighboors[i].up){

				memcpy(tmpBuf, packetBuf, packetBuflen);
				j = 4 + (2 * sizeof(int));

				for(k=0;k<256;k++){
					// Read the destination and the distance it currently thinks it is.
					dest = *((int*)&tmpBuf[j]);
					
					if (dest == -1) break; // we reached the end sentinel
					if (dest > 255) break; // we have an error
					j += sizeof(int);

					if (vector[dest].successor == i && i != globalMyID)
						*((int*)&tmpBuf[j]) = INT_MAX;

					j += sizeof(int);
					
				}
				j+= sizeof(int);
				sendPacket(tmpBuf, j, i);
			}
		}

		clock_gettime(CLOCK_REALTIME, &now);
		relTime.tv_nsec = now.tv_nsec;
		if (network_size > 100) offset = 2;
		else offset = 1;
		relTime.tv_sec = now.tv_sec + offset;

		usleep(3 * network_size);

		// Block until either: change in topology or 1 seconds is up.
		pthread_mutex_lock(&advr_lock);
		if (!advr_reasess)
			pthread_cond_timedwait(&advr_cond, &advr_lock, &relTime);
		pthread_mutex_unlock(&advr_lock);

	}
}

void createTable(){
	//We create a table in which it will introduce the neigboors distance, destination and successor, if they are not neighboors, the distance will be 16 (That is infinity).
	int i;
	pthread_mutex_lock(&vector_lock);
	for(i=0;i<256;i++){
		vector[i].distance = INT_MAX;
		vector[i].advertise = NO;
		vector[i].successor = globalMyID;
		routing_table[i] = -1;
		neighboors[i].up = NO;
	}
	vector[globalMyID].distance = 0;
	vector[globalMyID].advertise = YES;
	routing_table[globalMyID] = globalMyID;

	pthread_mutex_unlock(&vector_lock);

}