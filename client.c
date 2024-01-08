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

#define PORT "3490"
#define MAX 100

void *get_addr(struct sockaddr *sa) { 
	if(sa -> sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa) -> sin_addr);
	return &(((struct sockaddr_in6*)sa) -> sin6_addr);
}

int main(int argc, char *argv[]) {
	int sockfd, n;
	char buf[MAX];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if(argc != 2) {
		fprintf(stderr,"usage: client hostname\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

	for(p = servinfo; p != NULL; p = p -> ai_next) {
		if((sockfd = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1) {
			perror("clinet:  socket");
			continue;
		}

		if(connect(sockfd, p -> ai_addr, p -> ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}

		break;
	}

	inet_ntop(p -> ai_family, get_addr((struct sockaddr *) p -> ai_addr), s, sizeof s);
	printf("client connecting to %s\n", s);

	freeaddrinfo(servinfo);

	char msg[30] = "Hello from Client";
	if(send(sockfd, msg, 30, 0) == -1) {
		perror("send");
		exit(1);
	}
	printf("Sent message to the server\n");

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

	close(sockfd);

	return 0;
}
