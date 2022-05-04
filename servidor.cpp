#include <iostream>
#include <stdio.h>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/syscall.h>
#include <pthread.h>
#include <condition_variable>
#include <map>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "usuario.h"
#include <time.h>
#include "json.hpp"

#define BUFFER_SIZE 8192
#define DEFAULT_SENDER "Server"
#define HTTP_OK 200
#define HTTP_INTERNAL_ERROR 500
#define MAX_INACTIVE_TIME 30 //sec

using namespace std;
using json = nlohmann::json;

const char* default_ip = "172.31.41.2";
map<string, User *> users = {};
vector<vector<string>> general_chat;

void activity_check() {
    time_t server_time;
    time(&server_time);
    
    map<string, User *>::iterator itr;
    for(itr = users.begin(); itr != users.end(); ++itr) {
        string str_id = itr->first;
        User *u = itr->second;

        if(difftime(server_time, u->last_activity) > MAX_INACTIVE_TIME && u->status != BUSY ) {
            u->set_status("INACTIVO");
            printf("Setting to INACTIVE status user %s expiration time reached\n", u->username.c_str());
        }

        // if(difftime(server_time, u->last_activity) < MAX_INACTIVE_TIME ) {
        //     u->set_status("ACTIVO");
        //     printf("Checked: %s ACTIVE status user\n", u->username.c_str());

        // }
        if(difftime(server_time, u->last_activity) < MAX_INACTIVE_TIME && u->status != BUSY) {
            u->set_status("ACTIVO");
            printf("Updating to ACTIVE status user: %s activity detected\n", u->username.c_str());

        }
    }
}


