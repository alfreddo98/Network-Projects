/*
** client.c -- a stream socket client demo
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
//#define PORT "5900" 
//#define URL "cs438.cs.illinois.edu"
#define MAXDATASIZE 100 // max number of bytes we can get at once 

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc != 4) {
	    fprintf(stderr,"usage: client hostname or port\n");
	    exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
			s, sizeof s);
	//printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

//Perfoming the handshake.

	strcpy(buf, "HELO\n\0");
	if ((numbytes = send(sockfd, buf, strlen(buf), 0))==-1) {
	    perror("send");
	    exit(1);
	}

	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	    perror("recv");
	    exit(1);
	}

	buf[numbytes] = '\0';
	printf("client: received '%s'\n",buf);

//Sending the username.

	strcpy(buf, "USERNAME ");
	strcat(buf, argv[3]);
	strcat(buf, "\n");
	if ((numbytes = send(sockfd, buf, strlen(buf), 0))==-1) {
	    perror("send");
	    exit(1);
	}

	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	    perror("recv");
	    exit(1);
	}
//Using a loop for printing 10 times the respond.
	int a = 1;
	while(a<=10){
		strcpy(buf, "RECV\n\0");
		if ((numbytes = send(sockfd, buf, strlen(buf), 0))==-1) {
	    		perror("send");
	    		exit(1);
		}

		if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	    		perror("recv");
	    		exit(1);
		}
	
	buf[numbytes] = '\0';
	printf("Received: %s", buf + 12);
	}
	close(sockfd);
//Sending the close statement
	strcpy(buf, "BYE\n\0");
	if ((numbytes = send(sockfd, buf, strlen(buf), 0))==-1) {
	    perror("send");
	    exit(1);
	}

	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
	    perror("recv");
	    exit(1);
	}
	buf[numbytes] = '\0';

	if(close(sockfd)==-1){
		perror("Error while closing the socket");
		exit(1);
	}

	close(sockfd);

	return 0;
}

