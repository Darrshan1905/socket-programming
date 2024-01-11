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

#define PORT "3490"	//the port the users wil be connecting to
#define BACKLOG 10 	//pending connections the queue can hold
#define MAX 100	   	//maximum buffer size

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

        printf("server: waiting for connections...\n");
}

//accept an incoming connection
int server_accept(int sockfd) {
	int newfd;				//new socket descriptor after accepting the connection request
	struct sockaddr_storage their_addr;	//connector's address information
        socklen_t sin_size;			//size of struct sockaddr
	char s[INET6_ADDRSTRLEN];		//used to hold the ip address in presentation format

	sin_size = sizeof their_addr;

        newfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);	//accept the incoming connection on sockfd and store the											//connecter's address
        
	if(newfd == -1) {
        	perror("accept");
                return newfd;
        }
	
	//convert the ip address from network to presentation format
        inet_ntop(their_addr.ss_family, get_addr((struct sockaddr *) &their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);

	return newfd;
}

//send or receive messages on new socket descriptor
void sendrecv_message(int newfd) {
	int n;			//number of bytes actually read
	char buf[MAX];		//used to hold the message sent by the client

	//recieve message from clients on newfd
	if((n = recv(newfd, buf, MAX - 1, 0)) == -1)
        	perror("recv");
        printf("Server: received '%s' from client", buf);
        time_t t;
        time(&t);
        strcat(buf, " ");
        //buf[n + 1] = '\0';
        strcat(buf, ctime(&t));	//concatenate the client's message and the current timestamp

	//send message to clients on newfd
	if(send(newfd, buf, 70, 0) == -1)
        	perror("send");

	//close the new socket descriptor once the communication is over
        close(newfd);
        exit(0);
}

int main() {
	int sockfd, newfd;

	sockfd = create_serversocket();		//create socket and bind it to the server's address and port

	server_listen(sockfd);			//make the server listen for incoming connections

	signal_handler();			//kill dead processes

	//accept() loop
	while(1) {
		newfd = server_accept(sockfd);
		
	        if(newfd == -1)
			continue;

		if(!fork()) {			//for child processes
			close(sockfd);		//listener not needed for child processes

			sendrecv_message(newfd);
		}
		close(newfd);			//parent doesn't need this
	}

	return 0;
}
