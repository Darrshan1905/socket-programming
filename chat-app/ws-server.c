#include<stdio.h>
#include<string.h>
#include<libwebsockets.h>

#define PORT 5000
#define MAX_CLIENTS 100

struct lws *client_list[MAX_CLIENTS];

int callback_echo(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    	switch (reason)
    	{
        	case LWS_CALLBACK_ESTABLISHED:
            		printf("Client connected\n");

            		// Add client to the client list
            		for (int i = 0; i < MAX_CLIENTS; ++i) {
                		if (!client_list[i]) {
                    			client_list[i] = wsi;
                    			break;
                		}
            		}
            		break;

        	case LWS_CALLBACK_RECEIVE:
           		 // Forward the received message to all other clients
            		for (int i = 0; i < MAX_CLIENTS; ++i) {
                		if (client_list[i] && client_list[i] != wsi) {
                    			lws_write(client_list[i], in, len, LWS_WRITE_TEXT);
                		}
            		}
            		break;

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
        	0,
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