void *handle_client_connected(void *params)
{
    char buffer[BUFFER_SIZE];
    User current_user;
    User *new_user = (User *)params;
    int own_socket = new_user->socket;
    User theUser = *new_user;
    int user_connected = 1;

    while (user_connected) {
        int listener = recv(own_socket, buffer, BUFFER_SIZE, 0);
        if (listener > 0) {
            json request;
            json response;
            string message_serialized;

            request = json::parse(buffer);
            cout<<"REQUEST: "<<request<<endl;
            string request_option = request["request"];
            int option = 0;
            if (request_option.compare("INIT_CONEX") == 0) {

                if (users.count(request["body"][1]) == 0) {
                    time_t curr_time;
                    current_user.username = request["body"][1];
                    current_user.socket = own_socket;
                    current_user.status = ACTIVE;
                    current_user.update_last_activity_time();

                    strcpy(current_user.ip_address, new_user->ip_address);
                    users[current_user.username] = &current_user;
                    
                    response["response"] = "INIT_CONEX";
                    response["code"] = HTTP_OK;
                } else {
                    response["response"] = "INIT_CONEX";
                    response["code"] = 105;
                }


                string parsed_response= response.dump();
                cout<<parsed_response<<endl;

                strcpy(buffer, parsed_response.c_str());
                send(own_socket, buffer, parsed_response.size() + 1, 0);
            }
            if (request_option.compare("POST_CHAT") == 0) {
                response["response"] = "POST_CHAT";
                response["code"] = HTTP_OK;

                string parsed_response = response.dump();
                cout<<parsed_response<<endl;

                strcpy(buffer, parsed_response.c_str());
                send(own_socket, buffer, parsed_response.size() + 1, 0);

                response.erase("response");
                response.erase("code");

                response["response"] = "NEW_MESSAGE";
                response["body"] = request["body"];
                parsed_response= response.dump();
                cout<<parsed_response<<endl;
                strcpy(buffer, parsed_response.c_str());
                string to_at = response["body"][3];
                vector<string> message;
                message.push_back(response["body"][0]);
                message.push_back(response["body"][1]);
                message.push_back(response["body"][2]);
                // se esta usando el mismo socket para todos los usuarios y esta malo eso.
                if (to_at.compare("all") == 0) {
                    general_chat.push_back(message);
                    map<string, User *>::iterator itr;
                    for (itr = users.begin(); itr != users.end(); ++itr)
                    {
                        User *u = itr->second;
                        // We manage the current user broadcast because we already know the message...
                        if (u->username.compare(current_user.username) != 0) {
                            send(u->socket, buffer, parsed_response.size() + 1, 0);
                        }
                    }
                } else {
                    User *user_message = users[to_at];
                    user_message->inbox_messages.push_back(message);
                    send(user_message->socket, buffer, parsed_response.size() + 1, 0);
                }
            }
            if (request_option.compare("PUT_STATUS") == 0) {
                string status_message = request["body"];
                int status = stoi(status_message);
                switch (status)
                {
                case 0:
                    current_user.status = ACTIVE;
                    break;
                case 1:
                    current_user.status = INACTIVE;
                    break;
                case 2:
                    current_user.status = BUSY;
                    break;
                default:
                    cout<<"no existe este status"<<endl;
                    break;
                }
                response["response"] = "PUT_STATUS";
                response["code"] = HTTP_OK;

                string parsed_response= response.dump();
                cout<<parsed_response<<endl;

                strcpy(buffer, parsed_response.c_str());
                send(own_socket, buffer, parsed_response.size() + 1, 0);
            }
            if (request_option.compare("GET_USER") == 0) {
                response["response"] = "GET_USER";
                response["code"] = HTTP_OK;
                string who = request["body"];

                if (who.compare("all") == 0) {
                    int user_size = users.size();
                    vector<vector<string>> user_list; 
                    //string user_list[user_size][2];
                    map<string, User *>::iterator itr;
                    for (itr = users.begin(); itr != users.end(); ++itr)
                    {
                        User *u = itr->second;
                        vector<string> vector;
                        vector.push_back(u->username);
                        vector.push_back(u->get_status());
                        user_list.push_back(vector);
                    }
                    response["body"] = user_list;
                }
                else {
                    User *user_status = users[who];
                    response["body"] = {user_status->ip_address, user_status->get_status()};
                }

                string parsed_response = response.dump();
                cout<<parsed_response<<endl;
                strcpy(buffer, parsed_response.c_str());
                send(own_socket, buffer, parsed_response.size() + 1, 0);
                
            }
            if (request_option.compare("GET_CHAT") == 0) {
                response["response"] = "GET_CHAT";
                response["code"] = HTTP_OK;

                string chat_identifier = request["body"];
                if (chat_identifier.compare("all") == 0) {
                    response["body"] = general_chat;
                } else {
                    User *user = users[chat_identifier];
                    response["body"] = user->inbox_messages;
                }

                string parsed_response = response.dump();
                cout<<parsed_response<<endl;
                strcpy(buffer, parsed_response.c_str());
                send(own_socket, buffer, parsed_response.size() + 1, 0);
            }
            if (request_option.compare("END_CONEX") == 0) {
                response["response"] = "END_CONEX";
                response["code"] = HTTP_OK;

                string parsed_response = response.dump();
                cout<<parsed_response<<endl;
                strcpy(buffer, parsed_response.c_str());
                send(own_socket, buffer, parsed_response.size() + 1, 0);

                user_connected = 0;
            }
            memset(buffer, 0, BUFFER_SIZE);
        }
    }
    printf("Disconnecting user %s from server...", current_user.username.c_str());
    users.erase(current_user.username);
    close(own_socket);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        exit(1);
    }


    int port = atoi(argv[1]);

    sockaddr_in server_addr;
    sockaddr_in client_addr;
    socklen_t client_size;
    int socket_fd = 0, conn_fd = 0;

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(default_ip);
    server_addr.sin_port = htons(port);
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (socket_fd < 0)
    {
        fprintf(stderr, "Error creating socket...\n");
        return EXIT_FAILURE;
    }

    if (bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        fprintf(stderr, "Error on binding IP...\n");
        return EXIT_FAILURE;
    }

    if (listen(socket_fd, 5) < 0)
    {
        fprintf(stderr, "Socket listening failed...\n");
        return EXIT_FAILURE;
    }
    printf("SERVER RUNNING ON PORT %d\n", port);
    while (true)
    {
        client_size = sizeof client_addr;
        conn_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &client_size);
        // TODO: Manage a limited number of clients connected...
        User new_user;
        new_user.socket = conn_fd;
        inet_ntop(AF_INET, &(client_addr.sin_addr), new_user.ip_address, INET_ADDRSTRLEN);

        // Threads
        pthread_t thread_id;
        pthread_attr_t attrs;

        pthread_attr_init(&attrs);
        pthread_create(&thread_id, &attrs, handle_client_connected, (void *)&new_user);

        // Reduce CPU usage
        sleep(1);
    }

    close(socket_fd);
    return EXIT_SUCCESS;
}
