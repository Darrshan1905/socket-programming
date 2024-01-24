#include<stdio.h>
#include<string.h>
#include <stdlib.h>
#include <ctype.h>
#include<unistd.h>
#include<netdb.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <signal.h>
#include <time.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define PROXY_IP "127.0.0.1"
#define PROXY_PORT "8000"
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT "8010"
#define MAX 1024

//get the sock address and return in network representation
void *get_addr(struct sockaddr *sa) { 
	if(sa -> sa_family == AF_INET)				
		return &(((struct sockaddr_in*)sa) -> sin_addr);	//for ipv4
	return &(((struct sockaddr_in6*)sa) -> sin6_addr);		//for ipv6
}

SSL_CTX *create_SSL_context() {
	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_ssl_algorithms();

	SSL_CTX *context;
	const SSL_METHOD *method = TLS_client_method();

	context = SSL_CTX_new(method);

	if(context == NULL) {
		ERR_print_errors_fp(stderr);
		exit(1);
	}	

	return context;
}

int create_clientsocket() {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);		//make the struct initially empty
        hints.ai_family = AF_UNSPEC;			//can be either ipv4 or ipv6
        hints.ai_socktype = SOCK_STREAM;		//TCP stream socket

	//gives a pointer to a linked list, servinfo of results
        if((rv = getaddrinfo(PROXY_IP, PROXY_PORT, &hints, &servinfo)) != 0) {
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

	if (p == NULL) {
        	fprintf(stderr, "client: failed to connect to the proxy server\n");
        	exit(EXIT_FAILURE);
    	}

	//convert the ip address from network to presentation format
        inet_ntop(p -> ai_family, get_addr((struct sockaddr *) p -> ai_addr), s, sizeof s);
        printf("client connecting to %s\n", s);

	//free the address space for servinfo, done with this structure
        freeaddrinfo(servinfo);

	return sockfd;
}

void sendconnreq(SSL *ssl) {
	char req[1024];

	sprintf(req, "CONNECT %s:%s HTTP/1.1\r\nHost:%s\r\n\r\n", SERVER_IP, SERVER_PORT, SERVER_IP);

	if((SSL_write(ssl, req, strlen(req))) == -1) {
		perror("send");
		exit(1);
	}

	printf("Connect request sent to the destination server\n");
}

void sendgetreq(SSL *ssl) {
        char req[1024];

        sprintf(req, "GET / HTTP/1.1\r\nHost: %s\r\n\r\n", SERVER_IP);

        if((SSL_write(ssl, req, strlen(req))) == -1) {
                perror("send");
                exit(1);
        }

        printf("Get request sent to the destination server\n");
}

void sendpostreq(SSL *ssl) {
        char req[1024];
	char body[100] = "Hello! This is client";

        sprintf(req, "POST / HTTP/1.1\r\nHost: %s\r\n\r\n%s", SERVER_IP, body);

        if((SSL_write(ssl, req, strlen(req))) == -1) {
                perror("send");
                exit(1);
        }

        printf("Post request sent to the destination server\n");
}

void recvmessage(SSL *ssl) {
	int n;			//number of bytes actually received
	char buf[MAX];
	memset(buf,'\0', sizeof(buf) - 1);

	FILE* outputfile = fopen("out.html", "wb");
	if(outputfile == NULL) {
		fprintf(stderr,"Can't open file output.html\n");
		return;
	}

	if((n = SSL_read(ssl, buf, sizeof(buf) - 1)) == -1) {
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

int main() {
	int sockfd;
	SSL *ssl;
	SSL_CTX *context;
	
	//create and initialize an ssl context
	context = create_SSL_context();

	//instantiate an ssl object
	ssl = SSL_new(context);

	//create client socket
	sockfd = create_clientsocket();

	printf("%d", sockfd);
	//link the ssl object to the socket descriptor
	SSL_set_fd(ssl, sockfd);	

	//printf("%d", sockfd);
	//ssl handshake
	int rv;
	if((rv = SSL_connect(ssl)) == -1) {
		printf(" %d", rv);
		ERR_print_errors_fp(stderr);
		exit(1);
	}

	printf("%d", sockfd);

	while(1) {
		int req_method;

		printf("Enter the request method:\n0.CONNECT\n1.GET\n2.POST\n3.QUIT\n");
		scanf("%d", &req_method);

		if(req_method == 0) {
			sendconnreq(ssl);
			recvmessage(ssl);
		}
		else if(req_method == 1) {
			sendgetreq(ssl);
			recvmessage(ssl);
		}
		else if(req_method == 2) {
			sendpostreq(ssl);
			recvmessage(ssl);
		}
		else
			break;
	}

	SSL_free(ssl);
    	close(sockfd);
    	SSL_CTX_free(context);

	return 0;
}
