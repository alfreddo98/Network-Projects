#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#define BACKLOG 10	 		

void sigchld_handler(int s);
void *get_in_addr(struct sockaddr *sa);
int readline(int socket, char *buffer, int size) ;
int main(int argc, char *argv[]);

void sigchld_handler(int s) {
	(void)s;
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int readline(int socket, char *buffer, int size) {
	int i = 0;
	char chr = '\0';
	int line_size;

	while((i < size - 1) && (chr != '\n')) {
		line_size = recv(socket, &chr, 1, 0);
		if (line_size > 0) {

			if (chr == '\r') {
				line_size = recv(socket, &chr, 1, MSG_PEEK);
				if ((line_size > 0) && (chr == '\n'))
					recv(socket, &chr, 1, 0);
				else
					chr = '\n';
			}
			buffer[i] = chr;
			i++;
		}
		else {
			chr = '\n';
		}
	}
	buffer[i] = '\0';
	return i;
}

int main(int argc, char *argv[]) {
	int sockfd, client_fd;
	int option = 1;  
	struct sockaddr_storage their_addr;
	socklen_t sin_size;
	struct sigaction sa;
	char s[INET6_ADDRSTRLEN];

    if (argc != 2) {
        fprintf(stderr, "A port is needed\n");
        exit(1);
    }
	struct addrinfo hints, *servinfo, *p;
	int rv;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; 

	if ((rv = getaddrinfo(NULL, argv[1] , &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("http_server: socket");
			continue;
		}

		setsockopt(sockfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), &option, sizeof(option));

		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("http_server: bind");
			continue;
		}

		break;
	}

	printf("Socket in %s\n", argv[1]);
	freeaddrinfo(servinfo); 
	if (listen(sockfd, BACKLOG) == -1) {
		perror("escuchar");
		exit(1);
	}
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("asignacion");
		exit(1);
	}

	printf("listening...\n");

	while(1) {
		sin_size = sizeof their_addr;
		client_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if (client_fd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("\nConnected to %s\n", s);

		if (!fork()) { 
			close(sockfd);
	char request_line[1024];
	int n;
	n = readline(client_fd, request_line, sizeof(request_line));

	printf("request: %s\n", request_line);

	char discard[256];
	discard[0] = 'A'; discard[1] = '\0';
	while((n > 0) && strcmp(discard, "\n")) {
		readline(client_fd, discard, sizeof(discard));
	}

	char method[32];
	char uri[256];
	char version[32];
	int len = strlen(request_line);	
	char temp[len];				
	strcpy(temp, request_line);	

	char *pch;

	pch = strtok(request_line, " ");
	if (pch) {
		strcpy(method, pch);
	}
	else {
		strcpy(request_line, temp);
	}

	pch = strtok(NULL, " ");	
	if (pch) {
		strcpy(uri, pch);
	}
	else {
		strcpy(request_line, temp);
	}

	pch = strtok(NULL, "\n");
	if (pch) {
		strcpy(version, pch);
	}
	else {
		strcpy(request_line, temp);
	}

	strcpy(request_line, temp);	
	if (rv == -1) {
	char buffer1[1024];

	sprintf(buffer1, "HTTP/1.0 400 Bad Request\r\n");
	send(client_fd, buffer1, strlen(buffer1), 0);
	sprintf(buffer1, "Content-Type: text/html\r\n");
	send(client_fd, buffer1, strlen(buffer1), 0);
	sprintf(buffer1, "\r\n");
	send(client_fd, buffer1, strlen(buffer1), 0);
	sprintf(buffer1, "<HTML><TITLE>Bad Request</TITLE>\r\n");
	send(client_fd, buffer1, strlen(buffer1), 0);
	sprintf(buffer1, "<BODY><P>The server could not fulfill\r\n");
	send(client_fd, buffer1, strlen(buffer1), 0);
	sprintf(buffer1, "your request because the request contained\r\n");
	send(client_fd, buffer1, strlen(buffer1), 0);
	sprintf(buffer1, "an error or that feature has not been implemented.</P>\r\n");
	send(client_fd, buffer1, strlen(buffer1), 0);
	sprintf(buffer1, "</BODY></HTML>\r\n");
	send(client_fd, buffer1, strlen(buffer1), 0);
	}

	if (strcmp(method, "GET") != 0)	{	
	char buffer2[1024];

	sprintf(buffer2, "HTTP/1.0 400 Bad Request\r\n");
	send(client_fd, buffer2, strlen(buffer2), 0);
	sprintf(buffer2, "Content-Type: text/html\r\n");
	send(client_fd, buffer2, strlen(buffer2), 0);
	sprintf(buffer2, "\r\n");
	send(client_fd, buffer2, strlen(buffer2), 0);
	sprintf(buffer2, "<HTML><TITLE>Bad Request</TITLE>\r\n");
	send(client_fd, buffer2, strlen(buffer2), 0);
	sprintf(buffer2, "<BODY><P>The server could not fulfill\r\n");
	send(client_fd, buffer2, strlen(buffer2), 0);
	sprintf(buffer2, "your request because the request contained\r\n");
	send(client_fd, buffer2, strlen(buffer2), 0);
	sprintf(buffer2, "an error or that feature has not been implemented.</P>\r\n");
	send(client_fd, buffer2, strlen(buffer2), 0);
	sprintf(buffer2, "</BODY></HTML>\r\n");
	send(client_fd, buffer2, strlen(buffer2), 0);
	}
	char file[256];			
	strcpy(file, uri+1);	
	FILE *fptr, *sock;
	fptr = NULL;

	fptr = fopen(file, "r");
	if (fptr == NULL) {
	char buffer3[1024];

	sprintf(buffer3, "HTTP/1.0 404 Not Found\r\n");
	send(client_fd, buffer3, strlen(buffer3), 0);
	sprintf(buffer3, "Content-Type: text/html\r\n");
	send(client_fd, buffer3, strlen(buffer3), 0);
	sprintf(buffer3, "\r\n");
	send(client_fd, buffer3, strlen(buffer3), 0);
	sprintf(buffer3, "<HTML><TITLE>Not Found</TITLE>\r\n");
	send(client_fd, buffer3, strlen(buffer3), 0);
	sprintf(buffer3, "<BODY><P>The server could not fulfill\r\n");
	send(client_fd, buffer3, strlen(buffer3), 0);
	sprintf(buffer3, "your request because the resource specified\r\n");
	send(client_fd, buffer3, strlen(buffer3), 0);
	sprintf(buffer3, "is unavailable or nonexistent.</P>\r\n");
	send(client_fd, buffer3, strlen(buffer3), 0);
	sprintf(buffer3, "</BODY></HTML>\r\n");
	send(client_fd, buffer3, strlen(buffer3), 0);
	}

	char buffer4[1024];
	(void)file;	

	sprintf(buffer4, "HTTP/1.0 200 OK\r\n");
	send(client_fd, buffer4, strlen(buffer4), 0);
	sprintf(buffer4, "Content-Type: text\r\n");
	send(client_fd, buffer4, strlen(buffer4), 0);
	sprintf(buffer4, "\r\n");
	send(client_fd, buffer4, strlen(buffer4), 0);
	sock = fdopen(client_fd, "w");
	char buffer5[4096];

	do {
		n = fread(buffer5, 1, sizeof(buffer5), fptr);
		if (n < sizeof(buffer5)) {
			if (!feof(fptr)) {
				fprintf(stderr, "ERROR");
				break;
			}
		}
		fwrite(buffer5, 1, n, sock);
	} while(!feof(fptr));

	fclose(sock);
	fclose(fptr);
	close(client_fd);
	exit(0);
		}
		close(client_fd);
	}
	return 0;
}