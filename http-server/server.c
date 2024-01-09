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
#define MAX 200	   	//maximum buffer size

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

void getfileURL(char *route, char *fileURL) {
  	char *question = strrchr(route, '?');
  	if (question)
    		*question = '\0';

  	if (route[strlen(route) - 1] == '/') {
    		strcat(route, "index.html");
  	}
	
	strcpy(fileURL, "");
 	strcat(fileURL, route);

  	const char *dot = strrchr(fileURL, '.');
  	if (!dot || dot == fileURL) {
    		strcat(fileURL, ".html");
  	}
}

void getmimetype(char *file, char *mime)
{
  const char *dot = strrchr(file, '.');

  if (dot == NULL)
    strcpy(mime, "text/html");

  else if (strcmp(dot, ".html") == 0)
    strcpy(mime, "text/html");

  else if (strcmp(dot, ".css") == 0)
    strcpy(mime, "text/css");

  else if (strcmp(dot, ".js") == 0)
    strcpy(mime, "application/js");

  else if (strcmp(dot, ".jpg") == 0)
    strcpy(mime, "image/jpeg");

  else if (strcmp(dot, ".png") == 0)
    strcpy(mime, "image/png");

  else if (strcmp(dot, ".gif") == 0)
    strcpy(mime, "image/gif");

  else
    strcpy(mime, "text/html");
}

//handle get requests
void handlegetreq(int newfd, char req[]) {
	char resheader[MAX];
	char method[10], route[100];
	char fileURL[100];
	char mimetype[32];
	char timebuf[100];
	int headersize;

	sscanf(req, "%s %s", method, route);
	
	printf("%s %s\n",method, route);

	getfileURL(route, fileURL);

	printf("%s\n", fileURL);

	FILE *file = fopen(fileURL, "r");

	if(!file) {
		char response[MAX];
		sprintf(response, "HTTP/1.1 404 Not Found\r\n");
		if((send(newfd, response, strlen(response), 0)) == -1)
                	printf("send: error\n");
		return;
	}

	getmimetype(fileURL, mimetype);
	printf("%s\n", mimetype);

	time_t t;
	time(&t);
	strcpy(timebuf, ctime(&t));

	sprintf(resheader, "HTTP/1.1 200 OK\r\nDate: %s\r\nContent-Type: %s\r\n\n", timebuf, mimetype);
	headersize = strlen(resheader);

	fseek(file, 0, SEEK_END);
	long fsize = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *resbuffer = (char *)malloc(headersize + fsize);
	strcpy(resbuffer, resheader);
	char *filebuffer = resbuffer + headersize;
	fread(filebuffer, fsize, 1, file);

	if((send(newfd, resbuffer, headersize + fsize, 0)) == -1)
		printf("send: error\n");
	
	free(resbuffer);
	fclose(file);
}

//handle post requests
void handlepostreq(int newfd, char *body) {
	char response[MAX];
	printf("%s", body);
	sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello, this is a POST response. Received data: %s", body);

	if((send(newfd, response, strlen(response), 0)) == -1)
                printf("send: error\n");
}

//handle requests on new socket descriptor
void handlerequests(int newfd) {
	int n;			//number of bytes actually read
	char buf[MAX];		//used to hold the message sent by the client

	//recieve requests from clients on newfd
	if((n = recv(newfd, buf, MAX - 1, 0)) == -1)
        	perror("recv");

	//check whether a get or post request
        if(strstr(buf, "GET") != NULL)
		handlegetreq(newfd, buf);
	else if(strstr(buf, "POST") != NULL) {
		char *body = strstr(buf, "\r\n\r\n");
		if(body != NULL) {
			body += 4;
			handlepostreq(newfd, body);
		}
	}

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

			handlerequests(newfd);
		}
		close(newfd);			//parent doesn't need this
	}

	return 0;
}
