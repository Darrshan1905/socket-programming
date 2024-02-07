#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include<netdb.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAXBUF 2048

volatile sig_atomic_t f = 0;
int sockfd = 0;
char name[32];

void str_overwrite_stdout() {
	printf("%s", "> ");
	fflush(stdout);
}

void str_trim_lf(char *buff, int len) {
	int i;

	for(i = 0; i < len; i++) {
		if(buff[i] == '\n') {
			buff[i] = '\0';
			break;
		}
	}
}

void catch_ctrl_c(int signal) {
	f = 1;
}

//get the sock address and return in network representation
void *get_addr(struct sockaddr *sa) {
	if(sa -> sa_family == AF_INET)
		return &(((struct sockaddr_in*)sa) -> sin_addr);	//for ipv4
	return &(((struct sockaddr_in6*)sa) -> sin6_addr);		//for ipv6
}

int create_clientsocket(char *ip, char* port) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	char s[INET6_ADDRSTRLEN];
	int rv;

	memset(&hints, 0, sizeof hints);		//make the struct initially empty
        hints.ai_family = AF_UNSPEC;			//can be either ipv4 or ipv6
        hints.ai_socktype = SOCK_STREAM;		//TCP stream socket

	//gives a pointer to a linked list, servinfo of results
        if((rv = getaddrinfo(ip, port, &hints, &servinfo)) != 0) {
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

void send_msg_handler() {
	char msg[MAXBUF] = {};
	char buff[MAXBUF + 32] = {};

	while(1) {
		str_overwrite_stdout();
		fgets(msg, MAXBUF, stdin);
		str_trim_lf(msg, strlen(msg));

		char *to;
		
		if(strcmp(msg, "exit") == 0) {
			break;
		}
		else if(strcmp(msg, "online") == 0) {
			send(sockfd, msg, strlen(msg), 0);
		}
		else {
			if(to = strstr(msg, "To:")) {
                        	char *sc = strstr(to + 4, ":");
                        	*sc = '\0';
				sprintf(buff, "To: %s:%s", to + 4, sc + 1);
				send(sockfd, buff, strlen(buff), 0);
	                }
			else {
				sprintf(buff, "%s : %s", name, msg);
				send(sockfd, buff, strlen(buff), 0);
		
			}
		}
		bzero(msg, sizeof(msg));
		bzero(buff, sizeof(buff));
	}

	catch_ctrl_c(2);
}

void recv_msg_handler() {
	char msg[MAXBUF] = {};

	while(1) {
		int rv;
		if((rv = recv(sockfd, msg, sizeof(msg), 0)) > 0) {
			printf("%s\n", msg);
			str_overwrite_stdout();
		}
		else if(rv == 0)
			break;
		else {
			perror("recv: ");
			exit(1);
		}

		memset(msg, 0, sizeof(msg));

	}
}

int main(int argc, char* argv[]) {
	if(argc != 3) {
		printf("Usage: %s <server ip> <server port>", argv[0]);
		exit(1);
	}

	char *ip, *port;

	ip = argv[1];
	port = argv[2];
	
	signal(SIGINT, catch_ctrl_c);

	printf("Enter your name: ");
	fgets(name, 30, stdin);
	str_trim_lf(name, strlen(name));

	if(strlen(name) > 32 || strlen(name) < 2) {
		printf("Name length must be between 2 and 30 characters\n");
		exit(1);
	}

	sockfd = create_clientsocket(ip, port);

        send(sockfd, name, 30, 0);

	printf("1.Type something to chat with other clients\n2.To chat with a specific client: To: <username>:<message>\n3.Type \"online\" to get list of active users\n4.Type \"exit\" or ctrl + c to leave chat\n\n");

	pthread_t send_thread;
	if(pthread_create(&send_thread, NULL, (void*)send_msg_handler, NULL) != 0) {
		printf("ERROR: pthread\n");
		exit(1);	
	}

	pthread_t recv_thread;
	if(pthread_create(&recv_thread, NULL, (void*)recv_msg_handler, NULL) != 0) {
		printf("ERROR: pthread\n");
		exit(1);
	}

	while(1) {
		if(f) {
			printf("Left chat\n");
			break;
		}
	}

	close(sockfd);

	return 1;
}
