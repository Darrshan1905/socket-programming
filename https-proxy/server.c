#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<ctype.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<arpa/inet.h>
#include<sys/wait.h>
#include<signal.h>
#include<time.h>
#include<unistd.h>
#include<sys/poll.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define PORT "8010"	//the port the users wil be connecting to
#define BACKLOG 10 	//pending connections the queue can hold
#define MAX 1024	   	//maximum buffer size

//get the socket address and return in network representation
void *get_addr(struct sockaddr *sa ) {
	if(sa -> sa_family == AF_INET)
		return &(((struct sockaddr_in*) sa) -> sin_addr);	//for ipv4
	return &(((struct sockaddr_in6*) sa) -> sin6_addr);		//for ipv6
}

SSL_CTX *create_SSL_context() {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();

        const SSL_METHOD *method = TLS_server_method();

        SSL_CTX *ctx = SSL_CTX_new(method);

        if(ctx == NULL) {
                ERR_print_errors_fp(stderr);
                exit(1);
        }

        if(SSL_CTX_use_certificate_file(ctx, "server.crt", SSL_FILETYPE_PEM) <= 0) {
                ERR_print_errors_fp(stderr);
                exit(EXIT_FAILURE);
        }

        if (SSL_CTX_use_PrivateKey_file(ctx, "server.key", SSL_FILETYPE_PEM) <= 0) {
                ERR_print_errors_fp(stderr);
                exit(EXIT_FAILURE);
        }

	return ctx;
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
void server_accept(int sockfd, struct pollfd **pfds, int *fd_size, int *fd_count, SSL*** sslfds, SSL_CTX* context) {
	//printf("fu\n");
	int newfd;				//new socket descriptor after accepting the connection request
	struct sockaddr_storage their_addr;	//connector's address information
        socklen_t sin_size;			//size of struct sockaddr
	char s[INET6_ADDRSTRLEN];		//used to hold the ip address in presentation format

	sin_size = sizeof their_addr;

        newfd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);	//accept the incoming connection on sockfd and store the											//connecter's address
        //printf("%d \n", newfd);
	if(newfd == -1) {
        	perror("accept");
                exit(1);
        }

	if(*fd_count == *fd_size) {
                *pfds = realloc(*pfds, (*fd_size + 5) * sizeof(struct pollfd));
		*sslfds = realloc(*sslfds, (*fd_size + 5) * sizeof(SSL*));

                if(*pfds == NULL || *sslfds == NULL) {
                        perror("realloc: ");
                        exit(1);
                }

                *fd_size += 5;
        }

	printf("%d\n", *fd_size);

        (*pfds)[*fd_count].fd = newfd;
        (*pfds)[*fd_count].events = POLLIN;
	(*pfds)[*fd_count].revents = 0;
	
	printf("%d\n", *fd_size);

	SSL* ssl = SSL_new(context);
	printf("%d\n", *fd_size);
	SSL_set_fd(ssl, newfd);
	if(SSL_accept(ssl) <= 0) {
		ERR_print_errors_fp(stderr);
        	close(newfd);
        	exit(1);
	}
	printf("4");
	(*sslfds)[*fd_count] = ssl;

        (*fd_count)++;

	printf("%d", *fd_count);
	//convert the ip address from network to presentation format
        inet_ntop(their_addr.ss_family, get_addr((struct sockaddr *) &their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);
}

void decode_body(char *body) {
        int i, j = 0;
        char c;

        for (i = 0; body[i] != '\0'; ++i) {
                if (body[i] == '+') {
                        body[j++] = ' ';
                }
                else if (body[i] == '%' && isxdigit(body[i + 1]) && isxdigit(body[i + 2])) {
                        sscanf(&body[i + 1], "%2x", (unsigned int*)&c);
                        body[j++] = c;
                        i += 2;
                }
                else {
                        body[j++] = body[i];
                }
        }

        body[j] = '\0';
}

void getfileurl(char *route, char* fileurl) {
	char* question = strrchr(route, '?');

	if(question)
		*question = '\0';
	if(route[strlen(route) - 1] == '/')
		strcpy(route, "/index.html");

	strcpy(fileurl, "");
	strcat(fileurl, route);

	const char *dot = strrchr(fileurl, '.');
  	if (!dot || dot == fileurl) {
    		strcat(fileurl, ".html");
  	}
}

//get the mime type of the file
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

void handle_getreq(SSL* ssl, char req[]) {
	char resheader[MAX];
	memset(resheader, '\0', sizeof(resheader) - 1);
	char method[10], route[MAX];
	char fileurl[MAX];
	char mimetype[32];
	char timebuf[MAX];
	int headersize;

	sscanf(req, "%s %s", method, route);

	printf("method: %s route: %s\n", method, route);

	getfileurl(route, fileurl);

	printf("File URL: %s\n", fileurl);

	FILE* file = fopen(fileurl + 1, "r");

	if(!file) {
		char response[MAX];
		sprintf(response, "HTTP/1.1 404 Not Found\r\n");
		if(SSL_write(ssl, response, sizeof(response)) <= 0)
			printf("send: ");
		return;
	}

	getmimetype(fileurl, mimetype);
	printf("%s\n", mimetype);

	time_t t;
	time(&t);
	strcpy(timebuf, ctime(&t));

	sprintf(resheader, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n\r\n",  mimetype, 1024);

	headersize = strlen(resheader);

	fseek(file, 0, SEEK_END);
	long fsize = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *resbuffer = (char*)malloc(MAX);
	strcpy(resbuffer, resheader);
	char *filebuffer = resbuffer + headersize;
	fread(filebuffer, fsize, 1, file);
	
	int n;
	printf("%s\n",resbuffer);
	if((n = SSL_write(ssl, resbuffer, MAX - 1)) <= 0) {
		if(n == 0) {
			printf("\nClient closed connection\n");
                	SSL_free(ssl);
                	return;
		}
		if(n < 0) {
                	perror("SSL write: ");
                	SSL_free(ssl);
       		        exit(1);
        	}
	}
	
	free(resbuffer);
	fclose(file);
}

//handle post requests
void handle_postreq(SSL* ssl, char *body) {
	char response[MAX];
	memset(response,'\0', sizeof(response) - 1);
	printf("%s", body);
	sprintf(response, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello, this is a POST response. Received data: %s", body);
	int n;
	if((n = (SSL_write(ssl, response, MAX-1))) <= 0) {
                if(n == 0) {
			printf("\nClient closed connection\n");
                        SSL_free(ssl);
                        return;
		}
		if(n < 0) {
                        perror("SSL write: ");
                        SSL_free(ssl);
                        exit(1);
                }
	}
}

//handle requests on new socket descriptor
void handle_requests(SSL* ssl, struct pollfd *pfds) {
	int n = 0;
	char buf[MAX];

	if((n = SSL_read(ssl, buf, MAX - 1)) == 0) {
		printf("\nClient closed connection\n");
		SSL_free(ssl);
		close(pfds -> fd);
		pfds -> fd *= -1;
		return;
	}

	if(n < 0) {
		perror("SSL read: ");
		SSL_free(ssl);
                close(pfds -> fd);
                pfds -> fd *= -1;
		exit(1);
	}

	buf[n] = '\0';

	printf("%s", buf);

	//check whether a get or post request
        if(strstr(buf, "GET") != NULL)
		handle_getreq(ssl, buf);
	else if(strstr(buf, "POST") != NULL) {
		char *body = strstr(buf, "\r\n\r\n");
		decode_body(body);
		if(body != NULL) {
			body += 4;
			handle_postreq(ssl, body);
		}
	}

	SSL_free(ssl);
	close(pfds->fd);
	pfds->fd *= -1;

	return;
}

int main() {
	int sockfd, newfd;
	nfds_t nfds = 0;
	int fd_size = 5;
	int fd_count = 0;

	sockfd = create_serversocket();			//create socket and bind it to the server's address and port
	server_listen(sockfd);				//make the server listen for incoming connections

	struct pollfd *pfds = malloc(sizeof *pfds * fd_size);

	SSL_CTX *context = create_SSL_context();
	SSL** sslfds = malloc(sizeof(SSL*) * fd_size);

	pfds[0].fd = sockfd;
	pfds[0].events = POLLIN;
	pfds[0].revents = 0;

	fd_count = 1;

	printf("server: waiting for connections...\n");

	while(1) {
		nfds = fd_count;

		if((poll(pfds, nfds, -1)) == -1) {
			perror("poll");
			exit(1);
		}

		for(int i = 0; i < nfds; i++) {
			if(pfds[i].fd <= 0)
				continue;
			if((pfds[i].revents && POLLIN) == POLLIN) {
				if(pfds[i].fd == sockfd) {
					server_accept(sockfd, &pfds, &fd_size, &fd_count, &sslfds, context);
					printf("Proxy connected\n");
				}
				else {
					handle_requests(sslfds[i], pfds + i);
				}
			}
		}
	}	

	SSL_CTX_free(context);
	close(sockfd);

	return 0;
}	
