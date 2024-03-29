#include <iostream>
using namespace std;
#include <string>
#include <vector>
#include <libwebsockets.h>

#define PORT 5000
#define MAX_CLIENTS 100

class Users {
    public:
        static vector<struct lws *> client_list;
        static vector<string> username_list;

	static void addClient(struct lws *wsi) {
		for (size_t i = 0; i < MAX_CLIENTS; ++i) {
                	if (!client_list[i]) {
                    		client_list[i] = wsi;
                    		break;
                	}
            	}	
	}

	static void send_joined_message(char *msg, struct lws *wsi) {
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_list[i] && wsi != client_list[i]) {
                    lws_write(client_list[i], (unsigned char*)msg, strlen(msg), LWS_WRITE_TEXT);
                }
            }
        }

	static void setOrUpdateName(struct lws *wsi, string name) {
		for (size_t i = 0; i < MAX_CLIENTS; i++) {
                    if (client_list[i] && client_list[i] == wsi) {
                        char msg[50];
                        username_list[i] = name;
                    }
                }
	}

        static void send_pvt_msg(const string sender, const string &recvr, char *msg) {
            for (int i = 0; i < MAX_CLIENTS; ++i) {
                if (client_list[i] && username_list[i] == recvr) {
                    lws_write(client_list[i], (unsigned char*)msg, strlen(msg), LWS_WRITE_TEXT);
                    break;
                }
            }
        }   

        static void send_active_list(struct lws *wsi, int option) {
            char buff[2048] = "------ACTIVE USERS LIST------</br>";

            for (size_t i = 0; i < MAX_CLIENTS; i++) {
                if (client_list[i] && client_list[i] != wsi) {
                    strcat(buff, (char *)username_list[i].c_str());
                    strcat(buff, "</br>");
                }
            }

            if (option) {
                strcat(buff, "option");
            }

            lws_write(wsi, (unsigned char*)buff, strlen(buff), LWS_WRITE_TEXT);
        }

        static void send_message(const string &sender, char *buff) {
            for (int i = 0; i < MAX_CLIENTS; ++i) {
                if (client_list[i] && username_list[i] != sender) {

                    lws_write(client_list[i], (unsigned char*)buff, strlen(buff), LWS_WRITE_TEXT);
                }
            }
        }

	static void removeClient(struct lws *wsi) {
	    for (size_t i = 0; i < MAX_CLIENTS; ++i) {
                if (client_list[i] == wsi) {
                    cout << username_list[i] << " left the chat\n\n";
                    client_list[i] = nullptr;
                    break;
               	}
            }
	}
};

vector<struct lws*> Users::client_list(MAX_CLIENTS);
vector<string> Users::username_list(MAX_CLIENTS);

class WebSocketServer {
public:
    WebSocketServer() : context(nullptr) {}

    void start() {
        struct lws_context_creation_info info;
        memset(&info, 0, sizeof(info));

        info.port = PORT;
        info.protocols = protocols;

        context = lws_create_context(&info);
        if (!context) {
            cerr << "libwebsocket init failed" << endl;
            return;
        }

        cout << "WebSocket server started. Listening on port " << PORT << endl;

        while (true) {
            lws_service(context, 50);
        }

        lws_context_destroy(context);
    }

private:
    struct lws_context *context;
    static struct lws_protocols protocols[];

    // Callback function
    static int callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

};

// Initialize static member
struct lws_protocols WebSocketServer::protocols[] = {
    {"echo-protocol", WebSocketServer::callback, sizeof(char[30]), 100}, // Maximum number of bytes for the rx buffer
    {NULL, NULL, 0, 0} /* terminator */
};

int WebSocketServer::callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED: {
            // Add client to the client_list
	    Users::addClient(wsi);
            break;
        }

        case LWS_CALLBACK_RECEIVE: {
            char sender[30];
            char *buff = (char *)in;

            buff[len] = '\0';

            cout << "buffer: " << buff << endl;

            char *name;
            if (name = strstr(buff, "Name: ")) {
		char msg[50];
		Users::setOrUpdateName(wsi, string(name + 6));
                sprintf(msg, "%s joined the chat\n", name + 6);
		Users::send_joined_message(msg, wsi);
            } else {
                for (size_t i = 0; i < MAX_CLIENTS; ++i) {
                    if (Users::client_list[i] && Users::client_list[i] == wsi) {
                        strcpy(sender, Users::username_list[i].c_str());
                    }
                }

                char *to;
                if (to = strstr(buff, "To:")) {
                    char msg[30];
                    cout << sender << " -> " << buff << endl;
                    char *sc = strstr(to + 4, ":");
                    *sc = '\0';
                    sprintf(msg, "%s -> %s", sender, sc + 1);
                    Users::send_pvt_msg(sender, to + 4, msg);
                } else if (strcmp(buff, "online") == 0) {
                    cout << sender << " -> " << buff << endl;
                    Users::send_active_list(wsi, 0);
                } else if (strcmp(buff, "online1") == 0) {
                    cout << sender << " -> " << buff << endl;
                    Users::send_active_list(wsi, 1);
                } else {
                    char msg[30];
                    sprintf(msg, "%s -> %s", sender, buff);
                    Users::send_message(sender, msg);
                    cout << sender << " -> " << buff << endl;
                }
            }

            break;
        }

        case LWS_CALLBACK_CLOSED:
            cout << "Client disconnected\n";

            // Remove client from the client list
	    Users::removeClient(wsi);
            break;

        default:
            break;
    }

    return 0;
}

int main() {
    WebSocketServer server;
    server.start();

    return 0;
}
