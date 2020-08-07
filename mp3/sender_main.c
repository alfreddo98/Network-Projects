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


void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer);
void create_socket();
void read_file(char* filename, unsigned long long int bytesToTransfer);
int send_packet(payload_t * payload, int sock);
void * ack_listen(void* unusedParam);
void * resend_lost(void* unusedParam);


int ack;
int sock;
unsigned char sender_end = NO;

struct sockaddr_in receiveraddr;

pthread_mutex_t buf_lock = PTHREAD_MUTEX_INITIALIZER; // This locks the buffer across threads. Prevents data collisions.


unsigned short int udpPort;
char * hostname;


int main(int argc, char** argv)
{
	unsigned long long int numBytes;
	
	if(argc != 5)
	{
		fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
		exit(1);
	}
	
	udpPort = (unsigned short int)atoi(argv[2]);
	numBytes = atoll(argv[4]);
	hostname = argv[1];

	init_buffers();
	create_socket(argv[1], udpPort);

	// Start listen for ack thread
	pthread_t ackThread;
	pthread_create(&ackThread, 0, ack_listen, (void*)0);

	// // Start resend thread
	// pthread_t resendThread;
	// pthread_create(&resendThread, 0, resend_lost, (void*)0);

	// Begin the transfer
	reliablyTransfer(argv[1], udpPort, argv[3], numBytes);

	close(sock);
} 

void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {

	read_file(filename, bytesToTransfer);

}


//Function that creates a socket, for the function send packet, it return the socket.
void create_socket(){ // sender and receiver will listen on the same port.
		
		struct sockaddr_in bindAddr;
		memset(&bindAddr, 0, sizeof(bindAddr));

		sock = socket(AF_INET, SOCK_DGRAM, 0);

		bindAddr.sin_family = AF_INET;
		bindAddr.sin_port = 0; // auto assign a port
		bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		
		if(bind(sock, (struct sockaddr*)&bindAddr, sizeof(struct sockaddr_in)) < 0)
		{
			perror("bind");
			close(sock);
			exit(1);
		}

		// Now build the client data structs:

		struct hostent *receiver;

		receiver = gethostbyname(hostname);

		if (receiver == NULL){
			perror("DNS Query");
		}
		
		memset(&receiveraddr, 0, sizeof(receiveraddr));
		receiveraddr.sin_family = AF_INET;
		receiveraddr.sin_port = htons(udpPort);
		receiveraddr.sin_addr.s_addr = inet_addr(hostname); // THis assumes that the hostname will come as an ip address...

}

//Function that reads the file given.
void read_file(char* filename, unsigned long long int bytesToTransfer){
	payload_t * buffer;
	payload_t * resend_buffer;
	unsigned char counter = 0;
	unsigned short current_packet_size = 1468;
	struct timespec delay;
	delay.tv_sec = 0;
	int result;
	unsigned long bytes_read = 0;
	FILE *fp = fopen(filename, "rb");
	int read_len;
	if(fp == NULL){
		perror("File error");
		exit(1);
	}

	read_len = 1;
	//We copy the file into the buffer payload by payload:
	while(bytes_read < bytesToTransfer){

		counter++;

		pthread_mutex_lock(&buf_lock);
		resend_buffer = buffer_to_be_resent();
		pthread_mutex_unlock(&buf_lock);
		
		if (resend_buffer){
			send_packet(resend_buffer, sock);
		} else {

			do {
				buffer = (payload_t *) malloc(sizeof(payload_t)); // alocate a buffer to read file data into
			} while (!buffer); // make sure we aren't handed a null buffer
			
			
			read_len = fread(&(((unsigned short *)buffer)[2]), sizeof(char), MIN(current_packet_size, bytesToTransfer - bytes_read), fp); // read the file into that part of the buffer

			if (read_len == 0){
				printf("File is not as long as the number bytes you asked me to send.");
				return;
			}

			bytes_read += read_len;

			((unsigned short *)buffer)[0] = read_len; // put the length of the data in this packet into the payload

			pthread_mutex_lock(&buf_lock);
			result = buffer_insert(buffer);
			pthread_mutex_unlock(&buf_lock);

			while (result == -1){	
				current_send_delay();
				pthread_mutex_lock(&buf_lock);
				result = buffer_insert(buffer);
				pthread_mutex_unlock(&buf_lock);
			}
			
			pthread_mutex_lock(&buf_lock);
			send_packet(buffer, sock);
			pthread_mutex_unlock(&buf_lock);
		}

		if ((counter % BURST_SIZE) == 0){
			current_send_delay();
			counter = 0;
		}


	}

	// send a packet with no data (the end sentinel)
	buffer = (payload_t *) malloc(sizeof(payload_t)); // alocate a buffer to read file data into
	buffer->length = 0;
	pthread_mutex_lock(&buf_lock);
	buffer_insert(buffer);
	int total_sent = buffer->seqNum;
	send_packet(buffer, sock);
	pthread_mutex_unlock(&buf_lock);


	// Wait for evertyhing to be ack'ed and then exit:
	while (!sender_done()){
		pthread_mutex_lock(&buf_lock);
		resend_buffer = buffer_to_be_resent();
		pthread_mutex_unlock(&buf_lock);
		
		if (resend_buffer){
			send_packet(resend_buffer, sock);
		}
		current_send_delay();
	}

	fclose(fp);

	float loss = (float)total_sent / (float)(total_sent + resent);
	printf("Packet Loss: %f%%\n", loss);
}

//Function that sends a packet, returns the number of bytes sent.
int send_packet(payload_t * payload, int sock){

	if (!payload) // sanity check
		return -1;

	sendto(sock, payload, payload->length + 4, 0, (struct sockaddr *) &receiveraddr, sizeof(receiveraddr));
	
	return 0;
}

void * ack_listen(void* unusedParam){
	payload_t buffer;
	socklen_t socksize = sizeof(receiveraddr);

	while (1){
		recvfrom(sock, &buffer, 4, 0, (struct sockaddr *) &receiveraddr, &socksize);
		if (buffer.length == 0){
			pthread_mutex_lock(&buf_lock);
			buffer_mark_acked(buffer.seqNum);
			pthread_mutex_unlock(&buf_lock);
		}
	}

}