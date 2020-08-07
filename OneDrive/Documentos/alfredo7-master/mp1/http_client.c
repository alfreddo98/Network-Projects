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

void *get_in_addr(struct sockaddr *sa);
int create_socket(char *hostname, char *port);
void divide(char *input, char *hostname, char *path, char *port);
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


int main(int argc, char* argv[]) {
	int sockfd, numbytes;
	char buf[2048];          
	char hostname[512];    
	char port[6];           
	char path[512];      
	char buffer[4096];   	
	char status[32];		
	char location[1024];
	int n, i;


	if (argc != 2) {
		fprintf(stderr, "ERROR\n");
		exit(1);
	}

char input[1024];
strcpy(input, argv[1]);
do{
	divide(input, hostname, path, port);
		printf("%s\n", hostname);
		sockfd =  create_socket(hostname, port);
		char header[1024];
		printf("http_client: sending GET\n");
		sprintf(header, "GET %s HTTP/1.0\r\n", path);
		send(sockfd, header, strlen(header), 0);
		sprintf(header, "Host: %s\r\n", hostname);
		send(sockfd, header, strlen(header), 0);
		strcpy(header, "Connection: close\r\n");
		send(sockfd, header, strlen(header), 0);
		strcpy(header, "\r\n\r\n");
		send(sockfd, header, strlen(header), 0);

	n = readline(sockfd, buffer, sizeof(buffer));
		for (i = 0; i < n; i++) {
			if (buffer[i] == ' ') {
				strcpy(status, buffer+i+1);
				break;
			}
		}
		n = readline(sockfd, buffer, sizeof(buffer));
		buffer[n]='\0';
		for (i = 0; i < n; i++) {
			if (buffer[i] == '/') {
				strcpy(input, buffer+i+2);
				break;
			} 
		}
	 }while(strncmp(status, "301 Moved Permanently", 21) == 0);

	FILE *f = fopen("output", "w+");
	n = read(sockfd, buffer, sizeof(buffer));
	buffer[n] = '\0';
	char *temp = strstr(buffer, "\r\n\r\n");

	fprintf(f, "%s", temp+4);


	while((n = read(sockfd, buffer, sizeof(buffer))) != 0) {
		buffer[n] = '\0';
			printf("%s",buffer);
			fprintf(f,"%s" ,buffer);
    }
    fclose(f);

    return 0;
}
int create_socket(char *hostname, char *port){
		struct addrinfo hints, *servinfo;   
		int option = 1;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;

		if((getaddrinfo(hostname, port, &hints, &servinfo)) != 0) {
			perror("NO funciona");
			exit(1);
		}


		int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
		setsockopt(sockfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), &option, sizeof(option));
		connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);

/*		if (p == NULL) {
			fprintf(stderr, "http_client: unable to connect to %s\n", hostname);
			exit(1);
		}
*/		
		freeaddrinfo(servinfo);
		return sockfd;

}
void divide(char *input, char *hostname, char *path, char *port){
		char temporal[strlen(input)];
		strcpy(temporal, input+7);
		printf("Llegamos aqui\n");

		char *pch;
		pch = strchr(temporal, ':');
		if (pch != NULL) {
			pch[0]='\0';
			strcpy(hostname, temporal);
			char *pch1;
			pch1 = strchr(pch+1, '/');
			if(pch!=NULL){
				pch1[0]='\0';
				strcpy(path, pch1+1);
			}else{
				strcpy(path, "/");
			}
			strcpy(port, pch+1);

		} else {
			strcpy(port, "80");
			char *pch1;
			pch1 = strchr(temporal, '/');
			if(pch1!=NULL){
				pch1[0]='\0';
				strcpy(path, pch1+1);
				if(strncmp(path," ", 1)!=0){
					strcpy(path,"/index.html");
				}
			}else{
				strcpy(path, "/");
			}
			strcpy(hostname, temporal);

		}
		printf("Llegamos aqui\n");
}
