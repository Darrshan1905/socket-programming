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

#define BACKLOG 10 	//pending connections the queue can hold
#define MAX 1024	   	//maximum buffer size

void sig_handler(int s) {
	int saved_errno = errno;

	//errno might be overwritten by waitpid, so we save and restore it
	while(waitpid(-1, NULL, WNOHANG) > 0);
	errno = saved_errno;
}

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

//kill all the zombie processes
void signal_handler() {
	struct sigaction sa;
	
	sa.sa_handler = sig_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;

        if(sigaction(SIGCHLD, &sa, NULL) == -1) {
                perror("sigaction");
                exit(1);
        }

        printf("proxy-server: waiting for connections...\n");
}

//accept an incoming connection
int proxy_accept(int sockfd) {
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
	
	//convert the ip address from network to presentation format
        inet_ntop(their_addr.ss_family, get_addr((struct sockaddr *) &their_addr), s, sizeof s);
        printf("proxy-server: got connection from %s\n", s);

	return newfd;
}

int create_serversocket(char *host, char * port) {
	printf("0");
	
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);		//make the struct initially empty
        hints.ai_family = AF_UNSPEC;			//can be either ipv4 or ipv6
        hints.ai_socktype = SOCK_STREAM;		//TCP stream socket
	
	printf("1");

	//gives a pointer to a linked list, servinfo of results
        if((rv = getaddrinfo(host, port, &hints, &servinfo)) != 0) {
                fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
                return 1;
        }

	printf("%d", rv);
	
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

/*int recvfromclient(int clientfd, int serverfd) {
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
}*/

void recvfromserver(int serverfd, int clientfd) {
        int n;
        char buf[MAX];
        memset(buf,'\0', sizeof(buf) - 1);

        while((n = recv(serverfd, buf, MAX - 1, 0)) > 0) {
		printf("\n%s\n", buf);                
		if((n = send(clientfd, buf, MAX-1, 0)) == -1) {
                	perror("send\n");
                	exit(1);
        	}
		
        }

        printf("Proxy-server: Message sent to the client.\n\n\n");
}

void connectServer(int clientfd) {
	int serverfd;
	int n, c;

	char buf[MAX], data[MAX];
        char method[256], host[256];

	
	if((c = read(clientfd, buf, sizeof(buf))) == -1) {
                perror("proxy: read");
                exit(1);
        }
        
	printf("%s\n",buf);

	strcpy(data, buf);
        sscanf(buf, "%s %s", method, host);

        //printf("method: %s host: %s", method, host);
        char* host_start = strstr(buf,"Host: ") + 6;
        char* host_end = strstr(host_start,"\r\n");
        *host_end = '\0';
 	printf("1");
        char* port_start = strstr(host_start,":");
        char* port;
	char http_port[100] = "80";

	if(port_start != NULL) {
                *port_start = '\0';
		port = port_start + 1;
	}
	else {
		port = http_port;
	}
	
	printf("%s %s\n",host_start,port);
	serverfd = create_serversocket(host_start, port);
        printf("%d", serverfd);

	if((n = send(serverfd, data, MAX-1, 0)) == -1) {
                perror("send\n");
                exit(1);
        }

        printf("Proxy-server: Message sent to the server.  #%d\n", n);
	

	recvfromserver(serverfd, clientfd);
}

int main(int argc, char *argv[]) {
	char proxy_port[10];
	int proxyfd, newfd;

	if(argc != 2) {
		printf("usage: ./proxy-server.out proxy-port.\n");
		exit(1);
	}

	strcpy(proxy_port, argv[1]);

	printf("Proxy port: %s\n", proxy_port);

	proxyfd = create_proxysocket(proxy_port);		//create socket and bind it to the server's address and port

	proxy_listen(proxyfd);			//make the server listen for incoming connections

	signal_handler();			//kill dead processes

	while(1) {
		newfd = proxy_accept(proxyfd);

	        if(newfd == -1)
			continue;

		if(!fork()) {			//for child processes
			close(proxyfd);		//listener not needed for child processes

			connectServer(newfd);

			close(newfd);
			exit(0);
		}
		close(newfd);			//parent doesn't need this
	}
}
