#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>

#include "buffers.c"

int create_socket();
void *get_in_addr(struct sockaddr *sa);
void reliablyReceive(unsigned short int myUDPport, char* destinationFile);

void * write_to_file(void * params);
void * send_acks(void * unusedParam);


pthread_mutex_t buf_lock = PTHREAD_MUTEX_INITIALIZER; // This locks the buffer across threads. Prevents data collisions.
pthread_mutex_t soc_lock = PTHREAD_MUTEX_INITIALIZER; // This locks the buffer across threads. Prevents data collisions.



int sock;
char done = NO;


payload_t ack;
struct sockaddr_in senderaddr;
int senderaddr_len;
unsigned short int udpPort;


int main(int argc, char** argv)
{
	
	if(argc != 3)
	{
		fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
		exit(1);
	}
	
	udpPort = (unsigned short int)atoi(argv[1]);

	create_socket();
	
	reliablyReceive(udpPort, argv[2]);

	close(sock);
}


int create_socket(){
		
		struct sockaddr_in bindAddr;
		memset(&bindAddr, 0, sizeof(bindAddr));

		sock = socket(AF_INET, SOCK_DGRAM, 0);

		// struct timeval read_timeout;
		// read_timeout.tv_sec = 0;
		// read_timeout.tv_usec = 10;
		// setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);

		bindAddr.sin_family = AF_INET;
		bindAddr.sin_port = htons(udpPort);
		bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		
		if(bind(sock, (struct sockaddr*)&bindAddr, sizeof(struct sockaddr_in)) < 0)
		{
			perror("bind");
			close(sock);
			exit(1);
		}

}

void *get_in_addr(struct sockaddr *sa){
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

payload_t * recv_payload(){

	payload_t * buffer;
	int count = 0;

	buffer = (payload_t *) malloc(sizeof(payload_t)); // alocate a buffer to read file data into

	// receive something
	recvfrom(sock, buffer, sizeof(*buffer), 0, (struct sockaddr *) &senderaddr, &senderaddr_len);

	pthread_mutex_lock(&buf_lock);

	return buffer;
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
	payload_t * buffer;

	int result;
	int ret;
	unsigned short recv_len;

	senderaddr_len = sizeof(senderaddr);

	// Start file write thread
	pthread_t fileThread;
	pthread_create(&fileThread, 0, write_to_file, (void*)destinationFile);


	while(1){

		// insert received packet:
		buffer = recv_payload();
		result = buffer_insert(buffer);
		recv_len = buffer->length;
		buffer->length = 0;
		sendto(sock, buffer, 4, 0, (struct sockaddr *) &senderaddr, senderaddr_len);
		buffer->length = recv_len;
		pthread_mutex_unlock(&buf_lock);

	}


}


void * write_to_file(void * params){
	payload_t * buffer;
	int total_packets;

	FILE * fhandle = fopen((char *)params, "w");
	if (!fhandle)
		perror("File open failed.");
	
	while (1){
		
		pthread_mutex_lock(&buf_lock);
		buffer = buffer_to_be_written();
		pthread_mutex_unlock(&buf_lock);	

		if (buffer){

			if (buffer->length == 0){
				done = YES;
				total_packets = buffer->seqNum;
				break;
			}
			
			fwrite(&(buffer->data), 1, buffer->length, fhandle);

			pthread_mutex_lock(&buf_lock);
			buffer_mark_written(buffer->seqNum);
			pthread_mutex_unlock(&buf_lock);	

		}

		if (done){
			break;
		}
	}

	fflush(fhandle);
	fclose(fhandle);

	printf("Total Packets: %d\n", total_packets);
	printf("Duplicate Packets: %u\n", duplicates);
	exit(0);

}