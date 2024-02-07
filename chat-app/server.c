#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<sys/socket.h>
#include<sys/types.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<netdb.h>
#include<errno.h>
#include<pthread.h>
#include<signal.h>

#define MAXCLIENTS 100
#define MAXBUF 2048
#define BACKLOG 10

static _Atomic unsigned int clients_count = 0;
static int uid = 10;

typedef struct{
	struct sockaddr_in address;
	int sockfd;
	int uid;
	char name[32];
} client_t;

client_t *clients[MAXCLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_trim_lf(char *arr, int len) {
	int i;
	
	for(i = 0; i < len; i++) {
		if(arr[i] == '\n') {
			arr[i] = '\0';
			break;
		}
	}
}

//get the socket address and return in network representation
void *get_addr(struct sockaddr *sa ) {
	if(sa -> sa_family == AF_INET)
		return &(((struct sockaddr_in*) sa) -> sin_addr);	//for ipv4
	return &(((struct sockaddr_in6*) sa) -> sin6_addr);		//for ipv6
}

int create_serversocket(char* port) {
	int sockfd;
	struct addrinfo hints, *servinfo, *p;
	int rv;
	int yes = 1;

	memset(&hints, 0, sizeof hints);	//make the struct initially empty
        hints.ai_family = AF_UNSPEC;		//can be either ipv4 or ipv6
        hints.ai_socktype = SOCK_STREAM;	//TCP stream socket
        hints.ai_flags = AI_PASSIVE;		//fill in the host machine's IP

	//gives a pointer to a linked list, servinfo of results
        if((rv = getaddrinfo(NULL, port, &hints, &servinfo))) {
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
		
		signal(SIGPIPE, SIG_IGN);

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

void queue_add(client_t *cli) {
	pthread_mutex_lock(&clients_mutex);

	for(int i = 0; i < MAXCLIENTS; i++) {
		if(!clients[i]) {
			clients[i] = cli;
			break;
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

void queue_remove(int uid) {
	pthread_mutex_lock(&clients_mutex);

	for(int i = 0; i < MAXCLIENTS; i++) {
		if(clients[i]) {
			if(clients[i] -> uid == uid) {
				clients[i] = NULL;
				break;
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

void send_message(char *buff, int uid) {
	pthread_mutex_lock(&clients_mutex);

	for(int i = 0; i < MAXCLIENTS; i++) {
		if(clients[i]) {
			if(clients[i] -> uid != uid) {
				if(write(clients[i] -> sockfd, buff, strlen(buff)) < 0) {
					perror("write: ");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}

void send_pvt_msg(char *sender, char *msg, char *recvr) {
	printf("1\n");
	pthread_mutex_lock(&clients_mutex);
	printf("1\n");
	for(int i = 0; i < MAXCLIENTS; i++) {
		if(clients[i]) {
			if(strcmp(clients[i] -> name, recvr) == 0) {
				char buff[MAXBUF + 32];
				sprintf(buff, "%s -> %s", sender, msg);
				printf("%s\n", buff);
				if(write(clients[i] -> sockfd, buff, strlen(buff)) < 0) {
					perror("write: ");
					break;
				}
			}
		}
	}

	pthread_mutex_unlock(&clients_mutex);
}	

void *handle_client(void *arg) {
	char buff[MAXBUF], name[30];
	int f = 0;

	clients_count++;
	client_t *cli = (client_t*) arg;

	if(recv(cli -> sockfd, name, 30, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 30) {
		printf("Name required.\n");
		f = 1;
	}
	else {
		strcpy(cli -> name, name);
		sprintf(buff, "%s joined the chat", cli -> name);
		printf("%s\n", buff);
		send_message(buff, cli -> uid);
	}

	bzero(buff, MAXBUF);

	while(1) {
		if(f) {
			break;
		}

		int rv;
		if((rv = recv(cli -> sockfd, buff, MAXBUF, 0)) > 0) {
			if(strlen(buff) > 0) {
				char *to;
				if(to = strstr(buff, "To:")) {
					str_trim_lf(buff, strlen(buff));
                                	printf("%s -> %s\n", cli -> name, buff);
					char *sc = strstr(to + 4, ":");
		                        *sc = '\0';
                		        send_pvt_msg(cli -> name, sc + 1, to + 4);
		                }
				else {
					send_message(buff, cli -> uid);
					str_trim_lf(buff, strlen(buff));
                                	printf("%s -> %s\n", cli -> name, buff);
				}
			}
		}
		else if(rv == 0 || strcmp(buff, "exit") == 0) {
                                sprintf(buff, "%s has left.", cli -> name);
                                printf("%s\n", buff);
                                send_message(buff, cli -> uid);
				f = 1;
		}
		else {
			printf("ERROR: recv: -1\n");
			f = 1;
		}

		bzero(buff, MAXBUF);
	}

	close(cli -> sockfd);
	queue_remove(cli -> uid);
	clients_count--;

	pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char* argv[]) {
	if(argc != 2) {
		printf("Usage: %s <port>", argv[0]);
		exit(1);
	}

	struct sockaddr_in theiraddr;
	pthread_t tid;
	int sockfd, newfd;
	char *port;

	port = argv[1];
	sockfd = create_serversocket(port);
	server_listen(sockfd);

	while(1) {
		char s[INET6_ADDRSTRLEN];
		socklen_t sin_size = sizeof(theiraddr);

		newfd = accept(sockfd, (struct sockaddr*)&theiraddr, &sin_size);

		//convert the ip address from network to presentation format
        	inet_ntop(theiraddr.sin_family, get_addr((struct sockaddr *) &theiraddr), s, sizeof s);
        	printf("server: got connection from %s\n", s);

		if(clients_count + 1 == MAXCLIENTS) {
			printf("Maximum clients reached. Rejected: %s:%d\n", s, theiraddr.sin_port);
			close(newfd);
			continue;
		}

		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli -> address = theiraddr;
		cli -> sockfd = newfd;
		cli -> uid = uid++;

		queue_add(cli);
		pthread_create(&tid, NULL, &handle_client, (void*)cli);

		sleep(1);
	}

	return 1;
}
