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
#include<openssl/ssl.h>
#include<openssl/err.h>

#define BACKLOG 10 	//pending connections the queue can hold
#define MAX 1024	   	//maximum buffer size
#define PROXY_PORT "8000"

SSL_CTX *ctx1;
SSL_CTX *ctx2;

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

void create_ssl_context1() {
	SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();

        ctx1 = SSL_CTX_new(TLS_server_method());

        if(ctx1 == NULL) {
                ERR_print_errors_fp(stderr);
                exit(1);
        }

	if(SSL_CTX_use_certificate_file(ctx1, "server.crt", SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
        	exit(EXIT_FAILURE);
	}	

	if (SSL_CTX_use_PrivateKey_file(ctx1, "server.key", SSL_FILETYPE_PEM) <= 0) {
        	ERR_print_errors_fp(stderr);
        	exit(EXIT_FAILURE);
    	}
}

void create_ssl_context2() {
	SSL_library_init();
        SSL_load_error_strings();

        ctx2 = SSL_CTX_new(TLS_client_method());

        if(ctx2 == NULL) {
                ERR_print_errors_fp(stderr);
                exit(1);
        }
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
int proxy_accept(int sockfd) {
	int newfd;				//new socket descriptor after accepting the connection request
	struct sockaddr_storage their_addr;	//connector's address information
        socklen_t sin_size;			//size of struct sockaddr
	char s[INET6_ADDRSTRLEN];		//used to hold the ip address in presentation format
	sin_size = sizeof(their_addr);

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
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof(hints));		//make the struct initially empty
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

void handle_message(SSL* ssl1, SSL* ssl2, int clientfd, int serverfd) {
	char buf[1024];
	ssize_t n;

	if((n = SSL_read(ssl1, buf, sizeof(buf))) <= 0)
		return;

	buf[n] = '\0';
//	printf("%s\n", buf);

	SSL_write(ssl2, buf, n);

	while((n = SSL_read(ssl2, buf, sizeof(buf))) > 0)
		SSL_write(ssl1, buf, n);
}

void handle_message_http(int clientfd, int serverfd, char data[]) {
	ssize_t n;
	n = write(serverfd, data, 1024);

	while((n = recv(serverfd, data, 1024, 0)) > 0) {
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
		char *port_start = strstr(host, ":");
		char *port;
		char https_port[10] = "443";

		if(port_start != NULL) {
			*port_start = '\0';
			port = port_start + 1;
		}
		else {
			port = https_port;
		}

		if((serverfd = create_serversocket(host, port) == -1)) {
			perror("socket: ");
			close(newfd);
			exit(1);
		}

		printf("Host: %s Port: %s\n", host, port);
		char *response = "HTTP/1.1 200 Connection established\r\n\r\n";
		write(newfd, response, strlen(response));
		int i;
		SSL *ssl2 = SSL_new(ctx2);
		SSL_set_fd(ssl2, serverfd);
		printf("hello world\n");
		int j;
		if((j = SSL_connect(ssl2)) <= 0) {
			printf("%d\n", j);
			ERR_print_errors_fp(stderr);
			close(newfd);
			exit(1);
		}

		 printf("Enter something: \n");
                scanf("%d", &i);
		
		SSL* ssl1 = SSL_new(ctx1);
		SSL_set_fd(ssl1, newfd);

		if(SSL_accept(ssl1) <= 0) {
			ERR_print_errors_fp(stderr);
		 	SSL_free(ssl2);
			close(newfd);
		 	close(serverfd);
		 	return;
		}

		printf("hf\n");
		handle_message(ssl1, ssl2, newfd, serverfd);

		SSL_free(ssl2);
		close(serverfd);
		SSL_shutdown(ssl1);
		SSL_free(ssl1);
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

	create_ssl_context1();
	create_ssl_context2();

	proxyfd = create_proxysocket();		//create socket and bind it to the server's address and port

	proxy_listen(proxyfd);			//make the server listen for incoming connections

//	signal_handler();			//kill dead processes

	while(1) {
		newfd = proxy_accept(proxyfd);

	        if(newfd == -1)
			continue;

		if(!fork()) {			//for child processes
			int i;		
			printf("jj\n");	
			scanf("%d", &i);
			printf("#*!%d\n", i);			
			//close(proxyfd);		//listener not needed for child processes

			connectServer(newfd);

			close(newfd);
			exit(0);
		}
		close(newfd);			//parent doesn't need this
	}
	SSL_CTX_free(ctx1);
	close(proxyfd);
	return 0;
}
