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

#define MAX 200		//maximum buffer size

//get the sock address and return in network representation
void *get_addr(struct sockaddr *sa) { 
	if(sa -> sa_family == AF_INET)				
		return &(((struct sockaddr_in*)sa) -> sin_addr);	//for ipv4
	return &(((struct sockaddr_in6*)sa) -> sin6_addr);		//for ipv6
}

//creating a socket for client and returning the socket descriptor
int create_clientsocket(char *host, char* port) {
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
/*void sendmessage(int sockfd, char msg[]) {
	if(send(sockfd, msg, MAX-1, 0) == -1) {
                perror("send");
                exit(1);
        }
        printf("Sent message to the server\n");
}*/

//send get request to the server on sockfd
void sendgetrequest(int sockfd, char *host) {
	char req[MAX];

	sprintf(req, "GET / HTTP/1.1\r\nHost: %s\r\n\r\n", host);
	if(send(sockfd, req, MAX-1, 0) == -1) {
                perror("send");
                exit(1);
        }
        printf("Get Request sent to the server\n");
}

//send post request to the server on sockfd
void sendpostrequest(int sockfd, char *host) {
	char req[MAX];
	char body[25] = "Hello this is client";
	
	sprintf(req, "POST / HTTP/1.1\r\nHost: %s\r\n\r\n%s", host, body);

        if(send(sockfd, req,MAX-1, 0) == -1) {
                perror("send");
                exit(1);
        }
        printf("Post Request sent to the server\n");
}

//receive the message from the server on sockfd
void recvmessage(int sockfd) {
	/*int n;			//number of bytes actually received
	char buf[MAX];
	if((n = recv(sockfd, buf, MAX - 1, 0)) == -1) {
                perror("recv");
                exit(1);
        }
	
        buf[n] = '\0';

        printf("client: received message from server - '%s'\n", buf);*/

	int n;			//number of bytes actually received
	char buf[MAX];
	memset(buf,'\0', sizeof(buf) - 1);

	FILE* outputfile = fopen("out.html", "wb");
	if(outputfile == NULL) {
		fprintf(stderr,"Can't open file output.html\n");
		return;
	}

	if((n = recv(sockfd, buf, MAX - 1, 0)) == -1) {
                perror("recv\n");
		exit(1);
        }

        buf[n] = '\0';

	char *content = strstr(buf, "\r\n\r\n");
	printf("%s\n", content + 4);
	if(content != NULL)
		fwrite(content + 4, 1, strlen(content + 4), outputfile);

        printf("client: received response from server: \n'%s'\n", buf);

	fclose(outputfile);
}

int main(int argc, char *argv[]) {
	int sockfd;

	if(argc != 3) {
		fprintf(stderr,"usage: client hostname port\n");
		exit(1);
	}

	sockfd = create_clientsocket(argv[1], argv[2]);		//create a socket and bind it to the ip and port of the server

	while(1) {
		//int i = 0;
		//scanf("%d",&i);
		int req_method;
		printf("Enter request method\n0.GET\n1.POST\n2.QUIT\n");
		scanf("%d", &req_method);
		if(!req_method){
			sendgetrequest(sockfd, argv[1]);                       //send message to the server	
			recvmessage(sockfd);                       		//recv msg from the server	
		}
		else if(req_method == 1) {
			sendpostrequest(sockfd, argv[1]);                       //send message to the server     
                        recvmessage(sockfd);                                    //recv msg from the server 
		}
		else
			break;
	}

	close(sockfd);					//close the socket

	return 0;
}
