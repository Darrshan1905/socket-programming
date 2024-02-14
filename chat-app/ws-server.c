#include<stdio.h>
#include<string.h>
#include<libwebsockets.h>
#include<pthread.h>

#define PORT 5000
#define MAX_CLIENTS 100

struct lws *client_list[MAX_CLIENTS];
char *username_list[MAX_CLIENTS];

void send_pvt_msg(char *sender, char *recvr, char *msg) {
	for(int i = 0; i < MAX_CLIENTS; ++i) {
		if(client_list[i] && strcmp(username_list[i], recvr) == 0) {
			lws_write(client_list[i], msg, strlen(msg), LWS_WRITE_TEXT);
			break;
		}
	}
}

void send_active_list(struct lws *wsi) {
	char buff[2048] = "</br>---------ACTIVE USERS LIST---------</br>";

	for(int i = 0; i < MAX_CLIENTS; i++) {
                if(client_list[i] && client_list[i] != wsi) {
                        strcat(buff, username_list[i]);
                        strcat(buff, "</br>");
                }
        }

	lws_write(wsi, buff, strlen(buff), LWS_WRITE_TEXT);
}

void send_message(char *sender, char *buff) {
	for (int i = 0; i < MAX_CLIENTS; ++i) {
        	if (client_list[i] && strcmp(username_list[i], sender) != 0) {
                	lws_write(client_list[i], buff, strlen(buff), LWS_WRITE_TEXT);
                }
        }
}

int callback_echo(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    	switch (reason)
    	{
        	case LWS_CALLBACK_ESTABLISHED:
		{
			char buff[2048];
			int rv = lws_hdr_copy(wsi, buff, sizeof(buff), WSI_TOKEN_HTTP_URI_ARGS);
			
			char *username_start = strstr(buff, "username=");
			username_start += strlen("username=");

			char *username_end = strchr(username_start, '&');
			if(!username_end)
				username_end = strchr(username_start, '\0');
			
			int username_length = username_end - username_start;
			
			char *username = malloc(username_length);
			strncpy(username, username_start, username_length);
			username[username_length] = '\0';

			printf("%s joined the chat\n", username);
			
			if(username) {				
				// Add client to the client list
                       		for (int i = 0; i < MAX_CLIENTS; ++i) {
                               		if (!client_list[i]) {
                       	        	        client_list[i] = wsi;
						username_list[i] = username;
               	                	        break;
					}
                        	}
			}
            		break;
		}

        	case LWS_CALLBACK_RECEIVE:
		{
			char sender[30];
			char *buff = (char*)in;
           		 // Forward the received message to all other clients
			for (int i = 0; i < MAX_CLIENTS; ++i) {
                                if (client_list[i] && client_list[i] == wsi) {
                                        strcpy(sender, username_list[i]);
                                }
                        }

			char *to;
                        if(to = strstr(buff, "To:")) {
				char msg[30];
                                printf("%s -> %s\n", sender, buff);
                                char *sc = strstr(to + 4, ":");
                                *sc = '\0';
				sprintf(msg, "%s -> %s", sender, sc + 1);
                                send_pvt_msg(sender, to + 4, msg);
                         }
                         else if(strcmp(buff, "online") == 0) {
				printf("%s -> %s\n",sender, buff);
                                send_active_list(wsi);
                         }
                         else {
				char msg[30];
				sprintf(msg, "%s -> %s", sender, buff);
                                send_message(sender, msg);
                                printf("%s -> %s\n", sender, buff);
                         }
            		break;
		}

        	case LWS_CALLBACK_CLOSED:
            		printf("Client disconnected\n");

            		// Remove client from the client list
            		for (int i = 0; i < MAX_CLIENTS; ++i) {
                		if (client_list[i] == wsi) {
                    			client_list[i] = NULL;
                    			break;
                		}
            		}
            		break;

        	default:
			break;
    	}

    	return 0;
}

static struct lws_protocols protocols[] = {
    	{
        	"echo-protocol",
        	callback_echo,
        	sizeof(char[30]),
        	100, // Maximum number of bytes for the rx buffer
    	},
    	{ NULL, NULL, 0, 0 } /* terminator */
};

int main()
{
    	struct lws_context_creation_info info;
    	struct lws_context *context;

    	memset(&info, 0, sizeof(info));

    	info.port = PORT;
    	info.protocols = protocols;

    	context = lws_create_context(&info);
    	if (!context)
    	{
        	fprintf(stderr, "libwebsocket init failed\n");
        	return -1;
    	}

    	printf("WebSocket server started. Listening on port %d\n", PORT);

    	while (1)
    	{
        	lws_service(context, 50);
    	}

    	lws_context_destroy(context);

    	return 0;
}

