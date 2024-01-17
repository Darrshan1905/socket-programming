#include<stdio.h>
#include<sys/socket.h>  
#include<sys/types.h>  
#include<string.h>  
#include<stdlib.h>
#include<errno.h>
#include<stdlib.h>  
#include<unistd.h>  
#include<netdb.h>
#include<sys/wait.h>
#include<netinet/in.h>
#include<arpa/inet.h> 
#include<sys/poll.h>

#define BACKLOG 10 	//pending connections the queue can hold
#define MAX 200	   	//maximum buffer size

//get the socket address and return in network representation
void *get_addr(struct sockaddr *sa ) {
	if(sa -> sa_family == AF_INET)
		return &(((struct sockaddr_in*) sa) -> sin_addr);	//for ipv4
	return &(((struct sockaddr_in6*) sa) -> sin6_addr);		//for ipv6
}

//create the socket descriptor and bind it to server address, return the socket descriptor
int create_proxysocket(char *proxy_port) {
	int sockfd;
	struct addrinfo hints, *proxyinfo, *p;
	int rv;
	int yes = 1;

	memset(&hints, 0, sizeof hints);	//make the struct initially empty
        hints.ai_family = AF_UNSPEC;		//can be either ipv4 or ipv6
        hints.ai_socktype = SOCK_STREAM;	//TCP stream socket
        hints.ai_flags = AI_PASSIVE;		//fill in the host machine's IP

	//gives a pointer to a linked list, proxyinfo of results
	if((rv = getaddrinfo(NULL, proxy_port, &hints, &proxyinfo))) {
                fprintf(stderr, "getaddrinfo: %s", gai_strerror(rv));
                return 1;
        }
	
	//loop through each result in the proxyinfo and bind to the first you can
        for(p = proxyinfo; p != NULL; p = p -> ai_next) {
		//make a socket
                if((sockfd = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1) {
                        perror("proxy-server: socket");
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
                        perror("proxy-server: bind");
                        continue;
                }
                printf("Socket created\n");
                break;
        }

	//free the address space for proxyinfo, done with this structure
        freeaddrinfo(proxyinfo);

        if(p == NULL) {
                fprintf(stderr, "proxy-server: failed to bind\n");
                exit(1);
        }
	
	printf("Socket binded\n");
	
	return sockfd;
}

//server listens for the incoming connections on sockfd
void proxy_listen(int sockfd) {
	if(listen(sockfd, BACKLOG) == -1) {
                perror("listen");
                exit(1);
        }
}

//accept an incoming connection
void proxy_accept(int sockfd, struct pollfd **pfds, int *fd_size, int *fd_count) {
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
        printf("proxy-server: got connection from %s\n", s);
}

int create_serversocket(char *host, char * port) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);		//make the struct initially empty
        hints.ai_family = AF_UNSPEC;			//can be either ipv4 or ipv6
        hints.ai_socktype = SOCK_STREAM;		//TCP stream socket

	//gives a pointer to a linked list, servinfo of results
        if((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                return 1;
        }
	
	//loop through each result in the servinfo and bind to the first you can
        for(p = servinfo; p != NULL; p = p -> ai_next) {
		//make a socket
                if((sockfd = socket(p -> ai_family, p -> ai_socktype, p -> ai_protocol)) == -1) {
                        perror("proxy:  socket");
                        continue;
                }

		//connect the socket to the port and ip address of the server
                if(connect(sockfd, p -> ai_addr, p -> ai_addrlen) == -1) {
                        close(sockfd);
                        perror("proxy: connect");
                        continue;
                }

                break;
        }

	//convert the ip address from network to presentation format
        inet_ntop(p -> ai_family, get_addr((struct sockaddr *) p -> ai_addr), s, sizeof s);
        printf("proxy server connecting to %s\n", s);

	//free the address space for servinfo, done with this structure
        freeaddrinfo(servinfo);

	return sockfd;
}

int recvfromclient(int clientfd, int serverfd) {
	int n;
	char buf[MAX];
	memset(buf,'\0', sizeof(buf));

	if((n = recv(clientfd, buf, MAX - 1, 0)) == -1) {
		perror("rcv\n");
		exit(1);
	}		

	buf[n] = '\0';
	printf("Proxy-server: Message from the client to server: %s\n",buf);

	if((n = send(serverfd, buf, MAX-1, 0)) == -1) {
		perror("send\n");
		exit(1);
	}

	printf("Proxy-server: Message sent to the server.  #%d\n", n);
	
	return 1;
}

void recvfromserver(int serverfd, int clientfd) {
        int n;
        char buf[MAX];
        memset(buf,'\0', sizeof(buf) - 1);

        if((n = recv(serverfd, buf, MAX - 1, 0)) == -1) {
                perror("rcv\n");
                exit(1);
        }

        buf[n] = '\0';
        printf("Proxy-server: Message from the server to client: %s\n",buf);

        if((n = send(clientfd, buf, MAX-1, 0)) == -1) {
                perror("send\n");
                exit(1);
        }

        printf("Proxy-server: Message sent to the client.\n\n\n");
}

void connectServer(struct pollfd *pfds, char *ip, char *port) {
	int serverfd;
	int retflag = 0;
	
	serverfd = create_serversocket(ip, port);
	
	while(1) {
		retflag = recvfromclient(pfds -> fd, serverfd);
		if(retflag)
			recvfromserver(serverfd, pfds -> fd);
	}
}

int main(int argc, char *argv[]) {
	char port[10], ip[15], proxy_port[10];
	int proxyfd, newfd;
	nfds_t nfds = 0;
	int fd_count = 0;
	int fd_size = 5;
	struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

	if(argc != 4) {
		printf("usage: ./proxy-server.out server-ip server-port proxy-port.\n");
		exit(1);
	}

	strcpy(ip, argv[1]);
	strcpy(port, argv[2]);
	strcpy(proxy_port, argv[3]);

	printf("Server IP: %s\n", ip);
	printf("Server port: %s\n", port);
	printf("Proxy port: %s\n", proxy_port);

	proxyfd = create_proxysocket(proxy_port);		//create socket and bind it to the server's address and port

	proxy_listen(proxyfd);			//make the proxy-server listen for incoming connections

	pfds[0].fd = proxyfd;
	pfds[0].events = POLLIN;

	fd_count = 1;

	printf("proxy-server: waiting for connections...\n");

	/*while(1) {
		newfd = proxy_accept(proxyfd);

	        if(newfd == -1)
			continue;

		if(!fork()) {			//for child processes
			close(proxyfd);		//listener not needed for child processes

			connectServer(newfd, ip, port);
		}
		close(newfd);			//parent doesn't need this
	}*/

	while(1) {
		nfds = fd_count;
		int poll_count = poll(pfds, nfds, -1);

		if(poll_count == -1) {
			perror("poll: ");
			exit(1);
		}

		for(int i = 0; i < nfds; i++) {
			if((pfds[i].revents & POLLIN) == POLLIN) {
				if(pfds[i].fd == proxyfd) {
					proxy_accept(proxyfd, &pfds, &fd_size, &fd_count);
				}
				else {
					connectServer(pfds + i, ip, port);
				}
			}
		}
	}

	close(proxyfd);
	return 0;
}
