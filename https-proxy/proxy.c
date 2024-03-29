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
#include<signal.h>
#include<time.h>
#include<poll.h>

#define BACKLOG 10 	//pending connections the queue can hold
#define MAX 1024	   	//maximum buffer size
#define PROXY_PORT "8000"

void sig_handler(int s){
	int saved_errno = errno;

	//errno might be overwritten by waitpid, so we save and restore it
	while(waitpid(-1,NULL,WNOHANG) > 0);
	errno = saved_errno;
}

//get the sock address and return in network representation
void *get_addr(struct sockaddr *sa) {
        if(sa -> sa_family == AF_INET)
                return &(((struct sockaddr_in*)sa) -> sin_addr);        //for ipv4
        return &(((struct sockaddr_in6*)sa) -> sin6_addr);              //for ipv6
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

int create_proxysocket() {
	int sockfd;
	struct addrinfo hints, *proxyinfo, *p;
	int rv;
	int yes = 1;

	memset(&hints, 0, sizeof hints);	//make the struct initially empty
        hints.ai_family = AF_UNSPEC;		//can be either ipv4 or ipv6
        hints.ai_socktype = SOCK_STREAM;	//TCP stream socket
        hints.ai_flags = AI_PASSIVE;		//fill in the host machine's IP

	//gives a pointer to a linked list, proxyinfo of results
	if((rv = getaddrinfo(NULL, PROXY_PORT, &hints, &proxyinfo))) {
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
	
	printf("Socket binded , sockfd:%d\n", sockfd);
	
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
int proxy_accept(int sockfd) {
	int newfd;				//new socket descriptor after accepting the connection request
	struct sockaddr_storage their_addr;	//connector's address information
        socklen_t sin_size;			//size of struct sockaddr
	char s[INET6_ADDRSTRLEN];		//used to hold the ip address in presentation format
	sin_size = sizeof(their_addr);

        newfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);	//accept the incoming connection on sockfd and store the											//connecter's address

	if(newfd == -1) {
        	perror("accept");
                return -1;
        }

	//convert the ip address from network to presentation format
        inet_ntop(their_addr.ss_family, get_addr((struct sockaddr *) &their_addr), s, sizeof s);
        printf("proxy-server: got connection from %s\n", s);
	printf("newfd: %d\n",newfd);
	return newfd;
}

int create_serversocket(char *host, char * port) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof(hints));		//make the struct initially empty
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

	if(p == NULL){
		fprintf(stderr, "client: failed to connect\n");
		return -1;	
	}

	//convert the ip address from network to presentation format
        inet_ntop(p -> ai_family, get_addr((struct sockaddr *) p -> ai_addr), s, sizeof s);
        printf("proxy server connecting to %s\n", s);

	//free the address space for servinfo, done with this structure
        freeaddrinfo(servinfo);

	return sockfd;
}

void handle_message(int clientfd, int serverfd) {
	char buff[MAX];
	ssize_t n;
	struct pollfd pfds[2];

	pfds[0].fd = clientfd;
	pfds[0].events = POLLIN;
	pfds[0].revents = 0;

	pfds[1].fd = serverfd;
	pfds[1].events = POLLIN;
	pfds[1].revents = 0;


	while(1) {
		if(poll(pfds, 2, -1) == -1) {
			perror("poll: ");
			exit(1);
		}

		for(int fd = 0; fd < 2; fd++) {
			if((pfds[fd].revents & POLLIN) == POLLIN && fd == 0) {
				n = read(clientfd, buff, sizeof(buff));

				if(n <= 0) 
					return;
				buff[n] = '\0';
				n = write(serverfd, buff, n);
			}	
			if((pfds[fd].revents & POLLIN) == POLLIN && fd == 1) {
				n = read(serverfd, buff, sizeof(buff));

				if(n <= 0)
                                        return;
                                buff[n] = '\0';
                                n = write(clientfd, buff, n);
			}
		}
	}

}

void handle_message_http(int clientfd, int serverfd, char data[]) {
	ssize_t n;
	n = write(serverfd, data, MAX);

	while((n = recv(serverfd, data, MAX, 0)) > 0) {
		send(clientfd, data, n, 0);
	}
}

void connectServer(int newfd) {
	int serverfd, n, c;
	char method[256], host[256];
	char buf[MAX], data[MAX];

	if((c = read(newfd, buf, sizeof(buf))) <= 0) {
		close(newfd);
		perror("read");
		exit(1);
	}

	buf[c] = '\0';

	strcpy(data,buf);

	//printf("%s", buf);

	sscanf(buf, "%s %s", method, host);

	if(strcmp(method, "CONNECT") == 0) {
		char *port_start = strchr(host, ':');
		char *port;
		char https_port[10] = "443";

		if(port_start != NULL) {
			*port_start = '\0';
			port = port_start + 1;
		}
		else {
			port = https_port;
		}

		if((serverfd = create_serversocket(host, port)) == -1) {
			perror("socket: ");
			close(newfd);
			exit(1);
		}

		printf("Host: %s Port: %s\n", host, port);
		char *response = "HTTP/1.1 200 Connection established\r\n\r\n";
		write(newfd, response, strlen(response));

		handle_message(newfd, serverfd);
	}
	else {
		char *host_start = strstr(buf, "Host: ") + 6;
		char *host_end = strstr(host_start, "\r\n");
		*host_end = '\0';

		char *port;
		char http_port[10] = "80";
		char *port_start = strstr(host_start, ":");

		if(port_start != NULL) {
			*port_start = '\0';
			port = port_start + 1;
		}
		else {
			port = http_port;
		}

		printf("URL: %s\nHost: %s\nPort: %s\n", host, host_start, port);
		
		int serverfd;
		if((serverfd = create_serversocket(host, port) == -1)) {
                        perror("socket: ");
                        close(newfd);
                        exit(1);
                }

		handle_message_http(newfd, serverfd, data);
		close(serverfd);
	}
}

int  main() {
	int proxyfd, newfd;

	proxyfd = create_proxysocket();		//create socket and bind it to the server's address and port

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
	close(proxyfd);
	return 0;
}
