#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<netdb.h>
#include<sys/types.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<time.h>

#define PORT "3490"	//port number of the server
#define MAX 100000		//maximum buffer size

//get the sock address and return in network representation
void *get_addr(struct sockaddr *sa) { 
	if(sa -> sa_family == AF_INET)				
		return &(((struct sockaddr_in*)sa) -> sin_addr);	//for ipv4
	return &(((struct sockaddr_in6*)sa) -> sin6_addr);		//for ipv6
}

//creating a socket for client and returning the socket descriptor
int create_clientsocket(char *host) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);		//make the struct initially empty
        hints.ai_family = AF_UNSPEC;			//can be either ipv4 or ipv6
        hints.ai_socktype = SOCK_STREAM;		//TCP stream socket

	//gives a pointer to a linked list, servinfo of results
        if((rv = getaddrinfo(host, PORT, &hints, &servinfo)) != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                return 1;
        }
	
	//loop through each result in the servinfo and bind to the first you can
        for(p = servinfo; p != NULL; p = p -> ai_next) {
		//make a socket
                if((sockfd = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1) {
                        perror("client:  socket");
                        continue;
                }

		//connect the socket to the port and ip address of the server
                if(connect(sockfd, p -> ai_addr, p -> ai_addrlen) == -1) {
                        close(sockfd);
                        perror("client: connect");
                        continue;
                }

                break;
        }

	//convert the ip address from network to presentation format
        inet_ntop(p -> ai_family, get_addr((struct sockaddr *) p -> ai_addr), s, sizeof s);
        printf("client connecting to %s\n", s);

	//free the address space for servinfo, done with this structure
        freeaddrinfo(servinfo);

	return sockfd;
}

//send message to the server on sockfd
void sendmessage(int sockfd, char msg[]) {
	char buff[MAX];

	FILE *fd = fopen("file.txt", "r");
	int rv;
	if((rv = fread(&buff, 1, sizeof(buff), fd)) == -1) {
		perror("read");
		exit(1);
	}
	buff[rv - 1] = '\0';
	printf("%d %s", strlen(buff), buff);

	if(send(sockfd, buff, strlen(buff), 0) == -1) {
                perror("send");
                exit(1);
        }
        printf("Sent message to the server\n");
}

//receive the message from the server on sockfd
void recvmessage(int sockfd) {
	int n;			//number of bytes actually received
	char buf[MAX];
	if((n = recv(sockfd, buf, MAX - 1, 0)) == -1) {
                perror("recv");
                exit(1);
        }

        buf[n] = '\0';

        printf("client: received message from server - '%s'\n", buf);
        time_t t;
        time(&t);
        char *client_time = ctime(&t);
        printf("client: current time of the client - %s\n", client_time);
}

int main(int argc, char *argv[]) {
	int sockfd;
	char msg[30] = "Hello from Client";

	if(argc != 2) {
		fprintf(stderr,"usage: client hostname\n");
		exit(1);
	}

	sockfd = create_clientsocket(argv[1]);		//create a socket and bind it to the ip and port of the server

	sendmessage(sockfd, msg);			//send message to the server

	recvmessage(sockfd);				//receive message from the server

	close(sockfd);					//close the socket.

	return 0;
}
