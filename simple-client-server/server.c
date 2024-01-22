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
#include<time.h>
#include<sys/poll.h>

#define PORT "3490"	//the port the users wil be connecting to
#define BACKLOG 10 	//pending connections the queue can hold
#define MAX 100000	   	//maximum buffer size

//get the socket address and return in network representation
void *get_addr(struct sockaddr *sa ) {
	if(sa -> sa_family == AF_INET)
		return &(((struct sockaddr_in*) sa) -> sin_addr);	//for ipv4
	return &(((struct sockaddr_in6*) sa) -> sin6_addr);		//for ipv6
}

//create the socket descriptor and bind it to server address, return the socket descriptor
int create_serversocket() {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int yes = 1;

	memset(&hints, 0, sizeof hints);	//make the struct initially empty
        hints.ai_family = AF_UNSPEC;		//can be either ipv4 or ipv6
        hints.ai_socktype = SOCK_STREAM;	//TCP stream socket
        hints.ai_flags = AI_PASSIVE;		//fill in the host machine's IP

	//gives a pointer to a linked list, servinfo of results
        if((rv = getaddrinfo(NULL, PORT, &hints, &servinfo))) {
                fprintf(stderr, "getaddrinfo: %s", gai_strerror(rv));
                return 1;
        }
	
	//loop through each result in the servinfo and bind to the first you can
        for(p = servinfo; p != NULL; p = p -> ai_next) {
		//make a socket
                if((sockfd = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1) {
                        perror("server: socket");
                        continue;
                }

		//handle address already in use error message
                if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
                        perror("sockopt");
                        exit(1);
                }
		
		//bind the socket to the port passed in to getaddrinfo 
                if(bind(sockfd, p -> ai_addr, p -> ai_addrlen) == -1) {
                        close(sockfd);
                        perror("server: bind");
                        continue;
                }
                printf("Socket created\n");
                break;
        }

	//free the address space for servinfo, done with this structure
        freeaddrinfo(servinfo);

        if(p == NULL) {
                fprintf(stderr, "server: failed to bind\n");
                exit(1);
        }
	
	printf("Socket binded\n");
	
	return sockfd;
}

//server listens for the incoming connections on sockfd
void server_listen(int sockfd) {
	if(listen(sockfd, BACKLOG) == -1) {
                perror("listen");
                exit(1);
        }
}

//accept an incoming connection
void server_accept(int sockfd, struct pollfd **pfds, int *fd_size, int *fd_count) {
	int newfd;				//new socket descriptor after accepting the connection request
	struct sockaddr_storage their_addr;	//connector's address information
        socklen_t sin_size;			//size of struct sockaddr
	char s[INET6_ADDRSTRLEN];		//used to hold the ip address in presentation format
	
	sin_size = sizeof their_addr;

        newfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);	//accept the incoming connection on sockfd and store the											//connecter's address
        
	if(newfd == -1) {
        	perror("accept");
                exit(1);
        }

	if(*fd_count == *fd_size) {
		*pfds = realloc(*pfds, (*fd_size + 5) * sizeof(struct pollfd));

		if(*pfds == NULL) {
			perror("realloc: ");
			exit(1);
		}

		*fd_size += 5;
	}

	(*pfds)[*fd_count].fd = newfd;
	(*pfds)[*fd_count].events = POLLIN;

	(*fd_count)++;
	
	//convert the ip address from network to presentation format
        inet_ntop(their_addr.ss_family, get_addr((struct sockaddr *) &their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);
}

//send or receive messages on new socket descriptor
void sendrecv_message(struct pollfd *pfds) {
	int n;			//number of bytes actually read
	char buf[MAX];		//used to hold the message sent by the client

	//recieve message from clients on newfd
	if((n = recv(pfds -> fd, buf, MAX - 1, 0)) == -1) {
        	perror("recv");
		exit(1);
	}
		
	FILE *fp = fopen("file2.txt", "w");
	if(fwrite(buf, 1, strlen(buf), fp) == -1) {
		perror("write: ");
		exit(1);
	}
	buf[n-1] = '\0';
        printf("Server: received %s from client\n", buf);
	

        time_t t;
        time(&t);
        strcat(buf, " ");
        //buf[n + 1] = '\0';
        strcat(buf, ctime(&t));	//concatenate the client's message and the current timestamp

	//send message to clients on newfd
	if(send(pfds -> fd, buf, MAX - 1, 0) == -1) {
        	perror("send");
		exit(1);
	}

	//close the new socket descriptor once the communication is over
        close(pfds -> fd);
	pfds -> fd *= -1;
	fclose(fp);
}

int main() {
	int sockfd, newfd;
	struct sockaddr_storage their_addr; // Client address
	socklen_t addrlen;
	nfds_t nfds = 0;
	int fd_count = 0;
        int fd_size = 5;
        struct pollfd *pfds = malloc(sizeof *pfds * fd_size);	

	sockfd = create_serversocket();		//create socket and bind it to the server's address and port

	server_listen(sockfd);			//make the server listen for incoming connections

	pfds[0].fd = sockfd;
	pfds[0].events = POLLIN;

	fd_count = 1;

	//accept() loop
	/*while(1) {
		newfd = server_accept(sockfd);
		
	        if(newfd == -1)
			continue;

		if(!fork()) {			//for child processes
			close(sockfd);		//listener not needed for child processes

			sendrecv_message(newfd);
		}
		close(newfd);			//parent doesn't need this
	}*/

	printf("Server: waiting for connections...\n");

	while(1) {
		nfds = fd_count;

		int poll_count = poll(pfds, nfds, -1);

		if(poll_count == -1) {
			perror("poll");
			exit(1);
		}

		for(int i = 0; i < nfds; i++) {
			if(pfds[i].fd <= 0)
				continue;

			if((pfds[i].revents & POLLIN) == POLLIN) {
				if(pfds[i].fd == sockfd) {
					server_accept(sockfd, &pfds, &fd_size, &fd_count);
				}
				else {
					sendrecv_message(pfds + i);
				}
			}
		}
	}
	
	close(sockfd);
	return 0;
}
