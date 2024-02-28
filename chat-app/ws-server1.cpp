#include <iostream>
#include <string>
#include <vector>
#include <libwebsockets.h>
#include <map>
using namespace std;

#define PORT 5000
#define MAX_CLIENTS 100

class WebSocketServer {
public:
    static WebSocketServer* ws;
    struct lws *lws_list[MAX_CLIENTS];
     
    WebSocketServer() : context(nullptr) {
     }

    void initializews(WebSocketServer* ws){
        for (size_t i = 0; i < MAX_CLIENTS; ++i) {
            lws_list[i] = nullptr;
        }
        WebSocketServer::ws = ws;
        start();
    }

    virtual void addClient(int index) = 0;
    virtual void handleMessage(int index, char *msg) = 0;
    virtual void removeClient(int index) = 0;

    void send_message(int index,char* msg){
        lws_write(lws_list[index], (unsigned char*)msg, strlen(msg), LWS_WRITE_TEXT);
    }

    int addLws(struct lws *wsi) {
        cout<<"Inside addLws"<<endl;
		for (size_t i = 0; i < MAX_CLIENTS; ++i) {
            if (!lws_list[i]) {
                lws_list[i] = wsi;
                return i;
            }
        }	
        return  -1;
	}

    int removeLws(struct lws *wsi) {
	    for (size_t i = 0; i < MAX_CLIENTS; ++i) {
            if (lws_list[i] == wsi) {
                lws_list[i] = nullptr;
                return i;
            }
        }
        return -1;
	}

private:
    struct lws_context *context;
    

    
    //callback function
    static int callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len) {
        switch (reason) {
            case LWS_CALLBACK_ESTABLISHED: {
                // Add client to the client_list
                cout<<"Client connected\n";
                int index = ws->addLws(wsi);
                if( index == -1){
                    cout<<"Max clients reached"<<endl;
                    return -1;
                }
                ws->addClient(index);
                break;
            }

            case LWS_CALLBACK_RECEIVE: {
                char *buff = (char *)in;
                buff[len] = '\0';
                cout << "buffer: " << buff << endl;

                for (size_t i = 0; i < MAX_CLIENTS; ++i) {
                    if (ws->lws_list[i] && ws->lws_list[i] == wsi) {
                        ws->handleMessage(i, buff);
                    }
                }
                break;
            }

            case LWS_CALLBACK_CLOSED: {
                int index = ws->removeLws(wsi);
                cout << "Client disconnected\n";

                // Remove client from the client list
                ws->removeClient(index);
                break;
            }

            default:
                break;
        }
        return 0;
    }

    void start() {
        struct lws_context_creation_info info;
        memset(&info, 0, sizeof(info));
        struct lws_protocols protocols[] = {
            {"echo-protocol", callback, sizeof(char[30]), 100}, // Maximum number of bytes for the rx buffer
            {NULL, NULL, 0, 0} /* terminator */
        };

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
};

WebSocketServer* WebSocketServer::ws = nullptr;

class UserDetail{
    char name[100];
    
    public:
    void setName(char arr[]){
        strcpy(name,arr);
    }

    void removeName(){
        strcpy(name,"");
    }

    char* getName(){
        return name;
    }
};

class ChatServer : public WebSocketServer{
    public:
        void startServer(){
            WebSocketServer *ws = this;
            ws->initializews(ws);
        }
    
    map<int,UserDetail> userDetails;

    void addClient(int index){
        if(userDetails.find(index) == userDetails.end()){
            char name[100];
            sprintf(name,"user%d",index);
            userDetails[index].setName(name);
        }
    }

    void removeClient(int index){
        if(userDetails.find(index) != userDetails.end()){
            userDetails.erase(index);
        }
    }

    void handleMessage(int index, char *msg){
        char sender[30];

        char *name;
        if (name = strstr(msg, "Name: ")) {
            char buff[50];
            setOrUpdateName(index, name + 6);
            sprintf(buff, "%s joined the chat\n", name + 6);
            sendBroadcastMsg(index, buff);
        } else {
            
            strcpy(sender, userDetails[index].getName());
            char *to;

            if (to = strstr(msg, "To:")) {
                char buff[30];
                cout << sender << " -> " << msg << endl;
                char *sc = strstr(to + 4, ":");
                *sc = '\0';
                sprintf(buff, "%s -> %s", sender, sc + 1);
                sendPrivateMsg(index, to + 4, buff);
            } else if (strcmp(msg, "online") == 0) {
                cout << sender << " -> " << msg << endl;
                sendActiveList(index, 0);
            } else if (strcmp(msg, "online1") == 0) {
                cout << sender << " -> " << msg << endl;
                sendActiveList(index, 1);
            } else {
                char buff[30];
                sprintf(buff, "%s -> %s", sender, msg);
                sendBroadcastMsg(index, buff);
                cout << sender << " -> " << msg << endl;
            }
        }
    }

    void setOrUpdateName(int index, char *name) {
        userDetails[index].setName(name);
    }

    void sendPrivateMsg(int index,char* recvr, char *msg) {
        int recvrIndex = -1;

        for(auto userDetail : userDetails) {
            if(strcmp(userDetail.second.getName(), recvr) == 0) {
                recvrIndex = userDetail.first;
                break;
            }
        }

        send_message(recvrIndex, msg);
    }

    void sendBroadcastMsg(int index, char *msg) {
        for(auto userDetail : userDetails){
            if(userDetail.first != index) {
                send_message(userDetail.first, msg);
            }
        }
    }

    void sendActiveList(int index, int option) {
        char buff[2048] = "------ACTIVE USERS LIST------</br>";

        for (auto userDetail : userDetails) {
            if (userDetail.first != index) {
                strcat(buff, userDetail.second.getName());
                strcat(buff, "</br>");
            }
        }

        if (option) {
            strcat(buff, "option");
        }

        send_message(index, buff);
    }
};

int main() {
    ChatServer server;
    server.startServer();
    return 0;
}