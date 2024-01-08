#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<sys/wait.h>
#include<signal.h>
#include<time.h>

#define PORT "3490"
#define BACKLOG 10
#define MAX 100

void sig_handler(int s) {
	int saved_errno = errno;
	while(waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}

void *get_addr(struct sockaddr *sa ) {
	if(sa -> sa_family == AF_INET)
		return &(((struct sockaddr_in*) sa) -> sin_addr);
	return &(((struct sockaddr_in6*) sa) -> sin6_addr);
}

int main() {
	int sockfd, newfd;

	struct addrinfo hints, *servinfo, *p;
	struct sockaddr_storage their_addr;
	socklen_t sin_size;
	struct sigaction sa;

	int yes = 1;
	char s[INET6_ADDRSTRLEN];
	int rv;

	int n;
	char buf[MAX];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if((rv = getaddrinfo(NULL, PORT, &hints, &servinfo))) {
		fprintf(stderr, "getaddrinfo: %s", gai_strerror(rv));
		return 1;
	}

	for(p = servinfo; p != NULL; p = p -> ai_next) {		
		if((sockfd = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}

		if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			perror("sockopt");
			exit(1);
		}

		if(bind(sockfd, p -> ai_addr, p -> ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}
		printf("Socket created\n");
		break;
	}

	freeaddrinfo(servinfo);

	if(p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		exit(1);
	}

	printf("Socket binded\n");

	if(listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sig_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	if(sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connections...\n");

	while(1) {
		sin_size = sizeof their_addr;
		newfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
		if(newfd == -1) {
			perror("accept");
			continue;
		}

		inet_ntop(their_addr.ss_family, get_addr((struct sockaddr *) &their_addr), s, sizeof s);
		printf("server: got connection from %s\n", s);

		if(!fork()) {
			close(sockfd);
			if((n = recv(newfd, buf, MAX - 1, 0)) == -1)
				perror("recv");
			printf("Server: received '%s' from client", buf);
			time_t t;
			time(&t);
			strcat(buf, " ");
			//buf[n + 1] = '\0';
			strcat(buf, ctime(&t));	
			if(send(newfd, buf, 70, 0) == -1)
				perror("send");
			close(newfd);
			exit(0);
		}
		close(newfd);
	}

	return 0;
}
